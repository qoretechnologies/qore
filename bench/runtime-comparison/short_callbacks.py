#!/usr/bin/env python3
# Copyright 2026 Qore Technologies, s.r.o. -- SPDX-License-Identifier: MIT
"""Exact integer/rational references for batched short callback workloads."""
from pathlib import Path
import sys

KINDS = ('int_zero', 'int_unary', 'float_zero', 'float_unary')
KERNELS = tuple(kind + '_' + str(inner) for kind in KINDS for inner in (1, 17)) + tuple(
    kind + '_inline' for kind in KINDS)


def short_result(kind, inner, seed):
    if kind == 'int_zero':
        return inner * (17 if seed % 2 else 19)
    if kind == 'float_zero':
        return inner // 2 if seed % 2 else inner
    if kind == 'float_unary':
        doubled, total = 2 * seed, 0
        for _ in range(inner):
            doubled = (131071 if seed % 2 else 131069) - doubled
            total += doubled
        return total
    if kind != 'int_unary':
        raise ValueError('unknown short callback kind')
    value, total = seed, 0
    for _ in range(inner):
        value = (33 * value + (17 if seed % 2 else 19)) % 65536
        total += value
    return total


def expected_checks():
    lines = []
    for count in (0, 1, 17, 257, 4096):
        for initial in (0, 7, 65535):
            for name in KERNELS:
                kind, suffix = name.rsplit('_', 1)
                inner = 1 if suffix == 'inline' else int(suffix)
                checksum, seed = 0, initial
                for _ in range(count):
                    seed = (seed + 1) % 65536
                    checksum = (checksum * 33 + short_result(kind, inner, seed)) % 65536
                lines.append(f'CHECK\t{name}\t{count}\t{initial}\t{checksum}\n')
    return ''.join(lines)


def validate_checks(output):
    if output != expected_checks():
        raise ValueError('batched short callback results differ from the exact reference')


if __name__ == '__main__':
    if len(sys.argv) == 1:
        sys.stdout.write(expected_checks())
    elif len(sys.argv) == 2:
        validate_checks(Path(sys.argv[1]).read_text())
        print('Verified 180 short callback reference results')
    else:
        raise SystemExit('usage: short_callbacks.py [CHECK_OUTPUT]')
