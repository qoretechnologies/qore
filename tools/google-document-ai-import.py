#!/usr/bin/env python3
"""Reproduce the reviewed Document AI resources from vendored Discovery bytes.

Copyright 2026 Qore Technologies, s.r.o.; MIT license (see COPYING.MIT).
This offline importer owns Document AI decisions; google-discovery-openapi.py
owns the reusable normalization boundary. Run with --check to verify artifacts.
"""

import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "qlib/GoogleDocumentAiDataProvider"
spec = importlib.util.spec_from_file_location("discovery", Path(__file__).with_name("google-discovery-openapi.py"))
discovery = importlib.util.module_from_spec(spec)
spec.loader.exec_module(discovery)

# Reviewed identities, independent of dynamic schema/type construction. A leading
# underscore denotes the two native contracts used by the compatible process helper.
# API, method suffix, action, display name, short description.
OPERATIONS = [
    ("v1", "locations.list", "list-locations", "List Locations", "List locations available to this project"),
    ("v1", "locations.get", "get-location", "Get Location", "Read information about the connection's location"),
    ("v1", "locations.processorTypes.list", "list-processor-types", "List Processor Types", "List processor types available in this location"),
    ("v1", "locations.processorTypes.get", "get-processor-type", "Get Processor Type", "Read a processor type and its supported capabilities"),
    ("v1", "locations.processors.list", "list-processors", "List Processors", "List processors in this project and location"),
    ("v1", "locations.processors.get", "get-processor", "Get Processor", "Read a processor's configuration and state"),
    ("v1", "locations.processors.create", "create-processor", "Create Processor", "Create a processor of an available type"),
    ("v1", "locations.processors.delete", "delete-processor", "Delete Processor", "Start deleting a processor and all its versions"),
    ("v1", "locations.processors.enable", "enable-processor", "Enable Processor", "Start enabling a processor for document processing"),
    ("v1", "locations.processors.disable", "disable-processor", "Disable Processor", "Start disabling a processor"),
    ("v1", "locations.processors.setDefaultProcessorVersion", "set-default-version", "Set Default Version", "Select the version used when processing without a version ID"),
    ("v1", "locations.processors.process", "_process", "Process Document Contract", "Process a document with the default processor version"),
    ("v1", "locations.processors.processorVersions.process", "_process-version", "Process Version Contract", "Process a document with a selected processor version"),
    ("v1", "locations.processors.batchProcess", "start-batch-processing", "Start Batch Processing", "Submit documents in Cloud Storage for batch processing"),
    ("v1", "locations.processors.processorVersions.batchProcess", "start-version-batch-processing", "Start Version Batch Processing", "Submit a batch using a selected processor version"),
    ("v1", "locations.processors.processorVersions.list", "list-processor-versions", "List Processor Versions", "List the versions belonging to a processor"),
    ("v1", "locations.processors.processorVersions.get", "get-processor-version", "Get Processor Version", "Read a processor version and its document schema"),
    ("v1", "locations.processors.processorVersions.delete", "delete-processor-version", "Delete Processor Version", "Start deleting a processor version"),
    ("v1", "locations.processors.processorVersions.train", "train-processor-version", "Train Processor Version", "Start training a version from a processor's training dataset"),
    ("v1", "locations.processors.processorVersions.deploy", "deploy-processor-version", "Deploy Processor Version", "Start deploying a version for document processing"),
    ("v1", "locations.processors.processorVersions.undeploy", "undeploy-processor-version", "Undeploy Processor Version", "Start removing a processor version from serving"),
    ("v1", "locations.processors.processorVersions.evaluateProcessorVersion", "evaluate-processor-version", "Evaluate Processor Version", "Start an evaluation using labeled documents"),
    ("v1", "locations.processors.processorVersions.evaluations.list", "list-evaluations", "List Evaluations", "List evaluation results for a processor version"),
    ("v1", "locations.processors.processorVersions.evaluations.get", "get-evaluation", "Get Evaluation", "Read extraction quality metrics from an evaluation"),
    ("v1", "locations.operations.list", "list-operations", "List Operations", "List processing and management jobs using an operation filter"),
    ("v1", "locations.operations.get", "get-operation", "Get Operation", "Read a job's status, metadata, result, and error"),
    ("v1", "locations.operations.cancel", "cancel-operation", "Cancel Operation", "Request cancellation of a running job"),
    ("v1", "locations.schemas.list", "list-schemas", "List Schemas", "List document schemas in this project and location"),
    ("v1", "locations.schemas.get", "get-schema", "Get Schema", "Read a document schema's configuration"),
    ("v1", "locations.schemas.create", "create-schema", "Create Schema", "Create a named collection of document schema versions"),
    ("v1", "locations.schemas.patch", "update-schema", "Update Schema", "Update selected fields of a document schema"),
    ("v1", "locations.schemas.delete", "delete-schema", "Delete Schema", "Delete a document schema"),
    ("v1", "locations.schemas.schemaVersions.list", "list-schema-versions", "List Schema Versions", "List versions belonging to a document schema"),
    ("v1", "locations.schemas.schemaVersions.get", "get-schema-version", "Get Schema Version", "Read a schema version and its extraction fields"),
    ("v1", "locations.schemas.schemaVersions.create", "create-schema-version", "Create Schema Version", "Create a version of a document schema"),
    ("v1", "locations.schemas.schemaVersions.patch", "update-schema-version", "Update Schema Version", "Update selected fields of a schema version"),
    ("v1", "locations.schemas.schemaVersions.delete", "delete-schema-version", "Delete Schema Version", "Delete a document schema version"),
    ("v1", "locations.schemas.schemaVersions.generate", "generate-schema-version", "Generate Schema Version", "Generate a schema version from example documents"),
    ("v1beta3", "locations.processors.updateDataset", "beta-update-dataset", "Update Dataset (Beta)", "Update a processor's dataset storage configuration"),
    ("v1beta3", "locations.processors.dataset.getDatasetSchema", "beta-get-dataset-schema", "Get Dataset Schema (Beta)", "Read the labeling schema of a processor's dataset"),
    ("v1beta3", "locations.processors.dataset.updateDatasetSchema", "beta-update-dataset-schema", "Update Dataset Schema (Beta)", "Update the labeling schema of a processor's dataset"),
    ("v1beta3", "locations.processors.dataset.getDocument", "beta-get-dataset-document", "Get Dataset Document (Beta)", "Read a selected dataset document and its annotations"),
    ("v1beta3", "locations.processors.dataset.listDocuments", "beta-list-dataset-documents", "List Dataset Documents (Beta)", "List documents in a selected processor's dataset"),
    ("v1beta3", "locations.processors.dataset.importDocuments", "beta-import-dataset-documents", "Import Dataset Documents (Beta)", "Start importing labeled documents from Cloud Storage"),
    ("v1beta3", "locations.processors.dataset.batchDeleteDocuments", "beta-delete-dataset-documents", "Delete Dataset Documents (Beta)", "Start deleting explicitly selected dataset documents"),
    ("v1beta3", "locations.processors.processorVersions.importProcessorVersion", "beta-import-processor-version", "Import Processor Version (Beta)", "Start importing a processor version from another processor"),
]

