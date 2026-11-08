#!/usr/bin/env python3
"""build-perf.sh builds only what the performance tests need: perf-build-targets.py gives qore and the targets of the
modules that the tests require, also through the test modules they require by path and the files they include.

These tests check the modules found for the real performance tests and for fixture tests with each form of
requirement, the targets of qlib and binary modules, and that build-perf.sh selects its tests as test-perf.sh runs
them (run_tests.sh -P).

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import importlib.util
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location("perf_build_targets",
                                              ROOT / "examples/test/docker_test/perf-build-targets.py")
targets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(targets)


def perf_tests():
    out = subprocess.run(["sh", str(ROOT / "run_tests.sh"), "-P", "-l"], cwd=ROOT, check=True, capture_output=True,
                         text=True, env={"PATH": "/usr/bin:/bin"}).stdout
    return [str(ROOT / line) for line in out.split()]


class PerfBuildTargetsTest(unittest.TestCase):
    def test_modules_of_the_perf_tests(self):
        tests = perf_tests()
        self.assertGreaterEqual(len(tests), 8)
        modules = targets.modules_of(tests)
        # by name, by path to a qlib module (DbTestConnections.qm requires ../../qlib/... by path), a binary module,
        # and the modules of the test modules required by path (TestCertificates.qm, ExecModeHarness.qm)
        for name in ("QUnit", "HttpServer", "WebSocketHandler", "DataProvider", "json", "TableMapper", "FsUtil"):
            self.assertIn(name, modules)
        # every module the tests name in their own lines is found
        for test in tests:
            for kind, name in re.findall(r"(?m)^%(requires|try-module)\s+([A-Za-z]\w*)\s*$",
                                         Path(test).read_text()):
                self.assertIn(name, modules, "%s of %s" % (name, test))

    def test_forms_of_requirement(self):
        # outside the source tree, which the tests may not write
        with tempfile.TemporaryDirectory(prefix="qore-perf-targets-") as directory:
            root = Path(directory).resolve()
            qlib = os.path.relpath(ROOT / "qlib", root)
            (root / "sub").mkdir()
            (root / "Helper.qm").write_text("%requires Mime\n%requires ./sub/Deep.qm\n")
            (root / "sub/Deep.qm").write_text("%requires(reexport) Logger\n")
            (root / "inc.qc").write_text("%requires WebSocketUtil\n")
            test = root / "t.qtest"
            test.write_text("%modern\n%requires QUnit\n%requires Util >= 1.0\n%try-module process\n"
                            "%requires ./Helper.qm\n%include inc.qc\n"
                            + "%requires ${SCRIPT_DIR}/" + qlib + "/DataProvider\n"
                            + "%requires " + qlib + "/FsUtil.qm\n"
                            + "# %requires NotThis\n")
            self.assertEqual({"QUnit", "Util", "process", "Mime", "Logger", "WebSocketUtil", "DataProvider",
                              "FsUtil"}, targets.modules_of([test]))

    def test_targets(self):
        self.assertEqual("QUnit-qmod", targets.target_of("QUnit"))
        self.assertEqual("DataProvider-qmod", targets.target_of("DataProvider"))
        self.assertEqual("json", targets.target_of("json"))
        # a module from the image (not in this tree) has no target
        self.assertIsNone(targets.target_of("process"))
        self.assertIsNone(targets.target_of("xml"))

    def test_build_perf_selects_the_tests_test_perf_runs(self):
        build = (ROOT / "examples/test/docker_test/build-perf.sh").read_text()
        test = (ROOT / "examples/test/docker_test/test-perf.sh").read_text()
        # as the test-perf job sets them, whatever the job's own variables (QORE_EXCLUDE_PERF_TESTS=1 is global)
        self.assertIn('PERF_TESTS=$(QORE_EXCLUDE_PERF_TESTS="" QORE_PERF_TESTS_ONLY=1 ./run_tests.sh -l)', build)
        ci = (ROOT / ".gitlab-ci.yml").read_text()
        perf_job = re.search(r"(?ms)^test-perf:\n.*?(?=^\S)", ci).group(0)
        self.assertRegex(perf_job, r'QORE_PERF_TESTS_ONLY: "1"')
        self.assertRegex(perf_job, r'QORE_EXCLUDE_PERF_TESTS: ""')
        # the selection works in the environment of a job with the global variables
        out = subprocess.run(["sh", "-c", 'QORE_EXCLUDE_PERF_TESTS="" QORE_PERF_TESTS_ONLY=1 ./run_tests.sh -l'],
                             cwd=ROOT, capture_output=True, text=True,
                             env={"PATH": "/usr/bin:/bin", "QORE_EXCLUDE_PERF_TESTS": "1"})
        self.assertEqual(0, out.returncode, out.stderr)
        self.assertEqual(sorted(perf_tests()), sorted(str(ROOT / line) for line in out.stdout.split()))
        self.assertRegex(build, r"perf-build-targets\.py build \$\{PERF_TESTS\}\)")
        self.assertRegex(build, r"make -j\$\{MAKE_JOBS\} \$\{PERF_TARGETS\}")
        self.assertIn("./run_tests.sh -P ", test)
        # the partial build is not installed
        self.assertNotRegex(test, r"(?m)^\s*cmake --install")


if __name__ == "__main__":
    unittest.main()
