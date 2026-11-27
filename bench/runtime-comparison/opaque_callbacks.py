#!/usr/bin/env python3
# Copyright 2026 Qore Technologies, s.r.o. -- SPDX-License-Identifier: MIT
"""Independent exact references for opaque_callbacks.qr; validate a check-output file."""
import sys
from pathlib import Path


def expected_checks():
    rows = []
    for count in (0, 1, 17, 257, 4096):
        for seed in (0, 7, 65535):
            increment = 17 if seed % 2 else 19
            value, total = seed, 0
            for _ in range(count):
                value = (33 * value + increment) % 65536
                total += value
            doubled_value, doubled_total = 2 * seed, 0
            for _ in range(count):
                doubled_value = (131071 if seed % 2 else 131069) - doubled_value
                doubled_total += doubled_value
            values = {
                'int_zero': increment * count,
                'int_unary': total,
                'float_zero': count // 2 if seed % 2 else count,
                'float_unary': doubled_total,
            }
            for name, result in values.items():
                rows.append(f'CHECK\t{name}\t{count}\t{seed}\t{result}\n')
    return ''.join(rows)


def validate_checks(output):
    if output != expected_checks():
        raise ValueError('opaque callback results differ from the exact reference')


if __name__ == '__main__':
    if len(sys.argv) == 1:
        sys.stdout.write(expected_checks())
    elif len(sys.argv) == 2:
        validate_checks(Path(sys.argv[1]).read_text())
        print('Verified 60 opaque callback reference results')
    else:
        raise SystemExit('usage: opaque_callbacks.py [CHECK_OUTPUT]')
