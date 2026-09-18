#!/usr/bin/env python3
# Copyright 2026 Qore Technologies, s.r.o.
"""Check source parity, protected content, and reviewed AssemblyAI translations.

Pass a freshly extracted root.json to also compare extraction with the shipped root.
"""
import collections
import json
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parents[4]
CATALOG = REPO / 'qlib/AssemblyAiDataProvider/i18n/data-provider.QXNzZW1ibHlBSQ'
LOCALES = ('cs', 'de', 'es', 'fr', 'it', 'ja', 'ko', 'pl', 'sk', 'uk', 'zh-Hant', 'zh-TW')
PROTECTED = re.compile(r'`[^`]*`|https?://[^\s)>]+|\{[A-Za-z0-9_.-]+\}|\$[A-Za-z_][A-Za-z0-9_]*')
PRODUCTS = ('AssemblyAI', 'Universal-3.5 Pro', 'Universal-2', 'universal-3-5-pro',
            'json-repair', 'json_schema', 'medical-v1', 'mp3', 'wav', 'WebVTT', 'SubRip (SRT)')
SIGNING = ('Tajný podpisový klíč', 'Signaturschlüssel', 'Secreto de firma', 'Secret de signature',
           'Segreto di firma', '署名シークレット', '서명 시크릿', 'Klucz tajny podpisywania',
           'Tajný podpisový kľúč', 'Секретний ключ підпису', '簽章密鑰', '簽章密鑰')


def messages(path, locale):
    return json.loads(path.read_text(encoding='utf-8'))['locales'][locale]['messages']


root = messages(CATALOG / 'root.json', 'root')
if len(sys.argv) > 1:
    assert root == messages(pathlib.Path(sys.argv[1]), 'root'), 'fresh root differs from shipped root'
for locale, signing in zip(LOCALES, SIGNING):
    entries = messages(CATALOG / (locale + '.json'), locale)
    assert entries.keys() == root.keys(), (locale, 'ID parity')
    for key, entry in entries.items():
        source, text = entry['source'], entry['message']
        assert source == root[key]['source'], (locale, key, 'source parity')
        assert text.strip(), (locale, key, 'empty translation')
        assert collections.Counter(PROTECTED.findall(source)) == collections.Counter(PROTECTED.findall(text)), \
            (locale, key, 'code, URL, or placeholder changed')
        assert source.count('**') == text.count('**'), (locale, key, 'Markdown emphasis changed')
        assert source.count('AssemblyAI') == text.count('AssemblyAI'), (locale, key, 'product name changed')
        if source in PRODUCTS:
            assert text == source, (locale, key, 'technical literal changed')
        if source == 'Signing Secret':
            assert text == signing, (locale, key, 'reviewed signing label changed')
        if source in ('Disfluencies', 'Redact PII Return Unredacted', 'Redact PII Sub',
                      'Create Voice Agent', 'Completed Response'):
            assert text != source, (locale, key, 'untranslated UI label')
    print(f'{locale}: {len(entries)} messages; exact parity and translation regressions passed')
