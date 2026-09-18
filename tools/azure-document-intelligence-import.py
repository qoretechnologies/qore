#!/usr/bin/env python3
"""Normalize the pinned Microsoft Document Intelligence Swagger at boundary version 1.

Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

This is a checked adapter for this one vendor revision, not a general Swagger importer.
The original document remains the source of every request and response contract. Azure's
synthetic overload selectors are metadata and never become HTTP query parameters.
"""

import argparse
import copy
import hashlib
import json
import re
import subprocess
import tempfile
from pathlib import Path

REVISION = "f9a6d8b3ea13a55487777d3651380b49a787f1f6"
SOURCE = (f"https://raw.githubusercontent.com/Azure/azure-rest-api-specs/{REVISION}/"
          "specification/ai/data-plane/DocumentIntelligence/stable/2024-11-30/DocumentIntelligence.json")
SHA256 = "3f750834da9e3acc162b781711bf0eda02ab2bd8a30c2b4c6a62c6910e7f1bd0"
MODULE = Path(__file__).resolve().parents[1] / "qlib/AzureDocumentIntelligenceDataProvider"
METHODS = {"get", "post", "delete"}


def schema(value):
    """Rewrite declared schema positions, retaining vendor extensions and constraints."""
    result = copy.deepcopy(value)
    if "description" in result:
        result["description"] = re.sub(r"\b(?:urlSource|base64Source|docTypes|classifierId|modelId)\b",
                                       lambda match: "`" + match.group() + "`", result["description"])
    if "$ref" in result:
        assert result["$ref"].startswith("#/definitions/"), result["$ref"]
        result["$ref"] = result["$ref"].replace("#/definitions/", "#/components/schemas/", 1)
    if result.get("type") == "file":
        result.update(type="string", format="binary")
    for key in ("properties",):
        if key in result:
            result[key] = {name: schema(item) for name, item in result[key].items()}
    for key in ("items", "additionalProperties", "not"):
        if isinstance(result.get(key), dict):
            result[key] = schema(result[key])
    for key in ("allOf", "oneOf", "anyOf"):
        if key in result:
            result[key] = [schema(item) for item in result[key]]
    if "discriminator" in result:
        assert isinstance(result["discriminator"], str)
        result["discriminator"] = {"propertyName": result["discriminator"]}
    return result


def dereference(parameter, source):
    if "$ref" not in parameter:
        return parameter
    assert len(parameter) == 1 and parameter["$ref"].startswith("#/parameters/")
    return source["parameters"][parameter["$ref"].split("/", 2)[2]]


def parameter(value):
    """Move Swagger's inline parameter schema into its OpenAPI 3 schema member."""
    assert value["in"] in ("path", "query", "header")
    keys = {"name", "in", "description", "required"}
    result = {key: copy.deepcopy(item) for key, item in value.items()
              if key in keys or key.startswith("x-")}
    result["schema"] = schema({key: item for key, item in value.items()
                               if key not in keys and key != "collectionFormat" and not key.startswith("x-")})
    if "collectionFormat" in value:
        assert value["in"] == "query" and value["collectionFormat"] == "csv"
        result.update(style="form", explode=False)
    return result


def operation(value, source):
    result = {key: copy.deepcopy(item) for key, item in value.items()
              if key not in ("parameters", "responses", "consumes", "produces", "x-ms-examples")}
    result["parameters"] = []
    for item in value.get("parameters", []):
        item = dereference(item, source)
        if item["in"] == "body":
            assert "requestBody" not in result
            result["requestBody"] = {
                "required": item.get("required", False),
                "description": item.get("description", "Request body."),
                "content": {media: {"schema": schema(item["schema"])}
                            for media in value.get("consumes", source["consumes"])},
            }
        else:
            result["parameters"].append(parameter(item))
    result["responses"] = {}
    for code, response in value["responses"].items():
        converted = {"description": response["description"]}
        if "schema" in response:
            body = schema(response["schema"])
            # Swagger produces applies to the whole operation. Azure's JSON alternative is
            # its error response, not a JSON representation of the successful PNG/PDF bytes.
            media = value.get("produces", source["produces"])
            if code == "default":
                media = ["application/json"]
            elif body.get("format") == "binary":
                media = [item for item in media if item != "application/json"]
            converted["content"] = {item: {"schema": copy.deepcopy(body)} for item in media}
        if "headers" in response:
            converted["headers"] = {
                name: {"description": header.get("description", name),
                       "schema": schema({key: item for key, item in header.items() if key != "description"})}
                for name, header in response["headers"].items()
            }
        result["responses"][code] = converted
    return result


