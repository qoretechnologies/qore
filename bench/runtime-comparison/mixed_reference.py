#!/usr/bin/env python3
# Copyright (c) 2026 Qore Technologies, s.r.o. -- SPDX-License-Identifier: MIT
"""Independent reference for the synthetic mixed-processing workloads."""
from collections import Counter

KERNELS = ('csv_report', 'object_orders', 'callback_pipeline', 'text_index')

def reference(name, count, seed):
    values = []
    value = 7
    for _ in range(count):
        value = (value * 33 + 17) & 65535
        values.append(value)
    if name == 'csv_report':
        totals = [0] * 16
        selected = 0
        for value in values:
            amount = (value + seed) & 65535
            if value % 5 and amount >= 128:
                totals[value % 16] += amount
                selected += 1
        report = ''.join(f'{i}={amount};' for i, amount in enumerate(totals))
        return sum(totals) + selected * 65537 + len(report)
    if name == 'object_orders':
        totals = [0] * 16
        for index, value in enumerate(values):
            amount = ((value + seed) & 65535) * (index % 4 + 1)
            amount -= amount >> (2 if index % 2 else 3)
            amount += amount >> 4
            totals[value % 16] += amount
        report = ''.join(f'{i}={amount};' for i, amount in enumerate(totals))
        return sum((i + 1) * amount for i, amount in enumerate(totals)) + count * 17 + len(report)
    if name == 'callback_pipeline':
        result = 0
        for value in values:
            if value % 3:
                value = ((value ^ seed) * 33 + 17) & 65535
                result += ((value * 33 + 19) & 65535) + 17
        return result
    if name == 'text_index':
        frequencies = Counter(part for value in values for part in
            (value % 128, (value >> 3) % 128, (value >> 6) % 128))
        report = ''.join(f'item_{i}={frequencies[i]};' for i in range(128) if frequencies[i])
        return sum(frequency * (i + seed + 1) for i, frequency in frequencies.items()) + len(report)
    raise ValueError(name)

def validate_checks(output):
    expected = {(name, count, seed): reference(name, count, seed)
        for name in KERNELS for count in (0, 1, 17, 1024, 4096) for seed in (0, 7, 65535)}
    actual = {}
    for line in output.splitlines():
        tag, name, count, seed, value = line.split('\t')
        key = (name, int(count), int(seed))
        assert tag == 'CHECK' and key not in actual, line
        actual[key] = int(value)
    assert actual == expected, [(key, expected[key], actual.get(key)) for key in expected if actual.get(key) != expected[key]]
