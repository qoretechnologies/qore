#!/usr/bin/env python3
"""The compiler cache of the macOS CI job (ccache-ci.sh) is used only where it is safe, and only read where it may not
be written.

test-macos.sh compiles through ccache when the runner mounts <QORE_QMOD_CACHE_DIR>/ccache and the image has ccache.
The jobs of protected refs, on the runner that mounts the directory read-write, write the cache; every other job only
reads it, and only a cache of its own toolchain (the architecture, the macOS, SDK and compiler versions and ccache):
an image with another toolchain finds no cache.  These tests run qore_ccache_setup and qore_ccache_finish with
stand-ins for ccache and the toolchain's commands and check the settings that the build gets, what is created in the
cache directory, the removal of the caches that no job has used for a while, and that test-macos.sh uses them around
its build only.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import unittest


ROOT = Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "examples/test/docker_test/ccache-ci.sh"

#: the variables that the setup sets for the build, printed after the setup and after the finish
VARIABLES = ("CCACHE_CMAKE_ARGS", "CCACHE_DIR", "CCACHE_READONLY", "CCACHE_NOSTATS", "CCACHE_MAXSIZE",
             "CCACHE_COMPILERCHECK", "CCACHE_TEMPDIR", "CCACHE_STATSLOG")

DRIVER = """. "$1"
qore_ccache_setup
for v in %(vars)s; do eval "echo setup:$v=\\${$v-<unset>}"; done
env | grep '^CCACHE_' | sed 's/^/exported:/' | sort
tempdir="$CCACHE_TEMPDIR"
qore_ccache_finish
for v in %(vars)s; do eval "echo finish:$v=\\${$v-<unset>}"; done
[ -z "$tempdir" ] || [ -e "$tempdir" ] || echo "finish:tempdir-removed"
""" % {"vars": " ".join(VARIABLES)}


def system_tools(directory):
    """Links every program of the system path (os.defpath) into the directory but ccache

    The tests decide whether ccache is found, so the system's own ccache (as on a host or CI node that has one) must
    not be found in the path that they give the scripts.
    """
    for path_dir in os.defpath.split(os.pathsep):
        if not path_dir or not os.path.isdir(path_dir):
            continue
        with os.scandir(path_dir) as entries:
            for entry in entries:
                target = directory / entry.name
                if entry.name == "ccache" or target.exists() or target.is_symlink():
                    continue
                if entry.is_file() and os.access(entry.path, os.X_OK):
                    target.symlink_to(entry.path)


def stub(directory, name, text):
    path = directory / name
    path.write_text("#!/bin/sh\n" + text)
    path.chmod(0o755)


class CiCcacheTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="qore-ci-ccache-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.mount = self.root / "mount"
        self.mount.mkdir()
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.tmp = self.root / "tmp"
        self.tmp.mkdir()
        # the system's programs without the system's ccache (os.defpath would find the host's ccache)
        self.sys = self.root / "sys"
        self.sys.mkdir()
        system_tools(self.sys)
        # the toolchain: its version strings make the key
        self.compiler_version = "Apple clang version 17.0.0 (clang-1700.0.13.5)"
        self.write_toolchain()
        stub(self.bin, "ccache", 'case "$1" in --version) echo "ccache version 4.12.1";; *) echo "ccache $*";; esac\n')
        if not shutil.which("shasum", path=str(self.sys)):
            stub(self.bin, "shasum", 'shift 2; exec sha256sum "$@"\n')

    def write_toolchain(self):
        stub(self.bin, "sw_vers", 'echo 26.1\n')
        stub(self.bin, "xcrun", 'echo 26.1\n')
        stub(self.bin, "cc", 'echo "%s"\n' % self.compiler_version)
        stub(self.bin, "c++", 'echo "%s"\n' % self.compiler_version)

    def run_setup(self, env_extra, path_bin=True):
        env = {"PATH": (str(self.bin) + ":" if path_bin else "") + str(self.sys), "TMPDIR": str(self.tmp)}
        env.update(env_extra)
        result = subprocess.run(["sh", "-c", DRIVER, "driver", str(SCRIPT)], env=env, capture_output=True, text=True,
                                timeout=60)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        values = {}
        for line in result.stdout.splitlines():
            if line.startswith(("setup:", "finish:")) and "=" in line:
                key, value = line.split("=", 1)
                values[key] = value
        exported = sorted(line.split(":", 1)[1].split("=", 1)[0] for line in result.stdout.splitlines()
                          if line.startswith("exported:"))
        return result.stdout, values, exported

    def key_dirs(self):
        return sorted(p.name for p in (self.mount / "ccache").iterdir()) if (self.mount / "ccache").is_dir() else []

    def assert_no_cache(self, values, exported):
        self.assertEqual("", values["setup:CCACHE_CMAKE_ARGS"])
        self.assertEqual([], exported)

    def test_no_mount(self):
        for env in ({}, {"QORE_QMOD_CACHE_DIR": str(self.mount)}):
            with self.subTest(env=env):
                out, values, exported = self.run_setup(dict(env, CI_COMMIT_REF_PROTECTED="true"))
                self.assert_no_cache(values, exported)
                self.assertIn("no cache directory", out)
                self.assertEqual([], self.key_dirs())

    def test_no_ccache_or_disabled(self):
        (self.mount / "ccache").mkdir()
        (self.bin / "ccache").unlink()
        out, values, exported = self.run_setup({"QORE_QMOD_CACHE_DIR": str(self.mount),
                                                "CI_COMMIT_REF_PROTECTED": "true"})
        self.assert_no_cache(values, exported)
        self.assertIn("the image has no ccache", out)
        stub(self.bin, "ccache", 'echo "ccache version 4.12.1"\n')
        out, values, exported = self.run_setup({"QORE_QMOD_CACHE_DIR": str(self.mount),
                                                "CI_COMMIT_REF_PROTECTED": "true", "CCACHE_DISABLE": "1"})
        self.assertEqual("", values["setup:CCACHE_CMAKE_ARGS"])
        self.assertEqual(["CCACHE_DISABLE"], exported)
        self.assertIn("CCACHE_DISABLE=1", out)
        self.assertEqual([], self.key_dirs())

    def test_protected_ref_writes(self):
        (self.mount / "ccache").mkdir()
        out, values, exported = self.run_setup({"QORE_QMOD_CACHE_DIR": str(self.mount),
                                                "CI_COMMIT_REF_PROTECTED": "true"})
        dirs = self.key_dirs()
        self.assertEqual(1, len(dirs))
        key_dir = self.mount / "ccache" / dirs[0]
        self.assertTrue((key_dir / ".last-used").is_file())
        self.assertEqual(str(key_dir), values["setup:CCACHE_DIR"])
        self.assertEqual("-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache",
                         values["setup:CCACHE_CMAKE_ARGS"])
        self.assertEqual("<unset>", values["setup:CCACHE_READONLY"])
        self.assertEqual("8G", values["setup:CCACHE_MAXSIZE"])
        self.assertEqual("%compiler% --version", values["setup:CCACHE_COMPILERCHECK"])
        # the temporary files stay out of the cache directory
        self.assertTrue(values["setup:CCACHE_TEMPDIR"].startswith(str(self.tmp) + "/"))
        self.assertEqual(["CCACHE_COMPILERCHECK", "CCACHE_DIR", "CCACHE_MAXSIZE", "CCACHE_STATSLOG", "CCACHE_TEMPDIR"],
                         exported)
        self.assertIn("ccache --show-stats", out)
        # nothing compiled after the build goes through the cache, and the job's temporary files are removed
        for name in VARIABLES[1:]:
            self.assertEqual("<unset>", values["finish:" + name], name)
        self.assertIn("finish:tempdir-removed", out)

    def test_other_refs_only_read(self):
        (self.mount / "ccache").mkdir()
        for env in ({"CI_COMMIT_REF_PROTECTED": "false"}, {}):
            with self.subTest(env=env):
                # a job that may not write creates nothing: without a cache of its toolchain it builds without one
                out, values, exported = self.run_setup(dict(env, QORE_QMOD_CACHE_DIR=str(self.mount)))
                self.assert_no_cache(values, exported)
                self.assertIn("no cache of this toolchain", out)
                self.assertEqual([], self.key_dirs())
        # the protected ref's job made the cache of this toolchain; the other refs' jobs read it, and write nothing
        self.run_setup({"QORE_QMOD_CACHE_DIR": str(self.mount), "CI_COMMIT_REF_PROTECTED": "true"})
        key_dir = self.mount / "ccache" / self.key_dirs()[0]
        stamp = (key_dir / ".last-used").stat().st_mtime_ns
        out, values, exported = self.run_setup({"QORE_QMOD_CACHE_DIR": str(self.mount),
                                                "CI_COMMIT_REF_PROTECTED": "false"})
        self.assertEqual(str(key_dir), values["setup:CCACHE_DIR"])
        self.assertEqual("1", values["setup:CCACHE_READONLY"])
        self.assertEqual("1", values["setup:CCACHE_NOSTATS"])
        self.assertEqual("<unset>", values["setup:CCACHE_MAXSIZE"])
        self.assertIn("(read-only)", out)
        self.assertNotIn("ccache --show-stats", out)
        self.assertEqual(stamp, (key_dir / ".last-used").stat().st_mtime_ns)
        self.assertEqual([".last-used"], sorted(p.name for p in key_dir.iterdir()))

    @unittest.skipIf(hasattr(os, "geteuid") and os.geteuid() == 0, "root can write a directory of mode 0555")
    def test_protected_ref_on_read_only_mount_only_reads(self):
        (self.mount / "ccache").mkdir()
        self.run_setup({"QORE_QMOD_CACHE_DIR": str(self.mount), "CI_COMMIT_REF_PROTECTED": "true"})
        (self.mount / "ccache").chmod(0o555)
        self.addCleanup((self.mount / "ccache").chmod, 0o755)
        out, values, exported = self.run_setup({"QORE_QMOD_CACHE_DIR": str(self.mount),
                                                "CI_COMMIT_REF_PROTECTED": "true"})
        self.assertEqual("1", values["setup:CCACHE_READONLY"])
        self.assertIn("(read-only)", out)

    def test_toolchain_keys(self):
        (self.mount / "ccache").mkdir()
        env = {"QORE_QMOD_CACHE_DIR": str(self.mount), "CI_COMMIT_REF_PROTECTED": "true"}
        self.run_setup(env)
        self.run_setup(env)
        self.assertEqual(1, len(self.key_dirs()), "the same toolchain uses the same cache")
        first = self.key_dirs()[0]
        # another compiler: another cache, which a reader of the new toolchain does not find until a writer made it
        self.compiler_version = "Apple clang version 17.0.0 (clang-1700.3.19.1)"
        self.write_toolchain()
        out, values, exported = self.run_setup({"QORE_QMOD_CACHE_DIR": str(self.mount)})
        self.assert_no_cache(values, exported)
        self.run_setup(env)
        self.assertEqual(2, len(self.key_dirs()))
        self.assertNotEqual(str(self.mount / "ccache" / first), self.run_setup(env)[1]["setup:CCACHE_DIR"])

    def test_writer_removes_unused_caches(self):
        base = self.mount / "ccache"
        base.mkdir()
        old = time.time() - 40 * 86400
        for name, mtime in (("oldtoolchain", old), ("recenttoolchain", time.time() - 86400)):
            (base / name).mkdir()
            (base / name / "object").write_text("o")
            (base / name / ".last-used").touch()
            os.utime(base / name / ".last-used", (mtime, mtime))
        # a reader removes nothing
        self.run_setup({"QORE_QMOD_CACHE_DIR": str(self.mount)})
        self.assertEqual(["oldtoolchain", "recenttoolchain"], self.key_dirs())
        out, values, exported = self.run_setup({"QORE_QMOD_CACHE_DIR": str(self.mount),
                                                "CI_COMMIT_REF_PROTECTED": "true"})
        own = Path(values["setup:CCACHE_DIR"]).name
        self.assertEqual(sorted([own, "recenttoolchain"]), self.key_dirs())
        self.assertIn("removing %s" % (base / "oldtoolchain"), out)
        # a shorter limit removes the recent one too, never the job's own
        out, values, exported = self.run_setup({"QORE_QMOD_CACHE_DIR": str(self.mount),
                                                "CI_COMMIT_REF_PROTECTED": "true", "QORE_CCACHE_MAX_AGE_DAYS": "0"})
        self.assertEqual([own], self.key_dirs())

    def test_test_macos_uses_it_around_the_build(self):
        text = (ROOT / "examples/test/docker_test/test-macos.sh").read_text()
        setup = text.index("qore_ccache_setup")
        cmake = text.index("cmake .. \\")
        make = text.index("make -j${MAKE_JOBS}")
        finish = text.index("qore_ccache_finish")
        self.assertLess(text.index('. "${QORE_SRC_DIR}/test/docker_test/ccache-ci.sh"'), setup)
        self.assertLess(setup, cmake)
        self.assertLess(cmake, text.index("${CCACHE_CMAKE_ARGS}"))
        self.assertLess(text.index("${CCACHE_CMAKE_ARGS}"), make)
        self.assertLess(make, finish)
        self.assertLess(finish, text.index("./run_tests.sh"))
        # no other CI script uses the compiler cache
        for script in sorted((ROOT / "examples/test/docker_test").glob("*.sh")):
            if script.name not in ("test-macos.sh", "ccache-ci.sh"):
                with self.subTest(script=script.name):
                    self.assertNotIn("ccache", script.read_text())


if __name__ == "__main__":
    unittest.main()
