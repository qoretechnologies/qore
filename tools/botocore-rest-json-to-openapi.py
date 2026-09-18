#!/usr/bin/env python3
"""Convert a pinned botocore rest-json service model to OpenAPI 3 and a Qore action manifest.

The generated files are committed artifacts.  Runtime code never imports botocore or downloads a
service description; updates are explicit, reproducible review steps against an immutable upstream
revision.
"""

# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

from __future__ import annotations

import argparse
import html
import json
import re
from html.parser import HTMLParser
from pathlib import Path
from typing import Any


class MarkdownParser(HTMLParser):
    """Small deterministic converter for the HTML subset in botocore documentation."""

    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.parts: list[str] = []
        self.links: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        if tag in ("p", "ul", "ol", "note", "important"):
            self.parts.append("\n\n")
        elif tag == "li":
            self.parts.append("\n- ")
        elif tag == "code":
            self.parts.append("`")
        elif tag in ("b", "strong"):
            self.parts.append("**")
        elif tag in ("i", "em"):
            self.parts.append("*")
        elif tag == "a":
            href = dict(attrs).get("href") or ""
            self.links.append(href)
            self.parts.append("[")
        elif tag == "br":
            self.parts.append("\n")

    def handle_endtag(self, tag: str) -> None:
        if tag == "code":
            self.parts.append("`")
        elif tag in ("b", "strong"):
            self.parts.append("**")
        elif tag in ("i", "em"):
            self.parts.append("*")
        elif tag == "a":
            self.parts.append("](" + self.links.pop() + ")")
        elif tag in ("p", "ul", "ol", "note", "important"):
            self.parts.append("\n\n")

    def handle_data(self, data: str) -> None:
        self.parts.append(data)


def markdown(source: str | None) -> str:
    if not source:
        return ""
    parser = MarkdownParser()
    parser.feed(html.unescape(source))
    value = "".join(parser.parts)
    value = re.sub(r"[ \t]+", " ", value)
    value = re.sub(r" *\n *", "\n", value)
    value = re.sub(r"\n{3,}", "\n\n", value)
    # Botocore commonly wraps list-item text in <p>; keep it on the Markdown bullet line.
    value = re.sub(r"(?m)^-\n+(?=\S)", "- ", value)
    return value.strip()


def words(name: str) -> str:
    value = re.sub(r"([a-z0-9])([A-Z])", r"\1 \2", name)
    value = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1 \2", value)
    return value


def action_id(name: str) -> str:
    return words(name).replace(" ", "-").lower()


def qore(value: Any) -> str:
    """Render JSON-compatible metadata as deterministic Qore source."""
    if value is True:
        return "True"
    if value is False:
        return "False"
    if value is None:
        return "NOTHING"
    if isinstance(value, str):
        return json.dumps(value, ensure_ascii=False)
    if isinstance(value, (int, float)):
        return repr(value)
    if isinstance(value, list):
        if not value:
            return "()"
        return "(" + ", ".join(qore(item) for item in value) + ",)"
    if isinstance(value, dict):
        if not value:
            return "{}"
        return "{" + ", ".join(qore(key) + ": " + qore(val) for key, val in value.items()) + "}"
    raise TypeError(type(value))


def qore_fields(fields: dict[str, Any]) -> str:
    return "{" + ", ".join(
        qore(name) + ": <RestSchemaFieldOverlayInfo>" + qore(overlay)
        for name, overlay in fields.items()
    ) + "}"


