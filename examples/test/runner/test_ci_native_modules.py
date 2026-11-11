#!/usr/bin/env python3
"""The CI test jobs expose only the native modules that the qore under test loads.

test-ubuntu.sh, test-alpine.sh and test-macos.sh give the tests the native modules of the image through a link farm
(qore_ci_native_module_farm in examples/test/docker_test/ci-native-modules.sh).  A module built for a module API that
the qore under test does not load is refused by the loader, which reports it as not found; the tests that needed it
then failed instead of skipping, as in pipeline 64230, whose qore loaded module API 2.1 while the image's modules
were built for 2.0.  The farm leaves such a module out, so that it is absent, as on a host without it.

The cases run the function with a stand-in for qore that prints the module APIs it is given, and, when there is a
build (QORE_TEST_BUILD_DIR, or build/ in this tree), with the qore of the build.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
HELPER = ROOT / "examples/test/docker_test/ci-native-modules.sh"

BUILD_DIR = Path(os.environ.get("QORE_TEST_BUILD_DIR", str(ROOT / "build")))
LIBQORE = next((BUILD_DIR / name for name in ("libqore.so", "libqore.dylib") if (BUILD_DIR / name).exists()), None)
HAVE_QORE = (BUILD_DIR / "qore").exists() and LIBQORE is not None


class NativeModuleFarmTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="qore-ci-native-modules-")
        self.root = Path(self.tmp.name)
        self.farm = self.root / "farm"
        self.farm.mkdir()

    def tearDown(self):
        self.tmp.cleanup()

    def fake_qore(self, output, rc=0):
        """Returns a stand-in for qore whose --module-apis option prints the given text and exits with the given
        status"""
        path = self.root / "qore"
        path.write_text("#!/bin/sh\n"
                        "[ \"$1\" = \"--module-apis\" ] || { echo \"unexpected arguments: $*\" >&2; exit 2; }\n"
                        f"printf '%s' '{output}'\n"
                        f"exit {rc}\n")
        path.chmod(0o755)
        return path

    def module_dir(self, name, files):
        """Makes a module directory with the given (empty) files"""
        path = self.root / name
        path.mkdir()
        for f in files:
            (path / f).touch()
        return path

    def run_farm(self, qore, *dirs, shell="sh"):
        """Runs the function as the scripts do: sourced, under set -e"""
        return subprocess.run([shell, "-c", 'set -e; . "$1"; shift; qore_ci_native_module_farm "$@"; echo done',
                               shell, str(HELPER), str(qore), str(self.farm)] + [str(d) for d in dirs],
                              capture_output=True, text=True, timeout=60)

    def linked(self):
        return {p.name: os.readlink(p) for p in self.farm.iterdir()}

    def test_only_loaded_api(self):
        mods = self.module_dir("mods", ("pgsql-api-2.0.qmod", "pgsql-api-2.1.qmod", "xml-api-2.0.qmod",
                                        "yaml-api-2.1.qmod", "SqlUtil.qmod", "Util-1.0.qmod"))
        result = self.run_farm(self.fake_qore("2.1\n"), mods)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual({
            "pgsql-api-2.1.qmod": str(mods / "pgsql-api-2.1.qmod"),
            "yaml-api-2.1.qmod": str(mods / "yaml-api-2.1.qmod"),
        }, self.linked())
        # the modules left out are named with the module API that they were built for
        self.assertIn(f"-- {mods / 'pgsql-api-2.0.qmod'} is not used: it was built for module API 2.0 --",
                      result.stdout)
        self.assertIn(f"-- {mods / 'xml-api-2.0.qmod'} is not used: it was built for module API 2.0 --",
                      result.stdout)
        self.assertIn("-- native modules: this qore loads module API 2.1 --", result.stdout)
        # files that are not native modules are not mentioned
        self.assertNotIn("SqlUtil", result.stdout)
        # the summary gives the coverage of the run
        self.assertIn("-- native modules: 2 exposed, 2 left out (built for another module API): pgsql-api-2.0 "
                      "xml-api-2.0 --", result.stdout)
        self.assertTrue(result.stdout.endswith("done\n"), result.stdout)

    def test_more_than_one_api(self):
        mods = self.module_dir("mods", ("a-api-2.0.qmod", "b-api-2.1.qmod", "c-api-1.3.qmod", "d-api-2.10.qmod"))
        result = self.run_farm(self.fake_qore("2.1, 2.0\n"), mods)
        self.assertEqual(0, result.returncode, result.stderr)
        # 2.10 is not 2.1, and 1.3 is not loaded
        self.assertEqual({"a-api-2.0.qmod", "b-api-2.1.qmod"}, set(self.linked()))

    def test_first_directory_wins(self):
        first = self.module_dir("first", ("xml-api-2.1.qmod",))
        second = self.module_dir("second", ("xml-api-2.1.qmod", "zip-api-2.1.qmod", "uuid-api-2.0.qmod"))
        # an empty argument (an unset prefix) and a directory that does not exist are ignored
        result = self.run_farm(self.fake_qore("2.1"), "", first, self.root / "none", second)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual({
            "xml-api-2.1.qmod": str(first / "xml-api-2.1.qmod"),
            "zip-api-2.1.qmod": str(second / "zip-api-2.1.qmod"),
        }, self.linked())
        # a module linked from an earlier directory is neither exposed again nor left out
        self.assertIn("-- native modules: 2 exposed, 1 left out (built for another module API): uuid-api-2.0 --",
                      result.stdout)

    def test_name_with_api_in_it(self):
        mods = self.module_dir("mods", ("my-api-tool-api-2.1.qmod", "my-api-tool-api-2.0.qmod"))
        result = self.run_farm(self.fake_qore("2.1"), mods)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual({"my-api-tool-api-2.1.qmod"}, set(self.linked()))

    def test_no_modules(self):
        # an empty directory: no match of the glob is not a failure under set -e
        mods = self.module_dir("mods", ())
        for shell in ("sh", "bash"):
            with self.subTest(shell=shell):
                result = self.run_farm(self.fake_qore("2.1"), mods, shell=shell)
                self.assertEqual(0, result.returncode, result.stderr)
                self.assertEqual({}, self.linked())
                self.assertIn("-- native modules: 0 exposed, 0 left out (built for another module API) --",
                              result.stdout)
                self.assertTrue(result.stdout.endswith("done\n"), result.stdout)

    def test_qore_fails(self):
        mods = self.module_dir("mods", ("pgsql-api-2.1.qmod",))
        # a qore that cannot run ends the script (set -e): nothing is linked
        result = self.run_farm(self.fake_qore("", rc=1), mods)
        self.assertNotEqual(0, result.returncode)
        self.assertIn("--module-apis failed", result.stderr)
        self.assertNotIn("done", result.stdout)
        self.assertEqual({}, self.linked())

    def test_invalid_module_apis(self):
        mods = self.module_dir("mods", ("pgsql-api-2.1.qmod",))
        # an output that is not a list of <major>.<minor> items ends the script (set -e): nothing is linked
        for output in ("", "warning: blah\n2.1\n", "2.1\nwarning: blah", "Qore 3.0.0", "2.1, warning", "2", "2.",
                       ".1", "2.1.0", "2..1", "2.1,", "2.1, ", ",2.1", "2.1,,2.0", "2.1,  2.0", "2.1, 2.x"):
            with self.subTest(output=output):
                result = self.run_farm(self.fake_qore(output), mods)
                self.assertNotEqual(0, result.returncode, result.stdout)
                self.assertIn("did not give a list of module APIs", result.stderr)
                self.assertNotIn("done", result.stdout)
                self.assertEqual({}, self.linked())

    @unittest.skipUnless(HAVE_QORE, f"no qore build in {BUILD_DIR}")
    def test_build_qore(self):
        apis = subprocess.run([str(BUILD_DIR / "qore"), "--module-apis"], capture_output=True, text=True,
                              timeout=60, check=True).stdout.strip()
        current = apis.split(", ")[0]
        major, minor = (int(x) for x in current.split("."))
        other = f"{major}.{minor + 1}"
        self.assertNotIn(other, apis.split(", "))
        mods = self.module_dir("mods", (f"pgsql-api-{current}.qmod", f"pgsql-api-{other}.qmod",
                                        "xml-api-1.9.qmod"))
        result = self.run_farm(BUILD_DIR / "qore", mods)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual({f"pgsql-api-{current}.qmod"}, set(self.linked()))
        self.assertIn(f"built for module API {other}", result.stdout)
        self.assertIn("built for module API 1.9", result.stdout)


if __name__ == "__main__":
    unittest.main()
