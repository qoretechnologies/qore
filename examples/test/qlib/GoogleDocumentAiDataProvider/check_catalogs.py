#!/usr/bin/env python3
"""Check recursive catalog parity, literal syntax, and reviewed translations.

Copyright 2026 Qore Technologies, s.r.o.; MIT license (see COPYING.MIT).
"""
import argparse
import collections
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[4]
CATALOGS = ROOT / "qlib/GoogleDocumentAiDataProvider/i18n/data-provider.R29vZ2xlRG9jdW1lbnRBaQ"
LOCALES = "cs de es fr it ja ko pl sk uk zh-Hant zh-TW".split()
CODE = re.compile(r"`[^`]*`")
URL = re.compile(r'''https?://[^\s)<>「」。，；：、"'`]+''')
LINK = re.compile(r"\[([^]\n]+)\]\s*\((https?://[^)]+)\)")
PLACEHOLDER = re.compile(r"\{\{[^}]+\}\}|\{[A-Za-z_][A-Za-z0-9_]*\}|%[sd]")
WAIT = "Počkat na dokončení operace|Auf Abschluss des Vorgangs warten|Esperar a que finalice la operación|Attendre la fin de l’opération|Attendi il completamento dell’operazione|処理の完了を待機|작업 완료 대기|Poczekaj na zakończenie operacji|Počkať na dokončenie operácie|Дочекатися завершення операції|等待作業完成|等待作業完成".split("|")
MONEY = "Peněžní hodnota|Geldbetrag|Valor monetario|Valeur monétaire|Valore monetario|金額|금액|Wartość pieniężna|Peňažná hodnota|Грошова сума|金額|金額".split("|")


def messages(path, locale):
    return json.loads(path.read_text())["locales"][locale]["messages"]


def urls(text):
    return collections.Counter(value.rstrip(".,;:") for value in URL.findall(CODE.sub("", text)))


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--fresh", type=Path, help="fresh recursive extraction root.json")
args = parser.parse_args()
root = messages(CATALOGS / "root.json", "root")
assert len(root) == 4963, "review recursive inventory changes before updating the count"
assert {p.stem for p in CATALOGS.glob("*.json")} == {"root", *LOCALES}
if args.fresh:
    assert root == messages(args.fresh, "root"), "committed root differs from fresh recursive extraction"
for locale, wait, money in zip(LOCALES, WAIT, MONEY):
    translated = messages(CATALOGS / (locale + ".json"), locale)
    assert translated.keys() == root.keys(), (locale, "catalog identity mismatch")
    by_source = {}
    for key, expected in root.items():
        item = translated[key]
        source, value = item["source"], item["message"]
        assert source == expected["source"] and value.strip(), (locale, key, "stale or empty")
        assert collections.Counter(CODE.findall(source)) == collections.Counter(CODE.findall(value)), (locale, key, "code")
        assert urls(source) == urls(value), (locale, key, "URL")
        assert collections.Counter(PLACEHOLDER.findall(source)) == collections.Counter(PLACEHOLDER.findall(value)), (locale, key, "placeholder")
        assert len(LINK.findall(source)) == len(LINK.findall(value)), (locale, key, "Markdown link")
        assert source.count("**") == value.count("**"), (locale, key, "emphasis")
        assert source.count("\n") == value.count("\n"), (locale, key, "paragraph/list structure")
        assert not re.search(r"ZXQ|LITERAL\d+XZ", value), (locale, key, "draft marker")
        by_source[source] = value
    assert by_source["Wait for Operation"] == wait, locale
    assert by_source["Money Value"] == money, locale
    # These specific UI concepts require translated prose; HTTP methods, MIME
    # labels, brands, and valid cognates are intentionally allowed to match English.
    for source in ("Omit Document Images", "Text Anchor", "Dataset Split", "Image Quality Scores"):
        assert by_source[source] != source, (locale, source)
print(f"{len(root)} messages x {len(LOCALES)} locales: exact IDs/sources, protected syntax, and targeted translations passed")
