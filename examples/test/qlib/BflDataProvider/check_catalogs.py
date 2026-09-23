#!/usr/bin/env python3
"""Check Black Forest Labs catalog parity, syntax, and reviewed terminology.

Copyright 2026 Qore Technologies, s.r.o.; MIT license (see COPYING.MIT).
"""
import argparse
import collections
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[4]
CATALOGS = ROOT / "qlib/BflDataProvider/i18n/data-provider.QmxhY2tGb3Jlc3RMYWJz"
LOCALES = "cs de es fr it ja ko pl sk uk zh-Hant zh-TW".split()
CODE = re.compile(r"`[^`]*`")
URL = re.compile(r'''https?://[^\s)<>「」。，；：、"'`]+''')
LINK = re.compile(r"\[([^]\n]+)\]\s*\((https?://[^)]+)\)")
PLACEHOLDER = re.compile(r"\{\{[^}]+\}\}|\{[A-Za-z_][A-Za-z0-9_]*\}|%[sd]")
WAIT = 'Počkat na výsledek|Auf Ergebnis warten|Esperar el resultado|Attendre le résultat|Attendi il risultato|結果を待機|결과 대기|Poczekaj na wynik|Počkať na výsledok|Дочекатися результату|等待結果|等待結果'.split("|")
FACES = 'Oříznout podle plátna|Auf Leinwand zuschneiden|Recortar al lienzo|Recadrer sur le canevas|Ritaglia sulla tela|キャンバスに合わせて切り抜き|캔버스에 맞게 자르기|Przytnij do płótna|Orezať podľa plátna|Обрізати до полотна|裁切至畫布|裁切至畫布'.split("|")


def messages(path, locale):
    return json.loads(path.read_text())["locales"][locale]["messages"]


def urls(text):
    return collections.Counter(value.rstrip(".,;:") for value in URL.findall(CODE.sub("", text)))


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--fresh", type=Path, help="fresh recursive extraction root.json")
args = parser.parse_args()
root = messages(CATALOGS / "root.json", "root")
assert len(root) == 4993, "review recursive inventory changes before updating the count"
assert {p.stem for p in CATALOGS.glob("*.json")} == {"root", *LOCALES}
if args.fresh:
    assert root == messages(args.fresh, "root"), "root differs from fresh recursive extraction"
for locale, wait, faces in zip(LOCALES, WAIT, FACES):
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
        assert len(re.findall(r"\n\s*\n", source)) == len(re.findall(r"\n\s*\n", value)), (locale, key, "paragraphs")
        assert len(re.findall(r"^\s*[-*] ", source, re.M)) == len(re.findall(r"^\s*[-*] ", value, re.M)), (locale, key, "lists")
        assert not re.search(r"ZXQ|LITERAL\d+XZ", value), (locale, key, "draft marker")
        assert value != source or not re.search(r"[a-z]+ [a-z]+", source), (locale, key, "copied English prose")
        by_source[source] = value
    assert by_source["Wait for Result"] == wait, locale
    assert by_source["Crop to Canvas"] == faces, locale
    billing = next(value for source, value in by_source.items()
                   if source.startswith("Use [Black Forest Labs]"))
    assert "**" + by_source["Get Credit Balance"] + "**" in billing, (locale, "billing action label")
    for source in ("Keyframes", "Creative Detail", "Maximum Download Size", "Draft Cache"):
        assert by_source[source] != source, (locale, source)
print(f"{len(root)} messages x {len(LOCALES)} locales: exact IDs/sources, protected syntax, and targeted translations passed")
