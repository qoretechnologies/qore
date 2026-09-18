#!/usr/bin/env python3
"""Check exact catalog parity, protected syntax, and reviewed translation regressions.

Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT
"""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
CATALOGS = ROOT / "qlib/AzureDocumentIntelligenceDataProvider/i18n/data-provider.QXp1cmVEb2N1bWVudEludGVsbGlnZW5jZQ"
LOCALES = "cs de es fr it ja ko pl sk uk zh-Hant zh-TW".split()
OPERATIONS = "Získat stav operace|Vorgang abrufen|Obtener operación|Récupérer l’opération|Recupera operazione|操作を取得|작업 조회|Pobierz operację|Získať stav operácie|Отримати операцію|取得作業|取得作業".split("|")
FIGURES = "Obrázky|Abbildungen|Imágenes|Illustrations|Immagini|図|그림|Ilustracje|Obrázky|Ілюстрації|圖片|圖片".split("|")
PROTECTED = re.compile(r"`[^`]+`|https?://[^\s)]+|\{[A-Za-z_][A-Za-z0-9_]*\}|%[sd]|\b(?:urlSource|base64Source|docTypes|classifierId|modelId)\b")

def messages(locale):
    return json.loads((CATALOGS / (locale + ".json")).read_text())["locales"][locale]["messages"]

root = messages("root")
assert len(root) == 773, "review inventory changes and update the expected count"
assert set(p.stem for p in CATALOGS.glob("*.json")) == {"root", *LOCALES}
for locale, operation, figures in zip(LOCALES, OPERATIONS, FIGURES):
    translated = messages(locale)
    assert translated.keys() == root.keys(), locale
    by_source = {}
    for key, source in root.items():
        item = translated[key]
        assert item["source"] == source["source"] and item["message"].strip(), (locale, key)
        assert sorted(PROTECTED.findall(item["source"])) == sorted(PROTECTED.findall(item["message"])), (locale, key)
        assert item["source"].count("**") == item["message"].count("**"), (locale, key)
        assert item["source"].count("\n") == item["message"].count("\n"), (locale, key)
        by_source[item["source"]] = item["message"]
    assert by_source["Get Operation"] == operation, locale
    assert by_source["Figures"] == figures, locale
    # These are prose surrounding a product name, not untranslated product names.
    for source in ("Azure Blob File List Source", "Azure Blob Storage content", "File list in Azure Blob Storage",
                   "Read build or copy progress and its typed result", "Base Classifier ID"):
        assert by_source[source] != source, (locale, source)
print(f"{len(root)} messages x {len(LOCALES)} locales: exact IDs/sources, protected syntax, and translation regressions passed")
