# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("qore_requires", Path(__file__).resolve().parents[1] / "qore-requires.py")
requires = importlib.util.module_from_spec(spec)
spec.loader.exec_module(requires)


class RequirementsTest(unittest.TestCase):
    def test_fedora_runtime_floor_includes_epoch(self):
        self.assertEqual(requires.requirements(
            "libqore(x86-64) = 1:3.0.0-2.fc44\nopenssl-devel(x86-64)\n",
            "1:3.0.0-2.fc44", "(x86-64)", "2.0"),
            ["qore-module(abi)(x86-64) = 2.0", "qore(x86-64) >= 1:3.0.0-2.fc44",
             "libqore(x86-64) >= 1:3.0.0-2.fc44"])

    def test_suse_versioned_library_and_aarch64(self):
        result = requires.requirements("libqore20(aarch-64) = 3.0.0~git20261001.1-1.lp160",
                                       "0:3.0.0~git20261001.1-1.lp160", "(aarch-64)", "2.0")
        self.assertEqual(result[-1], "libqore20(aarch-64) >= 0:3.0.0~git20261001.1-1.lp160")

    def test_missing_or_ambiguous_runtime_fails(self):
        for value in ["", "libqore(x86-32) = 3.0.0-1", "libqore(x86-64) >= 3.0.0-1",
                      "libqore(x86-64) = 3.0.0-1\nlibqore20(x86-64) = 3.0.0-1"]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                requires.requirements(value, "0:3.0.0-1", "(x86-64)", "2.0")

    def test_invalid_inputs_fail(self):
        for evr, isa, api in [("3.0.0-1", "(x86-64)", "2.0"),
                              ("0:3.0.0-1", "", "2.0"),
                              ("0:3.0.0-1", "(x86-64)", "")]:
            with self.subTest(evr=evr, isa=isa, api=api), self.assertRaises(ValueError):
                requires.requirements("libqore(x86-64) = 3.0.0-1", evr, isa, api)


if __name__ == "__main__":
    unittest.main()
