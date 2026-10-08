#!/usr/bin/env python3
# Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
"""Verify ignored QPP parameters preserve metadata and execute real generated wrappers."""
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
BUILD = Path(os.environ.get("QORE_TEST_BUILD_DIR", ROOT / "build")).resolve()
INCLUDE = Path(os.environ.get("QORE_TEST_INCLUDE_DIR", ROOT / "include")).resolve()
QPP = Path(os.environ.get("QORE_QPP_EXECUTABLE", BUILD / "qpp")).resolve()
CASES = {
    "QC_AbstractDatasource": ("AbstractDatasource_", ("bulkLoadBegin", "bulkLoadRows", "bulkLoadEnd")),
    "QC_AsyncIoController": ("AsyncIoController_", ("constructor",)),
    "Pseudo_QC_Binary": ("PseudoBinary_", ("toBase64Url",)),
    "Pseudo_QC_String": ("PseudoString_", ("toBase64Url",)),
}


def function(text, name):
    match = re.search(r"^static [^\n]+\b" + re.escape(name) + r"[^\n]*\{\n.*?^\}", text, re.M | re.S)
    if not match:
        raise AssertionError("missing generated function " + name)
    return match.group(0)


class IgnoredQppParametersTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not QPP.is_file():
            raise RuntimeError("Build qpp or set QORE_QPP_EXECUTABLE")
        destination = os.environ.get("QORE_TEST_OUTPUT_DIR")
        if destination:
            cls.output = Path(destination).resolve()
            cls.output.mkdir(parents=True, exist_ok=True)
        else:
            cls.temporary = tempfile.TemporaryDirectory(prefix="qore-ignored-qpp-")
            cls.addClassCleanup(cls.temporary.cleanup)
            cls.output = Path(cls.temporary.name)
        cls.generated = {}
        cls.receipts = []
        for variant in ("baseline", "fixed"):
            directory = cls.output / variant
            directory.mkdir()
            wrappers = []
            for stem, (prefix, names) in CASES.items():
                text = (ROOT / "lib" / (stem + ".qpp")).read_text()
                if variant == "baseline":
                    # Restore only the affected signatures, leaving other documented arguments intact.
                    for name in names:
                        text, count = re.subn(r"^(?:(?:bool|nothing|string) )?(?:AbstractDatasource|AsyncIoController|<binary>|<string>)::" + name + r"\([^\n]*",
                            lambda match: match.group(0).replace("[doc]", ""), text, count=1, flags=re.M)
                        if count != 1:
                            raise AssertionError("missing source declaration " + name)
                src = directory / (stem + ".qpp")
                src.write_text(text)
                command = [str(QPP), "--output=" + str(directory / (stem + ".cpp")),
                    "--metadata=" + str(directory / (stem + ".json")),
                    "--stub-output=" + str(directory / (stem + ".stub.qc")),
                    "--dox-output=" + str(directory / (stem + ".dox.h")),
                    "--file-prefix-map=" + str(directory) + "=/fixture", str(src)]
                cls.invoke(variant + "-" + stem, command)
                cpp = (directory / (stem + ".cpp")).read_text()
                cls.generated[variant, stem] = cpp
                if stem == "Pseudo_QC_String":
                    wrappers.append(function(cpp, "get_pseudo_string_arg"))
                wrappers.extend(function(cpp, prefix + name) for name in names)
            (directory / "ignored-bindings.inc").write_text("\n\n".join(wrappers) + "\n")

    @classmethod
    def invoke(cls, name, command, success=True):
        result = subprocess.run(command, text=True, capture_output=True)
        (cls.output / (name + ".stdout")).write_text(result.stdout)
        (cls.output / (name + ".stderr")).write_text(result.stderr)
        cls.receipts.append({"name": name, "command": command, "exit_code": result.returncode})
        (cls.output / "commands.json").write_text(json.dumps(cls.receipts, indent=2) + "\n")
        if success and result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        return result

    def test_parameter_metadata_and_stubs_are_unchanged(self):
        for stem in CASES:
            with self.subTest(source=stem):
                for suffix in (".json", ".stub.qc"):
                    a = (self.output / "baseline" / (stem + suffix)).read_text()
                    b = (self.output / "fixed" / (stem + suffix)).read_text()
                    self.assertEqual(a, b, "public parameter names/types/defaults/flags must remain identical")
                # Native method registration also controls runtime argument checking and named dispatch.
                a = [line for line in self.generated['baseline', stem].splitlines()
                     if re.search(r'->add(?:ConstMethod|Method|Constructor)\(', line)]
                b = [line for line in self.generated['fixed', stem].splitlines()
                     if re.search(r'->add(?:ConstMethod|Method|Constructor)\(', line)]
                self.assertTrue(a)
                self.assertEqual(a, b)

    def test_documentation_is_unchanged(self):
        for stem in CASES:
            a = (self.output / "baseline" / (stem + ".dox.h")).read_text()
            b = (self.output / "fixed" / (stem + ".dox.h")).read_text()
            # Native debug source paths deliberately retain the actual temporary directory.
            self.assertEqual(a.replace(str(self.output / 'baseline'), '/fixture'),
                             b.replace(str(self.output / 'fixed'), '/fixture'))

    def test_actual_wrappers_compile_and_run(self):
        compiler = shlex.split(os.environ.get("CXX", "c++"))
        flags = shlex.split(os.environ.get("QORE_TEST_CXXFLAGS", "-O3 -DNDEBUG -g"))
        common = [*compiler, *flags, "-std=c++20", "-Wall", "-Werror",
            "-I" + str(INCLUDE), "-I" + str(BUILD / "include"), "-I" + str(BUILD)]
        fixture = ROOT / "examples/test/cmake/ignored_qpp_parameters.cpp"
        bad = self.invoke('baseline-compile', [*common, '-I' + str(self.output / 'baseline'),
            '-c', str(fixture), '-o', str(self.output / 'baseline.o')], success=False)
        self.assertNotEqual(0, bad.returncode)
        self.assertEqual(8, bad.stderr.count('error: unused variable'), bad.stderr)
        self.assertEqual(8, bad.stderr.count('error:'), bad.stderr)
        binary = self.output / 'fixed-wrappers'
        result = self.invoke('fixed-compile', [*common, '-I' + str(self.output / 'fixed'), str(fixture),
            '-L' + str(BUILD), '-Wl,-rpath,' + str(BUILD), '-lqore', '-o', str(binary)])
        self.assertEqual('', result.stderr)
        result = self.invoke('fixed-native', [str(binary)])
        self.assertEqual('PASS: 177 generated-wrapper checks\n', result.stdout)
        self.assertEqual('', result.stderr)
        if os.environ.get('QORE_TEST_VALGRIND') == '1':
            result = self.invoke('fixed-valgrind', ['valgrind', '--error-exitcode=99', '--leak-check=full',
                '--show-leak-kinds=definite,indirect,possible', '--errors-for-leak-kinds=definite,indirect,possible',
                str(binary)])
            self.assertIn('ERROR SUMMARY: 0 errors', result.stderr)
            for kind in ('definitely', 'indirectly', 'possibly'):
                self.assertRegex(result.stderr, kind + r' lost:\s+0 bytes in 0 blocks')


if __name__ == '__main__':
    unittest.main()