def normalize(source, size):
    assert source["swagger"] == "2.0" and source["info"]["version"] == "2024-11-30"
    definitions = {name: schema(value) for name, value in source["definitions"].items()}
    # Swagger inheritance gives each subtype its base properties. Materialize this inheritance
    # before adding the discriminator mapping, avoiding a base -> subtype -> base cycle.
    base_name = "DocumentIntelligenceOperationDetails"
    base = definitions[base_name]
    mapping = {}
    for name, definition in definitions.items():
        if "allOf" not in definition:
            continue
        assert definition["allOf"] == [{"$ref": "#/components/schemas/" + base_name}]
        del definition["allOf"]
        definition["properties"] = copy.deepcopy(base["properties"]) | definition["properties"]
        definition["required"] = list(dict.fromkeys(base["required"] + definition.get("required", [])))
        kind = definition["x-ms-discriminator-value"]
        definition["properties"]["kind"] = {"type": "string", "enum": [kind], "description": "Operation kind."}
        mapping[kind] = "#/components/schemas/" + name
    assert len(mapping) == 5
    base["discriminator"]["mapping"] = mapping
    provenance = {"boundary_version": 1, "source_url": SOURCE, "source_sha256": SHA256,
                  "source_size": size, "source_revision": REVISION, "api_version": "2024-11-30",
                  "source_operations": 34, "source_schemas": 79}
    common = {"openapi": "3.0.3", "info": copy.deepcopy(source["info"]),
              "servers": [{"url": "/documentintelligence"}], "paths": {},
              "components": {"schemas": definitions, "securitySchemes": {
                  "ApiKeyAuth": copy.deepcopy(source["securityDefinitions"]["ApiKeyAuth"]),
                  "OAuth2Auth": {"type": "oauth2", "flows": {"authorizationCode": {
                      "authorizationUrl": source["securityDefinitions"]["OAuth2Auth"]["authorizationUrl"],
                      "tokenUrl": source["securityDefinitions"]["OAuth2Auth"]["tokenUrl"],
                      "scopes": source["securityDefinitions"]["OAuth2Auth"]["scopes"]}}}}},
              "security": copy.deepcopy(source["security"]), "x-qore-source": provenance}
    main, streams = copy.deepcopy(common), copy.deepcopy(common)
    inventory = []
    for path, path_item in source["paths"].items():
        for method, value in path_item.items():
            assert method in METHODS
            main["paths"].setdefault(path, {})[method] = operation(value, source)
            inventory.append({"operation_id": value["operationId"], "method": method.upper(), "path": path})
    assert len(inventory) == 27
    for path, path_item in source["x-ms-paths"].items():
        wire_path, selector = path.split("?", 1)
        assert selector.startswith("_overload=")
        for method, value in path_item.items():
            converted = operation(value, source)
            previous = main["paths"][wire_path][method]
            assert converted["parameters"] == previous["parameters"]
            if method == "post":
                assert previous["operationId"].endswith("FromStream")
                assert converted["responses"] == previous["responses"]
                streams["paths"][wire_path] = {method: previous}
                main["paths"][wire_path][method] = converted
                decision = "JSON request alternative; stream alternative in the stream partition"
            else:
                assert method == "get" and wire_path == "/operations/{operationId}"
                if value["operationId"] == "MiscellaneousOperations_GetOperation":
                    main["paths"][wire_path][method] = converted
                decision = "One wire operation; the kind discriminator retains every typed result"
            inventory.append({"operation_id": value["operationId"], "method": method.upper(),
                              "path": wire_path, "normalization": decision})
    assert len(inventory) == 34 and len(streams["paths"]) == 2
    for doc in (main, streams):
        doc["x-qore-normalized"] = [
            "Swagger 2.0 converted to OpenAPI 3.0.3 at Azure Document Intelligence boundary version 1.",
            "JSON and binary requests occupy separate schema partitions with identical real service paths.",
            "Synthetic _overload selectors are removed; operation results use the vendor kind discriminator.",
            "Operation inheritance is expanded before installing discriminator mappings.",
            "Successful file responses use their binary media; error responses remain JSON.",
        ]
    return {"azure-document-intelligence-openapi.json": main,
            "azure-document-intelligence-streams.json": streams,
            "azure-document-intelligence-inventory.json": {"provenance": provenance, "operations": inventory}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Unmodified pinned Microsoft Swagger JSON")
    parser.add_argument("--output", type=Path, default=MODULE)
    parser.add_argument("--check", action="store_true", help="Verify byte-for-byte reproducibility")
    parser.add_argument("--qore", default="qore", help="Qore executable providing the shared RestSchemaPruner")
    args = parser.parse_args()
    raw = args.source.read_bytes()
    if hashlib.sha256(raw).hexdigest() != SHA256:
        parser.error("source checksum differs from the reviewed Microsoft revision")
    documents = normalize(json.loads(raw), len(raw))
    # Reuse the shared reachability implementation, including discriminator mappings. Keep the
    # original Swagger separately so pruning never hides the upstream input or unused definitions.
    with tempfile.TemporaryDirectory(prefix="azure-di-import-") as temporary:
        input_file = Path(temporary) / "normalized.json"
        input_file.write_text(json.dumps(documents), encoding="utf-8")
        result = subprocess.run([args.qore, str(Path(__file__).with_name("azure-document-intelligence-prune.qr")),
                                 str(input_file)], capture_output=True, text=True, check=True)
        documents = json.loads(result.stdout)
    for name, value in documents.items():
        data = (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode()
        path = args.output / name
        if args.check:
            if path.read_bytes() != data:
                parser.error(f"generated artifact differs: {name}")
        else:
            path.write_bytes(data)
        print(name)


if __name__ == "__main__":
    main()
