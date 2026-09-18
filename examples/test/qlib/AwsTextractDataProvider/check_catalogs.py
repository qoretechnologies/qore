#!/usr/bin/env python3
"""Qualify Textract catalog identity, protected syntax, and targeted nested translations."""
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT
import json
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[4] / "qlib/AwsTextractDataProvider/i18n/data-provider.QXdzVGV4dHJhY3Q"
LOCALES = ("cs", "de", "es", "fr", "it", "ja", "ko", "pl", "sk", "uk", "zh-Hant", "zh-TW")
TOKENS = re.compile(r"\x60[^\x60]+\x60|https?://[^\s)]+")
root = json.loads((ROOT / "root.json").read_text())["locales"]["root"]["messages"]
assert len(root) == 637, "Update the independently reviewed presentation inventory"
assert any("Custom adapters support" in item["source"] for item in root.values())
assert any("Free of Personally Identifiable Information" == item["source"] for item in root.values())
assert any("Page selectors such as \x601\x60" in item["source"] for item in root.values())
stems = dict(zip(LOCALES, ("Vlastní", "Benutzerdefinierte", "adaptadores", "adaptateurs",
    "adattatori", "カスタム", "어댑터", "adaptery", "Vlastné", "адаптери", "自訂", "自訂")))
for locale in LOCALES:
    messages = json.loads((ROOT / (locale + ".json")).read_text())["locales"][locale]["messages"]
    assert messages.keys() == root.keys(), locale + ": catalog identities differ"
    for key, expected in root.items():
        item = messages[key]
        assert item["source"] == expected["source"], (locale, key, "stale source")
        assert item["message"].strip(), (locale, key, "empty translation")
        assert not re.search(r"ZXQ\d+N\d+QXZ", item["message"]), (locale, key, "unexpanded token")
        assert sorted(TOKENS.findall(item["message"])) == sorted(TOKENS.findall(item["source"])), (
            locale, key, "code or URL changed")
        if item["source"] == "Custom adapters support the \x60QUERIES\x60 feature only.":
            assert stems[locale].casefold() in item["message"].casefold(), (locale, key, "query-only adapter description")
        if item["source"] in ("Free of Adult Content", "Free of Personally Identifiable Information", "Human Review",
                "Job Tag", "Dataset Config", "Manifest S3Object"):
            assert item["message"] != item["source"], (locale, key, "content choice lacks translation")
print(f"{len(root)} messages x {len(LOCALES)} locales: exact IDs/sources, protected syntax, nested regressions passed")
