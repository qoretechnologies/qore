#!/usr/bin/env python3
"""Import Textract's pinned AWS JSON model and generate component types, never HTTP routes.

Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

Usage: tools/textract-model-import.py /path/to/unmodified/service-2.json
The input must match the recorded immutable revision. The OpenAPI document is a local type
container only: its paths are empty, and native AWS JSON operations remain in the service model.
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import sys

REVISION = "2891dd413e8b644590042d8ff226fc2181711985"
SHA256 = "5ee161c8c63c848ff4b9ba3905d3fc00245b477056ccf042c9f79273a24b8da3"
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "qlib/AwsTextractDataProvider"

# Presentation is curated here, at the versioned import boundary. Full AWS documentation remains
# in the unmodified model. Shared field descriptions deliberately agree across all operations.
FIELDS = {
    "Document": "Document to process, supplied as bytes or an S3 object.",
    "DocumentPages": "One or two images of a US passport or driver's license.",
    "Bytes": "Raw document bytes or base64 content, up to 10 MB for a synchronous request.",
    "S3Object": "Source object in an S3 bucket in the connection's AWS region.",
    "Bucket": "Name of the S3 bucket containing the source document.",
    "Name": "Object key inside the S3 bucket, for example `invoices/sample.pdf`.",
    "Version": "Version of the selected resource.",
    "DocumentLocation": "S3 document location for an asynchronous analysis job.",
    "FeatureTypes": "Document features to extract; select at least one.",
    "QueriesConfig": "Questions to answer when the `QUERIES` feature is selected.",
    "Queries": "Questions to ask about the document, each with optional page selectors.",
    "Text": "Question to answer, for example `What is the invoice total?`.",
    "Alias": "A short name for locating this question in the results.",
    "Pages": "Page selectors such as `1`, `1-3`, or `4-*`; use only `*` for all pages.",
    "AdaptersConfig": "Custom query adapters and versions to use for selected pages.",
    "Adapters": "Adapters to apply; selected page ranges must not overlap.",
    "AdapterId": "Adapter in the connection's AWS account and region.",
    "AdapterVersion": "Version belonging to the selected adapter.",
    "AdapterName": "A descriptive name for the custom query adapter.",
    "AutoUpdate": "Whether AWS may automatically update the adapter.",
    "Description": "A description explaining the adapter's purpose.",
    "ClientRequestToken": "Idempotency token for safely retrying the same request.",
    "JobId": "Job ID returned by the matching start action; AWS provides no job listing.",
    "JobTag": "A label included in the job completion notification.",
    "MaxResults": "Maximum number of results in this response page.",
    "NextToken": "Continuation token from the preceding response in the same operation.",
    "NotificationChannel": "SNS topic and IAM role for asynchronous completion notifications.",
    "SNSTopicArn": "SNS topic ARN in the connection's region for completion notifications.",
    "RoleArn": "IAM role ARN allowing Textract to publish to the SNS topic.",
    "OutputConfig": "S3 destination for analysis or training output.",
    "S3Bucket": "S3 bucket to receive output; the caller must have permission to write it.",
    "S3Prefix": "Object-key prefix for output files in the destination bucket.",
    "KMSKeyId": "KMS key ID or ARN for encrypting output; required permissions must be granted.",
    "DatasetConfig": "S3 manifest identifying the training and test documents.",
    "ManifestS3Object": "S3 object containing the adapter training dataset manifest.",
    "AfterCreationTime": "Return resources created after this time.",
    "BeforeCreationTime": "Return resources created before this time.",
    "Tags": "Resource tags as a map of names to values; an empty tag value is valid.",
    "TagKeys": "Names of the resource tags to remove.",
    "ResourceARN": "Textract resource ARN in the connection's AWS account and region.",
    "HumanLoopConfig": "Human review settings for existing Amazon A2I customers only.",
    "HumanLoopName": "Unique name for this human review in the AWS region.",
    "FlowDefinitionArn": "ARN of an existing Amazon A2I human review workflow.",
    "DataAttributes": "Content declarations used to route the document for human review.",
    "ContentClassifiers": "Content declarations for the human review workflow.",
}

ACTIONS = {
    "AnalyzeDocument": "Extract tables, forms, signatures, layout, and query answers",
    "AnalyzeExpense": "Extract invoice and receipt totals, vendors, and line items",
    "AnalyzeID": "Extract identity fields from US passports and driver's licenses",
    "CreateAdapter": "Create a custom query adapter",
    "CreateAdapterVersion": "Train a version of a custom query adapter",
    "DeleteAdapter": "Delete an unused custom query adapter",
    "DeleteAdapterVersion": "Delete a version of a custom query adapter",
    "DetectDocumentText": "Extract lines and words from a document",
    "GetAdapter": "Read a custom query adapter's settings",
    "GetAdapterVersion": "Read training status and evaluation results for an adapter version",
    "GetDocumentAnalysis": "Read a page of asynchronous document analysis results",
    "GetDocumentTextDetection": "Read a page of asynchronous text detection results",
    "GetExpenseAnalysis": "Read a page of asynchronous expense analysis results",
    "GetLendingAnalysis": "Read a page of asynchronous lending analysis results",
    "GetLendingAnalysisSummary": "Read the document classification summary for a lending job",
    "ListAdapters": "List custom query adapters in this account and region",
    "ListAdapterVersions": "List versions of custom query adapters",
    "ListTagsForResource": "Read the tags attached to a Textract resource",
    "StartDocumentAnalysis": "Start asynchronous extraction of document structure",
    "StartDocumentTextDetection": "Start asynchronous extraction of document text",
    "StartExpenseAnalysis": "Start asynchronous invoice and receipt analysis",
    "StartLendingAnalysis": "Start asynchronous classification and extraction of lending documents",
    "TagResource": "Add or update tags on a Textract resource",
    "UntagResource": "Remove tags from a Textract resource",
    "UpdateAdapter": "Update a custom query adapter's settings",
}


def main() -> None:
    raw = Path(sys.argv[1]).read_bytes()
    if hashlib.sha256(raw).hexdigest() != SHA256:
        raise SystemExit("Input differs from the pinned official Textract model")
    model = json.loads(raw)
    if model["metadata"]["protocol"] != "json" or model["metadata"]["jsonVersion"] != "1.1":
        raise SystemExit("Unsupported AWS protocol")
    if set(model["operations"]) != set(ACTIONS):
        raise SystemExit("Operation inventory differs from the reviewed manifest")
    spec = importlib.util.spec_from_file_location("botocore_types", ROOT / "tools/botocore-rest-json-to-openapi.py")
    common = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(common)
    converter = common.Converter(model, set())
    shapes = model["shapes"]
    inputs: set[str] = set()
    reachable: set[str] = set()

    def visit(name: str, seen: set[str]) -> None:
        if name in seen:
            return
        seen.add(name)
        shape = shapes[name]
        for member in shape.get("members", {}).values():
            visit(member["shape"], seen)
        for key in ("member", "key", "value"):
            if key in shape:
                visit(shape[key]["shape"], seen)

    for op in model["operations"].values():
        visit(op["input"]["shape"], inputs)
        visit(op["input"]["shape"], reachable)
        visit(op["output"]["shape"], reachable)
    schemas = {}
    for name in sorted(reachable):
        schema = converter.shape_schema(name, shapes[name])
        # This is the action-value representation. Native transport explicitly converts timestamps
        # to/from epoch seconds and blobs to base64; no OpenAPI transport is generated.
        if name in inputs:
            schema.pop("description", None)
            for field, definition in schema.get("properties", {}).items():
                if field not in FIELDS:
                    raise SystemExit(f"Missing input field presentation: {name}.{field}")
                definition["title"] = common.words(field)
                definition["description"] = FIELDS[field]
        schemas[name] = schema
    # Document prose imposes these constraints although the SDK shape omits them.
    schemas["Document"]["oneOf"] = [{"required": ["Bytes"]}, {"required": ["S3Object"]}]
    schemas["DocumentLocation"]["required"] = ["S3Object"]
    schemas["S3Object"]["required"] = ["Bucket", "Name"]
    schemas["AdapterVersionDatasetConfig"]["required"] = ["ManifestS3Object"]
    schemas["FeatureTypes"]["minItems"] = 1
    schemas["DocumentPages"].update(minItems=1, maxItems=2)
    schemas["CreateAdapterRequest"]["properties"]["FeatureTypes"] = {
        "type": "array", "minItems": 1, "maxItems": 1,
        "items": {"type": "string", "enum": ["QUERIES"]},
        "title": "Feature Types", "description": "Custom adapters support the `QUERIES` feature only.",
    }
    document = {
        "openapi": "3.0.3", "info": {"title": "Textract action value types", "version": "2018-06-27"},
        "paths": {}, "components": {"schemas": schemas},
        "x-qore-textract-boundary": {"version": 1, "source_revision": REVISION,
            "source_sha256": SHA256, "source_size": len(raw), "operations": 25,
            "source_url": f"https://raw.githubusercontent.com/boto/botocore/{REVISION}/botocore/data/textract/2018-06-27/service-2.json",
            "representation": "Types only; native AWS JSON 1.1 transport is implemented separately",
            "normalizations": ["document source exclusivity", "required asynchronous S3 source",
                "required S3 bucket and key", "required training manifest", "nonempty features",
                "query-only adapter creation", "one or two identity images", "timestamp action values",
                "curated input presentation", "native query dependencies and page-selector validation"]},
    }
    (OUT / "textract-types.json").write_text(json.dumps(document, ensure_ascii=False, indent=2) + "\n")
    (OUT / "textract-service-2.json").write_bytes(raw)
    lines = ["# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT",
             "# Generated by tools/textract-model-import.py; do not edit.",
             "public namespace AwsTextractDataProvider {", "public class AwsTextractManifest {", "    public {",
             "        const Operations = {"]
    for operation, op in model["operations"].items():
        short = ACTIONS[operation]
        desc = short + ". See the [AWS API reference](https://docs.aws.amazon.com/textract/latest/APIReference/API_" + operation + ".html)."
        if operation.startswith("Start"):
            desc += " Retain `JobId` and use the matching get action to check status and retrieve every result page."
        if operation.startswith("Get") and "JobId" in shapes[op["input"]["shape"]].get("members", {}):
            desc += " Check `JobStatus` before using results; `PARTIAL_SUCCESS` includes warnings and `FAILED` includes a status message."
        if "NextToken" in shapes[op["output"]["shape"]].get("members", {}):
            desc += " Pass `NextToken` to the same action to retrieve the next page."
        if operation == "CreateAdapterVersion":
            desc += " **Training incurs AWS charges** and requires an authorized S3 dataset and output location."
        info = {"operation": operation, "display_name": common.words(operation), "short_desc": short,
                "desc": desc, "input": op["input"]["shape"], "output": op["output"]["shape"]}
        lines.append("            " + common.qore(common.action_id(operation)) + ": " + common.qore(info) + ",")
    lines += ["        };", "    }", "}", "}", ""]
    (OUT / "AwsTextractManifest.qc").write_text("\n".join(lines))
    print(f"Imported {len(model['operations'])} native operations and {len(schemas)} component types")


if __name__ == "__main__":
    main()