PATH_FIELDS = {
    "projectsId": ("project_id", "Project ID", None),
    "locationsId": ("location", "Location", None),
    "processorsId": ("processor_id", "Processor ID", "processors"),
    "processorVersionsId": ("processor_version_id", "Processor Version ID", "processor-versions"),
    "processorTypesId": ("processor_type", "Processor Type", "processor-types"),
    "operationsId": ("operation_id", "Operation ID", None),
    "evaluationsId": ("evaluation_id", "Evaluation ID", "evaluations"),
    "schemasId": ("schema_id", "Schema ID", "schemas"),
    "schemaVersionsId": ("schema_version_id", "Schema Version ID", "schema-versions"),
}


def manifest(version, declarations):
    result = []
    for api, suffix, action, title, summary in OPERATIONS:
        if api != version:
            continue
        method = declarations["documentai.projects." + suffix]
        fields = {}
        for wire in re.findall(r"\{([^}]+)\}", method["flatPath"]):
            name, label, reference = PATH_FIELDS[wire]
            fields[wire] = {"name": name, "display_name": label, "preselected": True,
                            "desc": f"Identifier or full resource name for the selected {label.lower()}."}
            if reference:
                fields[wire].update(ref_data=reference, supports_custom_values=True)
                parents = {"processor-versions": ["processor_id"], "evaluations": ["processor_id", "processor_version_id"],
                           "schema-versions": ["schema_id"]}.get(reference)
                if parents:
                    fields[wire]["depends_on"] = parents
        request = method.get("request", {}).get("$ref")
        if action == "create-processor":
            fields.update({"type": {"display_name": "Processor Type", "required": True, "ref_data": "processor-types",
                                     "supports_custom_values": True},
                           "displayName": {"display_name": "Display Name", "required": True}})
        if action == "set-default-version":
            fields["defaultProcessorVersion"] = {"display_name": "Default Processor Version", "ref_data": "processor-versions",
                                                  "depends_on": ["processor_id"], "supports_custom_values": True}
        if action.startswith("start-") and "batch" in action:
            fields["inputDocuments"] = {"required": True, "sensitive": True}
            fields["documentOutputConfig"] = {"required": True}
            fields["skipHumanReview"] = {"ignore": True}
        if action == "beta-update-dataset":
            fields["documentWarehouseConfig"] = {"ignore": True}
        if action == "list-operations":
            fields["filter"] = {"required": True, "preselected": True,
                                "example_value": 'Type="BATCH_PROCESS_DOCUMENTS"',
                                "desc": "Operation filter. A `Type` equality is required; for example `Type=\"BATCH_PROCESS_DOCUMENTS\"`."}
        if request and action.startswith("_"):
            fields["skipHumanReview"] = {"ignore": True}
        description = summary + ".\n\n" + method.get("description", "")
        if version != "v1":
            description += "\n\nUses the **v1beta3** API. Availability and permissions depend on the processor and location."
        if method.get("response", {}).get("$ref") == "GoogleLongrunningOperation":
            description += "\n\nReturns an operation immediately. Use **Get Operation** or **Wait for Operation** to observe completion."
        if action == "cancel-operation":
            description += "\n\nCancellation is asynchronous and may not be supported. The operation is retained; inspect its final status."
        group = "Processors"
        if suffix in ("locations.list", "locations.get"):
            group = "Locations"
        elif "schemas.schemaVersions" in suffix:
            group = "Schema Versions"
        elif "schemas." in suffix:
            group = "Document Schemas"
        elif "operations." in suffix:
            group = "Operations"
        elif "evaluations." in suffix or "evaluateProcessorVersion" in suffix:
            group = "Evaluations"
        elif suffix.endswith((".process", ".batchProcess")):
            group = "Document Extraction"
        elif "processorVersions." in suffix or action == "set-default-version":
            group = "Processor Versions"
        elif version != "v1":
            group = "Dataset (Beta)"
        result.append({"action": action, "method": method["httpMethod"], "path": "/" + method["flatPath"],
                       "operation_id": method["id"], "display_name": title, "short_desc": summary,
                       "desc": discovery.markdown_description(description), "fields": fields,
                       "groups": [group],
                       "metadata": {"api_version": version, "internal": action.startswith("_")}})
    return result


