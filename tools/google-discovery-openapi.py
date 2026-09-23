#!/usr/bin/env python3
"""Normalize a pinned Google Discovery v1 document to OpenAPI 3.0.3, offline.

Copyright 2026 Qore Technologies, s.r.o.; MIT license (see COPYING.MIT).

This is a Qore-generated description, not a Google-published OpenAPI document.
The versioned boundary uses Google's flatPath to expand reserved resource names
into separately escaped segments. It refuses unsupported constructs instead of
silently reducing their contracts to untyped values. No network I/O occurs here.
"""

import argparse
import copy
import hashlib
import json
from pathlib import Path
import re

BOUNDARY_VERSION = 1
SCHEMA_KEYS = {
    "$ref", "type", "format", "description", "properties", "items", "additionalProperties",
    "enum", "enumDescriptions", "enumDeprecated", "default", "readOnly", "deprecated", "id",
    "minimum", "maximum", "pattern", "required", "annotations", "location", "repeated",
}


def description(text):
    """Remove field-behavior prefixes already represented by structured metadata.

    This gives option summaries useful content instead of just 'Optional' or
    'Required'. The original descriptions remain in the pinned Discovery bytes.
    Deprecation notices and constraints within the description are untouched.
    """
    text = re.sub(r"^(?:(?:Required|Optional|Output only)\.\s*)+", "", text)
    # Some published descriptions leave the closing backtick off a final resource
    # template. Repair presentation only; the original source bytes stay pinned.
    if text.count("`") % 2:
        text = re.sub(r"`([^`\n]*\{[A-Za-z_]+\}[^`\n]*)\.$", r"`\1`.", text)
    return markdown_description(text)


def markdown_description(text):
    """Format long public prose without dropping any contract statements."""
    if len(text) <= 500 or ("**" in text and "\n- " in text):
        return text
    # Keep the first sentence as the summary and preserve the remainder verbatim
    # except for paragraph/list separators. Do not split periods inside code/URLs.
    first, separator, rest = text.partition(". ")
    if not separator:
        first, separator, rest = text.partition(".\n\n")
    if separator:
        rest = "".join(part if i % 2 else part.replace(" - ", "\n- ").replace(" * **", "\n- **")
                       for i, part in enumerate(re.split(r"(`[^`]*`)", rest)))
        return first + ".\n\n**Details**\n\n- " + rest
    return text


def schema(source):
    """Convert one declared schema position; arbitrary defaults remain opaque."""
    if not isinstance(source, dict):
        raise ValueError("Discovery schemas must be objects")
    unknown = source.keys() - SCHEMA_KEYS
    if unknown:
        raise ValueError(f"unsupported Discovery schema keys: {sorted(unknown)}")
    result = {}
    for key in ("type", "description", "default", "readOnly", "deprecated", "pattern", "enum"):
        if key in source:
            result[key] = copy.deepcopy(source[key])
    if "description" in result:
        result["description"] = description(result["description"])
        if re.match(r"^(?:(?:Required|Optional)\.\s*)*Output only\.", source["description"]):
            result["readOnly"] = True
    if source.get("type") == "any":
        result.pop("type")
    if "$ref" in source:
        ref = source["$ref"]
        if not isinstance(ref, str) or not re.fullmatch(r"[A-Za-z0-9_]+", ref):
            raise ValueError("invalid Discovery schema reference")
        result["$ref"] = "#/components/schemas/" + ref
    if "format" in source:
        fmt = source["format"]
        if fmt == "google-datetime":
            result["format"] = "date-time"
        elif fmt in ("google-fieldmask", "google-duration", "int64", "uint64") and source.get("type") == "string":
            # Google protobuf int64 is a decimal STRING on the JSON wire.
            result["x-google-format"] = fmt
        elif fmt == "uint32":
            result.update(format="int64", minimum=0, maximum=4294967295)
        elif fmt in ("byte", "date", "date-time", "int32", "int64", "float", "double"):
            result["format"] = fmt
        else:
            raise ValueError(f"unsupported Discovery format: {fmt}")
    for key in ("minimum", "maximum"):
        if key in source:
            result[key] = int(source[key]) if source.get("type") == "integer" else float(source[key])
    if "properties" in source:
        result["properties"] = {k: schema(v) for k, v in sorted(source["properties"].items())}
        # Discovery encodes protobuf field behavior in prose as well as readOnly.
        # Only the explicit standard prefix is interpreted; other prose is opaque.
        required = [k for k, v in source["properties"].items()
                    if v.get("description", "").startswith("Required.") and not result["properties"][k].get("readOnly")]
        if required:
            result["required"] = sorted(required)
    if "items" in source:
        result["items"] = schema(source["items"])
    if "additionalProperties" in source:
        value = source["additionalProperties"]
        result["additionalProperties"] = schema(value) if isinstance(value, dict) else value
    if "enumDescriptions" in source:
        if len(source["enumDescriptions"]) != len(source.get("enum", [])):
            raise ValueError("enum description count does not match values")
        result["x-enum-descriptions"] = copy.deepcopy(source["enumDescriptions"])
    if "enum" in source:
        result["x-enumNames"] = [" ".join(word if word in {"OCR", "PDF", "GCS", "AI", "ID", "URI", "UTF8"}
                                         else word.title() for word in str(v).split("_"))
                                 for v in source["enum"]]
    if "enumDeprecated" in source:
        result["x-google-enum-deprecated"] = copy.deepcopy(source["enumDeprecated"])
    if isinstance(source.get("required"), list):
        result["required"] = sorted(set(result.get("required", [])) | set(source["required"]))
    if "$ref" in result and len(result) > 1:
        # OpenAPI 3.0 Reference Objects ignore siblings; retain descriptions in allOf.
        result["allOf"] = [{"$ref": result.pop("$ref")}]
    return result


