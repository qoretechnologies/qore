#!/usr/bin/env python3
"""The CI test jobs expose only the native modules that the qore under test loads.

test-ubuntu.sh, test-alpine.sh and test-macos.sh give the tests the native modules of the image through a link farm
(qore_ci_native_module_farm in examples/test/docker_test/ci-native-modules.sh).  A module built for a module API that
the qore under test does not load is refused by the loader, which reports it as not found; the tests that needed it
then failed instead of skipping, as in pipeline 64230, whose qore loaded module API 2.1 while the image's modules
were built for 2.0.  The farm leaves such a module out, so that it is absent, as on a host without it.

A module of the image that has no build for a module API that the qore loads fails the job instead, as its tests
would be skipped and the job would pass without them (pipeline 64250, whose 34 native modules were all built for
2.0), unless QORE_CI_NATIVE_MODULE_API_TRANSITION=1 is set for a batch that changes the module API; the job then
names the modules at the start and again after the tests (qore_ci_native_module_notice).

The cases run the function with a stand-in for qore that prints the module APIs it is given, and, when there is a
build (QORE_TEST_BUILD_DIR, or build/ in this tree), with the qore of the build.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
HELPER = ROOT / "examples/test/docker_test/ci-native-modules.sh"

BUILD_DIR = Path(os.environ.get("QORE_TEST_BUILD_DIR", str(ROOT / "build")))
LIBQORE = next((BUILD_DIR / name for name in ("libqore.so", "libqore.dylib") if (BUILD_DIR / name).exists()), None)
HAVE_QORE = (BUILD_DIR / "qore").exists() and LIBQORE is not None

# the shells that the function runs in: sh, bash, and dash where it is installed (sh is dash on Debian and Ubuntu,
# and bash on macOS)
SHELLS = ("sh", "bash") + (("dash",) if shutil.which("dash") else ())


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

    def run_farm(self, qore, *dirs, shell="sh", transition=None):
        """Runs the function as the scripts do: sourced, under set -e, then the notice that they print after the
        tests; transition is the value of QORE_CI_NATIVE_MODULE_API_TRANSITION, which is unset if it is None"""
        env = {k: v for k, v in os.environ.items() if k != "QORE_CI_NATIVE_MODULE_API_TRANSITION"}
        if transition is not None:
            env["QORE_CI_NATIVE_MODULE_API_TRANSITION"] = transition
        return subprocess.run([shell, "-c", 'set -e; . "$1"; shift; qore_ci_native_module_farm "$@"; echo done; '
                               'qore_ci_native_module_notice',
                               shell, str(HELPER), str(qore), str(self.farm)] + [str(d) for d in dirs],
                              capture_output=True, text=True, timeout=60, env=env)

    def linked(self):
        return {p.name: os.readlink(p) for p in self.farm.iterdir()}

    def test_only_loaded_api(self):
        mods = self.module_dir("mods", ("pgsql-api-2.0.qmod", "pgsql-api-2.1.qmod", "xml-api-2.0.qmod",
                                        "yaml-api-2.1.qmod", "SqlUtil.qmod", "Util-1.0.qmod"))
        result = self.run_farm(self.fake_qore("2.1\n"), mods, transition="1")
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
        self.assertIn("done\n", result.stdout)

    def test_more_than_one_api(self):
        mods = self.module_dir("mods", ("a-api-2.0.qmod", "b-api-2.1.qmod", "c-api-1.3.qmod", "d-api-2.10.qmod"))
        result = self.run_farm(self.fake_qore("2.1, 2.0\n"), mods, transition="1")
        self.assertEqual(0, result.returncode, result.stderr)
        # 2.10 is not 2.1, and 1.3 is not loaded
        self.assertEqual({"a-api-2.0.qmod", "b-api-2.1.qmod"}, set(self.linked()))

    def test_list_without_spaces(self):
        # the items may be separated by a comma alone: they are matched all the same
        mods = self.module_dir("mods", ("a-api-2.0.qmod", "b-api-2.1.qmod", "c-api-1.9.qmod"))
        result = self.run_farm(self.fake_qore("2.0,2.1"), mods, transition="1")
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual({"a-api-2.0.qmod", "b-api-2.1.qmod"}, set(self.linked()))
        self.assertIn("-- native modules: this qore loads module API 2.0, 2.1 --", result.stdout)
        self.assertIn("-- native modules: 2 exposed, 1 left out (built for another module API): c-api-1.9 --",
                      result.stdout)

    # the caller's shell state, set up before the call, and what it is after it: IFS as hexadecimal bytes, then
    # whether pathname expansion is off
    STATE_CASES = {
        "": "ifs=set:20090a\nglob",
        "IFS=:;": "ifs=set:3a\nglob",
        "IFS=;": "ifs=set:\nglob",
        "unset IFS;": "ifs=:\nglob",
        "set -f;": "ifs=set:20090a\nnoglob",
    }

    def shell_state(self, shell, setup, qore, mods):
        """Runs the function in the given shell after the given setup, and returns its exit status and the caller's
        IFS and pathname expansion setting after it"""
        script = ('. "$1"; shift; {setup} qore_ci_native_module_farm "$@" > /dev/null 2>&1; echo "rc=$?"; '
                  'echo "ifs=${{IFS+set}}:$(printf %s "$IFS" | od -An -tx1 | tr -d " \\n")"; '
                  'case $- in *f*) echo noglob ;; *) echo glob ;; esac')
        for f in self.farm.iterdir():
            f.unlink()
        result = subprocess.run([shell, "-c", script.format(setup=setup), shell, str(HELPER), str(qore),
                                 str(self.farm), str(mods)], capture_output=True, text=True, timeout=60)
        self.assertEqual(0, result.returncode, result.stderr)
        rc, state = result.stdout.split("\n", 1)
        return rc, state

    def test_shell_state_restored(self):
        """The caller's IFS (set, empty or unset) and pathname expansion setting are kept"""
        mods = self.module_dir("mods", ("a-api-2.1.qmod",))
        qore = self.fake_qore("2.1, 2.0")
        for shell in SHELLS:
            for setup, expected in self.STATE_CASES.items():
                with self.subTest(shell=shell, setup=setup):
                    self.assertEqual(("rc=0", expected + "\n"), self.shell_state(shell, setup, qore, mods))
                    # the modules are found also when the caller turned pathname expansion off
                    self.assertEqual({"a-api-2.1.qmod"}, set(self.linked()))

    def test_shell_state_restored_on_error(self):
        """The caller's IFS and pathname expansion setting are kept when the module APIs are refused"""
        mods = self.module_dir("mods", ("a-api-2.1.qmod",))
        # refused while the list is split, and before it is
        for output in ("2.1, 2.x", "2.1,,2.0", "warning: blah\n2.1"):
            qore = self.fake_qore(output)
            for shell in SHELLS:
                for setup, expected in self.STATE_CASES.items():
                    with self.subTest(output=output, shell=shell, setup=setup):
                        self.assertEqual(("rc=1", expected + "\n"), self.shell_state(shell, setup, qore, mods))
                        self.assertEqual({}, self.linked())

    def test_first_directory_wins(self):
        first = self.module_dir("first", ("xml-api-2.1.qmod",))
        second = self.module_dir("second", ("xml-api-2.1.qmod", "zip-api-2.1.qmod", "uuid-api-2.0.qmod"))
        # an empty argument (an unset prefix) and a directory that does not exist are ignored
        result = self.run_farm(self.fake_qore("2.1"), "", first, self.root / "none", second, transition="1")
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

    NOTICE = ("!!! native modules: QORE_CI_NATIVE_MODULE_API_TRANSITION=1: the image's native modules uuid xml were "
              "built only for another module API (uuid-api-2.0 xml-api-1.9 xml-api-2.0) than the 2.1 that this qore "
              "loads, so their tests would be skipped; this job does not test them\n")

    def stale_image(self):
        """Makes the modules of an image whose xml and uuid modules were built for other module APIs only, while
        pgsql has a build for 2.1 too"""
        return self.module_dir("mods", ("pgsql-api-2.0.qmod", "pgsql-api-2.1.qmod", "uuid-api-2.0.qmod",
                                        "xml-api-1.9.qmod", "xml-api-2.0.qmod", "yaml-api-2.1.qmod"))

    def test_stale_image_fails(self):
        """A module of the image without a build for the loaded module API fails the job, naming the module APIs
        and the modules; a module with such a build next to builds for other APIs does not"""
        mods = self.stale_image()
        for shell in SHELLS:
            for transition in (None, "", "0", "yes"):
                with self.subTest(shell=shell, transition=transition):
                    for f in self.farm.iterdir():
                        f.unlink()
                    result = self.run_farm(self.fake_qore("2.1"), mods, shell=shell, transition=transition)
                    self.assertNotEqual(0, result.returncode, result.stdout)
                    self.assertNotIn("done", result.stdout)
                    self.assertEqual("qore_ci_native_module_farm: the image's native modules uuid xml were built only "
                                     "for another module API (uuid-api-2.0 xml-api-1.9 xml-api-2.0) than the 2.1 that "
                                     "this qore loads, so their tests would be skipped: rebuild the image's native "
                                     "modules for module API 2.1, or set QORE_CI_NATIVE_MODULE_API_TRANSITION=1 in a "
                                     "batch that changes the module API (see ci-native-modules.sh)\n", result.stderr)
                    # the coverage is printed before the job fails
                    self.assertIn("-- native modules: 2 exposed, 4 left out (built for another module API): "
                                  "pgsql-api-2.0 uuid-api-2.0 xml-api-1.9 xml-api-2.0 --", result.stdout)

    def test_all_modules_stale(self):
        """The case of pipeline 64250: no module of the image is exposed"""
        mods = self.module_dir("mods", ("process-api-2.0.qmod", "sqlite3-api-2.0.qmod"))
        result = self.run_farm(self.fake_qore("2.1"), mods)
        self.assertNotEqual(0, result.returncode, result.stdout)
        self.assertIn("native modules process sqlite3 were built only for another module API (process-api-2.0 "
                      "sqlite3-api-2.0) than the 2.1", result.stderr)
        self.assertEqual({}, self.linked())

    def test_api_transition(self):
        """QORE_CI_NATIVE_MODULE_API_TRANSITION=1 lets the job run, and the job names the modules that it does not
        test, at the start and in the notice after the tests"""
        mods = self.stale_image()
        for shell in SHELLS:
            with self.subTest(shell=shell):
                for f in self.farm.iterdir():
                    f.unlink()
                result = self.run_farm(self.fake_qore("2.1"), mods, shell=shell, transition="1")
                self.assertEqual(0, result.returncode, result.stderr)
                self.assertEqual("", result.stderr)
                self.assertEqual({"pgsql-api-2.1.qmod", "yaml-api-2.1.qmod"}, set(self.linked()))
                start, end = result.stdout.split("done\n")
                self.assertIn(self.NOTICE, start)
                self.assertIn(self.NOTICE, end)
                self.assertTrue(end.startswith("!!!!"), end)

    def test_no_notice(self):
        """A job whose modules all have a build for the loaded API prints no notice, also with the override"""
        mods = self.module_dir("mods", ("pgsql-api-2.0.qmod", "pgsql-api-2.1.qmod"))
        for transition in (None, "1"):
            with self.subTest(transition=transition):
                for f in self.farm.iterdir():
                    f.unlink()
                result = self.run_farm(self.fake_qore("2.1"), mods, transition=transition)
                self.assertEqual(0, result.returncode, result.stderr)
                self.assertTrue(result.stdout.endswith("done\n"), result.stdout)
                self.assertNotIn("!!!", result.stdout)

    def test_stale_shell_state_restored(self):
        """The caller's IFS and pathname expansion setting are kept when the job fails for a stale image and when
        the override lets it run"""
        mods = self.module_dir("mods", ("a-api-2.1.qmod", "b-api-2.0.qmod"))
        qore = self.fake_qore("2.1")
        for transition, rc in (("", "rc=1"), ("1", "rc=0")):
            for shell in SHELLS:
                for setup, expected in self.STATE_CASES.items():
                    with self.subTest(transition=transition, shell=shell, setup=setup):
                        setup = f"QORE_CI_NATIVE_MODULE_API_TRANSITION={transition}; {setup}"
                        self.assertEqual((rc, expected + "\n"), self.shell_state(shell, setup, qore, mods))

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
        result = self.run_farm(BUILD_DIR / "qore", mods, transition="1")
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual({f"pgsql-api-{current}.qmod"}, set(self.linked()))
        self.assertIn(f"built for module API {other}", result.stdout)
        self.assertIn("built for module API 1.9", result.stdout)


if __name__ == "__main__":
    unittest.main()
