#!/usr/bin/env python3
"""Contract tests for the offline Google Discovery normalization boundary.

Copyright 2026 Qore Technologies, s.r.o.; MIT license (see COPYING.MIT).
"""
import copy
import importlib.util
from pathlib import Path
import unittest
import sys

sys.dont_write_bytecode = True

spec = importlib.util.spec_from_file_location("discovery", Path(__file__).with_name("google-discovery-openapi.py"))
discovery = importlib.util.module_from_spec(spec)
spec.loader.exec_module(discovery)
app_spec = importlib.util.spec_from_file_location("document_ai", Path(__file__).with_name("google-document-ai-import.py"))
document_ai = importlib.util.module_from_spec(app_spec)
app_spec.loader.exec_module(document_ai)


class NormalizeTest(unittest.TestCase):
    def test_complete_source_inventory_and_versioned_operations(self):
        artifacts = document_ai.artifacts()
        inventory = artifacts["google-document-ai-inventory.json"]
        self.assertEqual(90, len(inventory["methods"]))
        self.assertEqual({"v1", "v1beta3"}, set(inventory["sources"]))
        self.assertTrue(all(m["decision"] for m in inventory["methods"]))
        for version in ("v1", "v1beta3"):
            api = artifacts["google-document-ai-openapi-" + version + ".json"]
            self.assertTrue(all(p.startswith("/" + version + "/") for p in api["paths"]))
            for field in ("metadata", "response"):
                alternatives = api["components"]["schemas"]["GoogleLongrunningOperation"]["properties"][field]["anyOf"]
                for branch in alternatives[:-1]:
                    type_url = branch["properties"]["@type"]["enum"][0]
                    self.assertTrue(type_url.startswith("type.googleapis.com/google.cloud.documentai." + version + "."))
                    self.assertNotIn(".beta3", type_url)
            for path in api["paths"].values():
                if "patch" in path:
                    ref = path["patch"]["requestBody"]["content"]["application/json"]["schema"]["$ref"]
                    self.assertFalse(api["components"]["schemas"][ref.split("/")[-1]].get("required"))

    def test_wire_types_and_opaque_defaults(self):
        self.assertEqual({"type": "string", "x-google-format": "int64"},
                         discovery.schema({"type": "string", "format": "int64"}))
        self.assertEqual("byte", discovery.schema({"type": "string", "format": "byte"})["format"])
        self.assertEqual("date-time", discovery.schema({"type": "string", "format": "google-datetime"})["format"])
        self.assertEqual({"type": "object", "default": {"value": False, "$ref": "literal"}},
                         discovery.schema({"type": "object", "default": {"value": False, "$ref": "literal"}}))

    def test_choices_are_wire_values(self):
        original = {"type": "string", "enum": ["IMAGE_QUALITY", "OCR"],
                    "enumDescriptions": ["Image quality scores", "Optical character recognition"]}
        converted = discovery.schema(original)
        self.assertEqual(original["enum"], converted["enum"])
        self.assertEqual(["Image Quality", "OCR"], converted["x-enumNames"])
        self.assertEqual(original["enumDescriptions"], converted["x-enum-descriptions"])
        self.assertEqual(original["enum"], discovery.schema(original)["enum"])

    def test_required_readonly_and_reference_siblings(self):
        result = discovery.schema({"type": "object", "properties": {
            "name": {"type": "string", "description": "Required. Human-readable name."},
            "state": {"type": "string", "description": "Required. Output only. Current state."},
            "document": {"$ref": "Document", "description": "Input document"}}})
        self.assertEqual(["name"], result["required"])
        self.assertTrue(result["properties"]["state"]["readOnly"])
        self.assertEqual("Human-readable name.", result["properties"]["name"]["description"])
        self.assertEqual("Current state.", result["properties"]["state"]["description"])
        self.assertEqual({"allOf": [{"$ref": "#/components/schemas/Document"}], "description": "Input document"},
                         result["properties"]["document"])

    def test_unsupported_constructs_fail(self):
        for source in ({"type": "string", "mystery": True}, {"format": "future-format"},
                       {"$ref": "../../escape"}, {"enum": ["x"], "enumDescriptions": []}):
            with self.assertRaises(ValueError):
                discovery.schema(source)

    def test_presentation_preserves_contract_text(self):
        self.assertEqual("Resource `projects/{project}`.", discovery.description("Required. Resource `projects/{project}."))
        text = "Summary. " + "The contract remains intact. " * 25
        formatted = discovery.markdown_description(text)
        self.assertTrue(formatted.startswith("Summary.\n\n**Details**\n\n- "))
        self.assertEqual(text, formatted.replace("\n\n**Details**\n\n- ", " "))
        expression = "`alpha * (1.0 - color)`"
        self.assertIn(expression, discovery.markdown_description(text + expression))

    def test_expanded_paths_and_request_contract(self):
        document = {"discoveryVersion": "v1", "kind": "discovery#restDescription", "title": "Fixture",
                    "version": "v1", "revision": "1", "schemas": {"Document": {"type": "object"}},
                    "resources": {"documents": {"methods": {"get": {
                        "id": "fixture.documents.get", "httpMethod": "POST", "path": "v1/{+name}",
                        "flatPath": "v1/projects/{projectsId}/documents/{documentsId}",
                        "parameters": {"name": {"type": "string", "location": "path", "required": True},
                                       "pageToken": {"type": "string", "location": "query"}},
                        "request": {"$ref": "Document"}, "response": {"$ref": "Document"}}}}}}
        original = copy.deepcopy(document)
        converted = discovery.normalize(document, "checksum")
        operation = converted["paths"]["/v1/projects/{projectsId}/documents/{documentsId}"]["post"]
        self.assertEqual(["projectsId", "documentsId", "pageToken"], [p["name"] for p in operation["parameters"]])
        self.assertEqual("v1/{+name}", operation["x-google-uri-template"])
        self.assertTrue(operation["requestBody"]["required"])
        self.assertEqual(original, document)
        with self.assertRaises(ValueError):
            discovery.normalize(document, "checksum", ["missing"])
        del document["resources"]["documents"]["methods"]["get"]["flatPath"]
        with self.assertRaises(ValueError):
            discovery.normalize(document, "checksum")


if __name__ == "__main__":
    unittest.main()