def methods(document):
    """Yield all declarations, including root and legacy resources."""
    yield from document.get("methods", {}).values()
    for resource in document.get("resources", {}).values():
        yield from methods(resource)


def normalize(document, source_sha256, selected=None):
    if document.get("discoveryVersion") != "v1" or document.get("kind") != "discovery#restDescription":
        raise ValueError("only Google Discovery v1 REST descriptions are supported")
    declarations = sorted(methods(document), key=lambda m: m["id"])
    if selected is not None:
        missing = set(selected) - {m["id"] for m in declarations}
        if missing:
            raise ValueError(f"unknown selected methods: {sorted(missing)}")
        declarations = [m for m in declarations if m["id"] in selected]
    result = {
        "openapi": "3.0.3",
        "info": {"title": document["title"] + " (Qore-normalized Discovery)", "version": document["version"],
                 "description": "Generated from a pinned Google Discovery document; not official Google OpenAPI."},
        "servers": [{"url": "/"}],
        "x-qore-google-discovery": {"boundary_version": BOUNDARY_VERSION, "revision": document["revision"],
                                     "source_sha256": source_sha256},
        "paths": {},
        "components": {"schemas": {k: schema(v) for k, v in sorted(document["schemas"].items())}},
    }
    for method in declarations:
        template = method.get("flatPath", method["path"])
        if re.search(r"\{[^A-Za-z_]|\{[^}]*[+*=]", template):
            raise ValueError(f"method {method['id']} requires a flatPath with simple segment variables")
        path = "/" + template.lstrip("/")
        parameters = []
        original = method.get("parameters", {})
        for name in re.findall(r"\{([A-Za-z_][A-Za-z0-9_]*)\}", template):
            param = original.get(name, {"type": "string"})
            parameters.append({"name": name, "in": "path", "required": True,
                               "description": description(param.get("description", f"Resource identifier for `{name}`.")),
                               "schema": schema(param) | {"minLength": 1}})
        for name, param in sorted(original.items()):
            if param.get("location") == "path":
                continue
            if param.get("location") != "query":
                raise ValueError(f"unsupported parameter location in {method['id']}")
            converted = schema(param)
            if param.get("repeated"):
                converted = {"type": "array", "items": converted}
            parameters.append({"name": name, "in": "query", "required": param.get("required", False),
                               "description": description(param.get("description", name)), "schema": converted})
        operation = {"operationId": method["id"], "description": method.get("description", method["id"]),
                     "parameters": parameters, "responses": {},
                     "x-google-uri-template": method["path"],
                     "x-google-path-parameters": {k: v for k, v in original.items() if v.get("location") == "path"}}
        if "request" in method:
            operation["requestBody"] = {"required": True,
                "content": {"application/json": {"schema": schema(method["request"])}}}
        response = {"description": "Successful response"}
        if "response" in method:
            response["content"] = {"application/json": {"schema": schema(method["response"])}}
        operation["responses"]["200"] = response
        target = result["paths"].setdefault(path, {})
        verb = method["httpMethod"].lower()
        if verb in target:
            raise ValueError(f"duplicate method/path: {verb} {path}")
        target[verb] = operation
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--methods", type=Path, help="JSON list of selected Discovery method IDs")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    source = args.source.read_bytes()
    selected = json.loads(args.methods.read_text()) if args.methods else None
    normalized = normalize(json.loads(source), hashlib.sha256(source).hexdigest(), selected)
    output = json.dumps(normalized, indent=2, sort_keys=True, ensure_ascii=False) + "\n"
    if args.check:
        if args.output.read_text() != output:
            raise SystemExit("normalized Discovery artifact is stale")
    else:
        args.output.write_text(output)


if __name__ == "__main__":
    main()