class Converter:
    def __init__(self, model: dict[str, Any], excluded: set[str]) -> None:
        self.model = model
        self.shapes: dict[str, dict[str, Any]] = model["shapes"]
        self.operations = {
            name: op for name, op in model["operations"].items() if name not in excluded
        }
        # OpenAPI path templates are identified by their structure, not by the spelling of their
        # parameter names.  Botocore occasionally uses different names for the same resource path
        # in different operations (for example endpointArn vs endpointIdentifier).  Normalize such
        # paths so the resulting Path Item can contain all of its HTTP methods.
        self.canonical_paths: dict[str, str] = {}
        for op in self.operations.values():
            path = op["http"]["requestUri"]
            self.canonical_paths.setdefault(self.path_key(path), path)

    @staticmethod
    def path_key(path: str) -> str:
        return re.sub(r"\{[^/{}]+\}", "{}", path)

    def normalized_path(self, path: str) -> tuple[str, dict[str, str]]:
        canonical = self.canonical_paths[self.path_key(path)]
        source_names = re.findall(r"\{([^/{}]+)\}", path)
        canonical_names = re.findall(r"\{([^/{}]+)\}", canonical)
        return canonical, dict(zip(source_names, canonical_names))

    @staticmethod
    def ref(name: str) -> dict[str, Any]:
        return {"$ref": "#/components/schemas/" + name}

    def member_schema(self, member: dict[str, Any]) -> dict[str, Any]:
        schema: dict[str, Any] = self.ref(member["shape"])
        desc = markdown(member.get("documentation"))
        if desc:
            schema = {"allOf": [schema], "description": desc}
        if member.get("deprecated"):
            schema["deprecated"] = True
        return schema

    def shape_schema(self, name: str, shape: dict[str, Any]) -> dict[str, Any]:
        stype = shape["type"]
        schema: dict[str, Any]
        if shape.get("document"):
            # Smithy documents are arbitrary JSON, including scalars, arrays, and null.
            # An object schema incorrectly rejects scalar retrieval filters and tool results.
            schema = {}
        elif stype == "structure" and shape.get("union"):
            branches = []
            for member_name, member in shape.get("members", {}).items():
                branches.append({
                    "type": "object",
                    "properties": {member_name: self.member_schema(member)},
                    "required": [member_name],
                    "additionalProperties": False,
                })
            schema = {"oneOf": branches} if branches else {
                "type": "object", "additionalProperties": False,
            }
        elif stype == "structure":
            properties = {
                member_name: self.member_schema(member)
                for member_name, member in shape.get("members", {}).items()
            }
            schema = {
                "type": "object",
                "properties": properties,
                "additionalProperties": False,
            }
            if shape.get("required"):
                schema["required"] = shape["required"]
        elif stype == "list":
            schema = {"type": "array", "items": self.member_schema(shape["member"])}
            if "min" in shape:
                schema["minItems"] = shape["min"]
            if "max" in shape:
                schema["maxItems"] = shape["max"]
        elif stype == "map":
            schema = {"type": "object", "additionalProperties": self.member_schema(shape["value"])}
            if "min" in shape:
                schema["minProperties"] = shape["min"]
            if "max" in shape:
                schema["maxProperties"] = shape["max"]
        elif stype == "string":
            schema = {"type": "string"}
            if shape.get("sensitive"):
                schema["format"] = "password"
                schema["writeOnly"] = True
            if "min" in shape:
                schema["minLength"] = shape["min"]
            if "max" in shape:
                schema["maxLength"] = shape["max"]
            if shape.get("pattern"):
                schema["pattern"] = shape["pattern"]
            if shape.get("enum"):
                schema["enum"] = shape["enum"]
        elif stype in ("integer", "long"):
            schema = {"type": "integer", "format": "int64" if stype == "long" else "int32"}
            if "min" in shape:
                schema["minimum"] = shape["min"]
            if "max" in shape:
                schema["maximum"] = shape["max"]
        elif stype in ("float", "double"):
            schema = {"type": "number", "format": stype}
            if "min" in shape:
                schema["minimum"] = shape["min"]
            if "max" in shape:
                schema["maximum"] = shape["max"]
        elif stype == "boolean":
            schema = {"type": "boolean"}
        elif stype == "timestamp":
            schema = {"type": "string", "format": "date-time"}
        elif stype == "blob":
            schema = {"type": "string", "format": "byte"}
            if shape.get("sensitive"):
                schema["writeOnly"] = True
        else:
            raise ValueError(f"unsupported botocore shape type {stype!r} for {name}")
        desc = markdown(shape.get("documentation"))
        if desc:
            schema["description"] = desc
        return schema

    def body_schema(self, op_name: str, shape: dict[str, Any], output: bool) -> dict[str, Any] | None:
        payload = shape.get("payload")
        if payload:
            member = shape["members"][payload]
            # InvokeModel is documented as JSON despite botocore exposing its payload as bytes.
            if op_name == "InvokeModel" and payload == "body":
                return {
                    "type": "object",
                    "additionalProperties": True,
                    "description": markdown(member.get("documentation")),
                }
            return self.member_schema(member)

        members = {
            name: member for name, member in shape.get("members", {}).items()
            if member.get("location") not in ("uri", "querystring", "header", "headers", "statusCode")
        }
        if not members:
            if output:
                return {
                    "type": "object",
                    "nullable": True,
                    "properties": {},
                    "additionalProperties": False,
                    "description": "The operation completed successfully without a response document.",
                }
            return None
        schema: dict[str, Any] = {
            "type": "object",
            "properties": {name: self.member_schema(member) for name, member in members.items()},
            "additionalProperties": False,
        }
        required = [name for name in shape.get("required", []) if name in members]
        if required:
            schema["required"] = required
        return schema

    def parameter(self, logical_name: str, member: dict[str, Any], required: set[str],
            path_names: dict[str, str]) -> dict[str, Any]:
        location = member["location"]
        where = {"uri": "path", "querystring": "query", "header": "header"}[location]
        wire_name = member.get("locationName", logical_name)
        if where == "path":
            wire_name = path_names.get(wire_name, wire_name)
        result = {
            "name": wire_name,
            "in": where,
            "required": where == "path" or logical_name in required,
            "schema": self.ref(member["shape"]),
        }
        desc = markdown(member.get("documentation"))
        if desc:
            result["description"] = desc
        if where == "query" and self.shapes[member["shape"]]["type"] == "list":
            result.update({"style": "form", "explode": True})
        return result

    def operation(self, name: str, op: dict[str, Any]) -> tuple[str, str, dict[str, Any]]:
        method = op["http"]["method"].lower()
        path, path_names = self.normalized_path(op["http"]["requestUri"])
        operation: dict[str, Any] = {
            "operationId": name,
            "summary": words(name),
            "description": markdown(op.get("documentation")),
            "responses": {},
        }
        if op.get("input"):
            input_shape = self.shapes[op["input"]["shape"]]
            required = set(input_shape.get("required", []))
            params = [
                self.parameter(member_name, member, required, path_names)
                for member_name, member in input_shape.get("members", {}).items()
                if member.get("location") in ("uri", "querystring", "header")
            ]
            if params:
                operation["parameters"] = params
            body = self.body_schema(name, input_shape, False)
            if body:
                operation["requestBody"] = {
                    "required": bool(name == "InvokeModel"
                        or input_shape.get("payload") in required
                        or any(key in required for key, member in input_shape.get("members", {}).items()
                            if not member.get("location"))),
                    "content": {"application/json": {"schema": body}},
                }
        output_shape = self.shapes[op["output"]["shape"]]
        response_code = str(op["http"].get("responseCode", 200))
        response: dict[str, Any] = {"description": "Successful response"}
        response_body = self.body_schema(name, output_shape, True)
        if response_body:
            response["content"] = {"application/json": {"schema": response_body}}
        header_members = {
            member.get("locationName", member_name): self.member_schema(member)
            for member_name, member in output_shape.get("members", {}).items()
            if member.get("location") == "header"
        }
        if header_members:
            response["headers"] = {
                header_name: {"schema": header_schema}
                for header_name, header_schema in header_members.items()
            }
        operation["responses"][response_code] = response
        operation["responses"]["default"] = {
            "description": "AWS service error",
            "content": {"application/json": {"schema": {
                "type": "object", "additionalProperties": True,
            }}},
        }
        return path, method, operation

    def openapi(self) -> dict[str, Any]:
        paths: dict[str, Any] = {}
        for name, op in self.operations.items():
            path, method, operation = self.operation(name, op)
            paths.setdefault(path, {})[method] = operation
        schemas = {
            name: self.shape_schema(name, shape)
            for name, shape in self.shapes.items()
        }
        endpoint = self.model["metadata"]["endpointPrefix"]
        return {
            "openapi": "3.0.3",
            "info": {
                "title": self.model["metadata"]["serviceFullName"],
                "version": self.model["metadata"]["apiVersion"],
                "description": markdown(self.model.get("documentation")),
            },
            "servers": [{"url": f"https://{endpoint}.{{region}}.amazonaws.com", "variables": {
                "region": {"default": "us-east-1", "description": "AWS Region"},
            }}],
            "paths": paths,
            "components": {"schemas": schemas},
            "x-qore-botocore-boundary-version": 1,
        }

    @staticmethod
    def group(name: str) -> str:
        rules = (
            ("AdvancedPromptOptimization", "Advanced Prompt Optimization"),
            ("AutomatedReasoning", "Automated Reasoning"),
            ("Guardrail", "Guardrails"),
            ("Evaluation", "Evaluations"),
            ("InferenceProfile", "Inference Profiles"),
            ("Marketplace", "Marketplace Models"),
            ("ModelInvocationJob", "Model Invocation Jobs"),
            ("ModelInvocationLogging", "Account Settings"),
            ("ModelCustomization", "Model Customization"),
            ("CustomModelDeployment", "Custom Models"),
            ("CustomModel", "Custom Models"),
            ("ImportedModel", "Model Imports"),
            ("ModelImport", "Model Imports"),
            ("PromptRouter", "Prompt Routers"),
            ("ProvisionedModel", "Provisioned Throughput"),
            ("FoundationModelAgreement", "Model Access"),
            ("FoundationModel", "Foundation Models"),
            ("ResourcePolicy", "Resource Policies"),
            ("Tag", "Tags"),
            ("AsyncInvoke", "Asynchronous Inference"),
            ("CountTokens", "Inference"),
            ("Invoke", "Inference"),
            ("Converse", "Inference"),
        )
        return next((group for needle, group in rules if needle in name), "Bedrock Management")

    def manifest_entries(self) -> list[dict[str, Any]]:
        entries = []
        for name, op in self.operations.items():
            display = words(name)
            plain = re.sub(r"[`*_\[\]()]", "", markdown(op.get("documentation"))).split("\n", 1)[0]
            first = re.split(r"(?<=[.!?])\s", plain, maxsplit=1)[0].strip()
            short = first if 0 < len(first) < 80 else f"Run the {display} operation."
            input_shape = self.shapes[op["input"]["shape"]]
            required = set(input_shape.get("required", []))
            path, path_names = self.normalized_path(op["http"]["requestUri"])
            fields: dict[str, Any] = {}
            payload = input_shape.get("payload")
            for logical, member in input_shape.get("members", {}).items():
                if payload == logical:
                    fields["body"] = {
                        "display_name": words(logical).capitalize(),
                        "short_desc": f"{display} request body",
                        "desc": markdown(member.get("documentation"))
                            or f"JSON request document sent by the {display} operation.",
                        "preselected": name == "InvokeModel" or logical in required,
                    }
                    continue
                wire = member.get("locationName", logical) if member.get("location") else logical
                if member.get("location") == "uri":
                    wire = path_names.get(wire, wire)
                overlay: dict[str, Any] = {}
                if wire != logical:
                    overlay["name"] = logical
                if logical in required:
                    overlay["preselected"] = True
                if logical in ("modelId", "modelIdentifier"):
                    overlay.update({"ref_data": "foundation-models", "supports_custom_values": True})
                elif logical == "guardrailIdentifier":
                    overlay.update({"ref_data": "guardrails", "supports_custom_values": True})
                elif logical in ("inferenceProfileIdentifier", "inferenceProfileId"):
                    overlay.update({"ref_data": "inference-profiles", "supports_custom_values": True})
                if re.search(r"password|secretAccessKey|privateKey", logical, re.I):
                    overlay["sensitive"] = True
                if name == "InvokeModel" and logical in ("contentType", "accept"):
                    overlay["default_value"] = "application/json"
                    overlay["allowed_values"] = [{"value": "application/json", "display_name": "JSON"}]
                if overlay:
                    fields[wire] = overlay
            desc = markdown(op.get("documentation")) or f"Runs the AWS Bedrock `{name}` operation."
            entries.append({
                "action": action_id(name),
                "method": op["http"]["method"],
                "path": path,
                "operation_id": name,
                "display_name": display,
                "short_desc": short,
                "desc": desc,
                "groups": [self.group(name)],
                "fields": fields,
                "flatten_body": not bool(payload),
                "metadata": {"aws_operation": name},
            })
        return entries