def decorate(normalized, document):
    schemas = normalized["components"]["schemas"]
    # PATCH is a partial resource update selected by updateMask. Required fields
    # on the resource's create form must not force unrelated fields into updates.
    for path in normalized["paths"].values():
        if "patch" not in path:
            continue
        body = path["patch"]["requestBody"]["content"]["application/json"]["schema"]
        original = body["$ref"].removeprefix("#/components/schemas/")
        partial = copy.deepcopy(schemas[original])
        partial.pop("required", None)
        if original.endswith("Dataset"):
            partial["properties"]["state"]["readOnly"] = True  # explicitly ignored on update by Google
        schemas[original + "PatchRequest"] = partial
        body["$ref"] = "#/components/schemas/" + original + "PatchRequest"
    # protobuf Any is the only intentionally open payload. Publish each known
    # Document AI operation metadata/result shape while retaining unknown type URLs.
    jobs = ("BatchProcess", "EnableProcessor", "DisableProcessor", "DeleteProcessor", "DeleteProcessorVersion",
            "SetDefaultProcessorVersion", "DeployProcessorVersion", "UndeployProcessorVersion",
            "TrainProcessorVersion", "EvaluateProcessorVersion") if document["version"] == "v1" else (
                "ImportDocuments", "BatchDeleteDocuments", "UpdateDatasetOperation", "ImportProcessorVersion")
    for field, suffix in (("metadata", "Metadata"), ("response", "Response")):
        alternatives = []
        prefix = "GoogleCloudDocumentai" + ("V1beta3" if document["version"] == "v1beta3" else "V1")
        for name, value in list(schemas.items()):
            if name not in {prefix + job + suffix for job in jobs}:
                continue
            full_name = "google.cloud.documentai." + document["version"] + "." + name[len(prefix):]
            branch = copy.deepcopy(value)
            branch.setdefault("properties", {})["@type"] = {"type": "string", "enum": ["type.googleapis.com/" + full_name]}
            branch["required"] = sorted(set(branch.get("required", [])) | {"@type"})
            alternatives.append(branch)
        # A known result may also be a resource (e.g. train returns ProcessorVersion).
        if field == "response":
            for ending in ("Processor", "ProcessorVersion", "SchemaVersion", "Dataset", "DatasetSchema"):
                if prefix + ending in schemas:
                    value = copy.deepcopy(schemas[prefix + ending])
                    value.setdefault("properties", {})["@type"] = {"type": "string", "enum": [
                        "type.googleapis.com/google.cloud.documentai." + document["version"] + "." + ending]}
                    value["required"] = sorted(set(value.get("required", [])) | {"@type"})
                    alternatives.append(value)
        original = schemas["GoogleLongrunningOperation"]["properties"][field]
        fallback = copy.deepcopy(original)
        schemas["GoogleLongrunningOperation"]["properties"][field] = {
            "description": original["description"], "anyOf": alternatives + [fallback]}
    # Lead with a concise sentence so the field's summary remains useful even
    # when the linked specification URL is longer than the summary width.
    for name, definition in schemas.items():
        if name.endswith("DocumentPageDetectedLanguage"):
            definition["properties"]["languageCode"]["description"] = (
                "BCP-47 language code, such as `en-US` or `sr-Latn`. "
                "See the [language code specification](https://www.unicode.org/reports/tr35/#Unicode_locale_identifier).")
    # Keep only transitive schema dependencies of exported/native-helper methods.
    keep = set()
    def references(node):
        if isinstance(node, dict):
            if "$ref" in node:
                name = node["$ref"].removeprefix("#/components/schemas/")
                if name not in keep:
                    keep.add(name)
                    references(schemas[name])
            for key, value in node.items():
                if key not in ("default", "example", "examples"):
                    references(value)
        elif isinstance(node, list):
            for item in node:
                references(item)
    references(normalized["paths"])
    normalized["components"]["schemas"] = {k: v for k, v in schemas.items() if k in keep}
    return normalized


