#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Retain complete source-embedded notices as license data without implementation."""
from pathlib import Path
import sys

NOTICES = {'linenoise': 'Redistribution and use in source and binary forms',
           'wcwidth': 'Permission to use, copy, modify, and distribute this software'}


def extract_notice(data, marker):
    if not data.startswith('/*') or '*/' not in data:
        raise ValueError('Expected a complete leading license comment')
    notice = data[:data.index('*/') + 2] + '\n'
    if marker not in notice:
        raise ValueError('Expected license grant is missing from the leading comment')
    return notice


def stage(source, output):
    source, output = Path(source), Path(output)
    notices = {name: extract_notice((source / 'modules/linenoise/src/linenoise' / (name + '.cpp')).read_text(), marker)
               for name, marker in NOTICES.items()}
    for name, notice in notices.items():
        (output / (name + '.LICENSE')).write_text(notice)


if __name__ == '__main__':
    stage(*sys.argv[1:])