def render_manifest(namespace: str, class_name: str, entries: list[dict[str, Any]], reference_data: bool) -> str:
    lines = [
        "# -*- mode: qore; indent-tabs-mode: nil -*-",
        "#! Generated AWS Bedrock action manifest; regenerate with tools/botocore-rest-json-to-openapi.py",
        "",
        "/** Copyright 2026 Qore Technologies, s.r.o.",
        "    SPDX-License-Identifier: MIT",
        "*/",
        "",
        f"public namespace {namespace} {{",
        f"#! Pinned action inventory and presentation metadata for {class_name}",
        f"public class {class_name} {{",
        "    public {",
        "        #! Exact reviewed operation inventory in upstream declaration order",
        "        const Manifest = (",
    ]
    for entry in entries:
        lines.extend([
            "            <RestSchemaActionInfo>{",
            *[f"                {json.dumps(key)}: "
              f"{qore_fields(value) if key == 'fields' else qore(value)}," for key, value in entry.items()
              if value not in ({}, (), [], None)],
            "            },",
        ])
    lines.append("        );")
    if reference_data:
        lines.extend([
            "",
            "        #! Resource listings used for identifier dropdowns; custom values remain accepted",
            "        const ReferenceData = {",
            "            \"foundation-models\": <RestSchemaReferenceDataInfo>{\"name\": \"foundation-models\", \"action\": \"list-foundation-models\", \"records_path\": (\"modelSummaries\",), \"value_field\": \"modelId\", \"display_field\": \"modelName\"},",
            "            \"guardrails\": <RestSchemaReferenceDataInfo>{\"name\": \"guardrails\", \"action\": \"list-guardrails\", \"records_path\": (\"guardrails\",), \"value_field\": \"id\", \"display_field\": \"name\"},",
            "            \"inference-profiles\": <RestSchemaReferenceDataInfo>{\"name\": \"inference-profiles\", \"action\": \"list-inference-profiles\", \"records_path\": (\"inferenceProfileSummaries\",), \"value_field\": \"inferenceProfileId\", \"display_field\": \"inferenceProfileName\"},",
            "        };",
        ])
    lines.extend(["    }", "}", "}", ""])
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--openapi-output", required=True, type=Path)
    parser.add_argument("--manifest-output", required=True, type=Path)
    parser.add_argument("--namespace", required=True)
    parser.add_argument("--manifest-class", required=True)
    parser.add_argument("--exclude", action="append", default=[])
    parser.add_argument("--reference-data", action="store_true")
    args = parser.parse_args()

    model = json.loads(args.input.read_text(encoding="utf-8"))
    if model.get("version") != "2.0" or model.get("metadata", {}).get("protocol") != "rest-json":
        raise SystemExit("input is not a botocore service-2 rest-json model")
    unknown = set(args.exclude) - set(model["operations"])
    if unknown:
        raise SystemExit("unknown excluded operations: " + ", ".join(sorted(unknown)))
    converter = Converter(model, set(args.exclude))
    args.openapi_output.write_text(
        json.dumps(converter.openapi(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    args.manifest_output.write_text(
        render_manifest(args.namespace, args.manifest_class, converter.manifest_entries(), args.reference_data),
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
