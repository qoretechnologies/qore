#!/usr/bin/env python3
"""Check Stability AI catalog parity, syntax, and reviewed terminology.

Copyright 2026 Qore Technologies, s.r.o.; MIT license (see COPYING.MIT).
"""
import argparse
import collections
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[4]
CATALOGS = ROOT / "qlib/StabilityAiDataProvider/i18n/data-provider.U3RhYmlsaXR5QWk"
LOCALES = "cs de es fr it ja ko pl sk uk zh-Hant zh-TW".split()
CODE = re.compile(r"`[^`]*`")
URL = re.compile(r'''https?://[^\s)<>「」。，；：、"'`]+''')
LINK = re.compile(r"\[([^]\n]+)\]\s*\((https?://[^)]+)\)")
PLACEHOLDER = re.compile(r"\{\{[^}]+\}\}|\{[A-Za-z_][A-Za-z0-9_]*\}|%[sd]")
WAIT = "Počkat na dokončení obrazové úlohy|Auf Abschluss des Bildauftrags warten|Esperar a que termine la tarea de imagen|Attendre la fin de la tâche d’image|Attendi il completamento dell’attività immagine|画像ジョブの完了を待機|이미지 작업 완료 대기|Poczekaj na zakończenie zadania obrazu|Počkať na dokončenie obrazovej úlohy|Дочекатися завершення завдання зображення|等待影像作業完成|等待影像作業完成".split("|")
FACES = "Stěny sítě|Netzflächen|Caras de la malla|Faces du maillage|Facce della mesh|メッシュの面|메시 면|Ściany siatki|Steny siete|Грані сітки|網格面|網格面".split("|")


def messages(path, locale):
    return json.loads(path.read_text())["locales"][locale]["messages"]


def urls(text):
    return collections.Counter(value.rstrip(".,;:") for value in URL.findall(CODE.sub("", text)))


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--fresh", type=Path, help="fresh recursive extraction root.json")
args = parser.parse_args()
root = messages(CATALOGS / "root.json", "root")
assert len(root) == 2416, "review recursive inventory changes before updating the count"
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
        by_source[source] = value
    assert by_source["Wait for Image Job"] == wait, locale
    assert by_source["Mesh Faces"] == faces, locale
    for source in ("Prompt Guidance", "Increase image resolution for reuse in larger layouts.",
                   "Keep Original Background", "Light Source Strength"):
        assert by_source[source] != source, (locale, source)
print(f"{len(root)} messages x {len(LOCALES)} locales: exact IDs/sources, protected syntax, and targeted translations passed")