def artifacts():
    inventory = {"imported": "2026-09-19", "normalization_boundary": 1, "sources": {}, "methods": []}
    output = {}
    manifests = {}
    stable_actions = {"documentai.projects." + suffix: action for version, suffix, action, _, _ in OPERATIONS if version == "v1"}
    for version in ("v1", "v1beta3"):
        raw = (DEST / ("google-document-ai-discovery-" + version + ".json")).read_bytes()
        document = json.loads(raw)
        digest = hashlib.sha256(raw).hexdigest()
        declarations = {m["id"]: m for m in discovery.methods(document)}
        selected = {"documentai.projects." + suffix: action for api, suffix, action, _, _ in OPERATIONS if api == version}
        inventory["sources"][version] = {"url": "https://documentai.googleapis.com/$discovery/rest?version=" + version,
            "revision": document["revision"], "sha256": digest, "bytes": len(raw), "methods": len(declarations),
            "schemas": len(document["schemas"])}
        for name, method in sorted(declarations.items()):
            action = selected.get(name)
            if action:
                decision = "consolidated into process-document" if action.startswith("_") else "dedicated action"
                action = "process-document" if action.startswith("_") else action
            elif "humanReviewConfig" in name:
                decision = "excluded: Human in the Loop deprecated; https://docs.cloud.google.com/document-ai/docs/deprecation"
            elif name.endswith("fetchProcessorTypes"):
                decision = "consolidated into stable list-processor-types (paginated processorTypes.list)"
                action = "list-processor-types"
            elif version == "v1beta3" and name in stable_actions:
                decision = "duplicate: supported stable contract is exposed instead"
                action = stable_actions[name]
                action = "process-document" if action.startswith("_") else action
            elif name in ("documentai.projects.operations.get", "documentai.operations.delete"):
                decision = "excluded: legacy nonregional operation namespace; modern jobs use projects.locations.operations"
            else:
                raise ValueError("unreviewed method: " + version + " " + name)
            inventory["methods"].append({"version": version, "id": name, "method": method["httpMethod"],
                "uri_template": method["path"], "flat_path": method["flatPath"], "action": action, "decision": decision})
        manifests[version] = manifest(version, declarations)
        output["google-document-ai-openapi-" + version + ".json"] = decorate(
            discovery.normalize(document, digest, selected), document)
    output["google-document-ai-manifest.json"] = manifests
    output["google-document-ai-inventory.json"] = inventory
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    for name, artifact in artifacts().items():
        text = json.dumps(artifact, indent=2, sort_keys=True, ensure_ascii=False) + "\n"
        path = DEST / name
        if args.check:
            if path.read_text() != text:
                raise SystemExit("stale generated artifact: " + name)
        else:
            path.write_text(text)


if __name__ == "__main__":
    main()
