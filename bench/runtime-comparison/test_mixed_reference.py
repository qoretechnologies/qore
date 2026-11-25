#!/usr/bin/env python3
# Copyright (c) 2026 Qore Technologies, s.r.o. -- SPDX-License-Identifier: MIT
"""Check hand-calculated mixed-workload cases and reference-output validation."""
import unittest

from mixed_reference import KERNELS, reference, validate_checks


class MixedReferenceTests(unittest.TestCase):
    def output(self):
        return '\n'.join(f'CHECK\t{name}\t{count}\t{seed}\t{reference(name, count, seed)}'
            for name in KERNELS for count in (0, 1, 17, 1024, 4096) for seed in (0, 7, 65535)) + '\n'

    def test_empty(self):
        # Empty reports contain sixteen "key=0;" fields, of total length 70.
        for seed in (0, 7, 65535):
            self.assertEqual([reference(name, 0, seed) for name in KERNELS], [70, 70, 0, 0])

    def test_single_record(self):
        # First input is 248. At seed7, CSV amount255 is selected. Order subtotal
        # 255 discounts to224, taxes to238, and contributes238*9 +17 +72.
        self.assertEqual([reference(name, 1, 7) for name in KERNELS], [65864, 2231, 16148, 208])

    def test_wrapping_seed(self):
        self.assertEqual(reference('csv_report', 1, 65535), 65856)
        self.assertEqual(reference('object_orders', 1, 65535), 2159)

    def test_complete_grid(self):
        validate_checks(self.output())

    def test_missing_record(self):
        with self.assertRaises(AssertionError):
            validate_checks('\n'.join(self.output().splitlines()[1:]))

    def test_duplicate_record(self):
        with self.assertRaises(AssertionError):
            validate_checks(self.output() + self.output().splitlines()[0] + '\n')

    def test_wrong_value(self):
        with self.assertRaises(AssertionError):
            validate_checks(self.output().replace('\t70\n', '\t71\n', 1))

    def test_unknown_workload(self):
        with self.assertRaises(ValueError):
            reference('unknown', 1, 7)


if __name__ == '__main__':
    unittest.main()
