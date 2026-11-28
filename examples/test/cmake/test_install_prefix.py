#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Checks that "cmake --install --prefix <dir>" installs Qore below <dir> (qore#7849).

The install destinations are relative to the install prefix, so that the prefix given at install time relocates the
installation; before, most of them were absolute paths of the prefix given at configure time, and "--prefix" did not
apply to them.  The build tree's components that hold the core installation (qore-bootstrap: the programs, the
library, the headers, the CMake package files, the module sources, the catalogs and the metadata; and the unnamed
component: aot-link.conf) are installed into a temporary prefix, and every file installed, as the install manifests
and the install log name them, must be below it.  aot-link.conf must name the library directory of the install-time
prefix, and the installed QoreConfig.cmake must take its paths from its own location, not from the configured prefix.
The AOT-compiled modules (qore-qlib-aot) use the same relative directories; they are not installed here, as they are
large.

The install runs the build tree's install script as "cmake --install --prefix <dir> --component <c>" does, from a
copy whose install manifest is written to the test's temporary directory: the script writes it into the build tree,
which the user that runs the test may not be allowed to write (the CI jobs build as root and test as another user).
The install rules themselves are the build tree's: the copy includes the scripts of the subdirectories by their
absolute paths.  A configured prefix that is empty (the macOS CI job configures with -DCMAKE_INSTALL_PREFIX=) is the
root directory, below which the temporary prefix always is, so the checks against it are not made then.

The test needs a configured and built Qore build tree (QORE_TEST_BUILD_DIR, or build/ in this tree), and is skipped
without one.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
BUILD_DIR = Path(os.environ.get("QORE_TEST_BUILD_DIR", str(ROOT / "build")))
COMPONENTS = ("qore-bootstrap", "Unspecified")


def configured_prefix():
    cache = BUILD_DIR / "CMakeCache.txt"
    for line in cache.read_text(errors="replace").splitlines():
        if line.startswith("CMAKE_INSTALL_PREFIX:"):
            return line.split("=", 1)[1]
    return None


@unittest.skipUnless((BUILD_DIR / "cmake_install.cmake").is_file() and (BUILD_DIR / "CMakeCache.txt").is_file(),
                     f"no configured Qore build tree in {BUILD_DIR} (QORE_TEST_BUILD_DIR)")
class InstallPrefixTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qore-install-prefix-")
        self.addCleanup(temporary.cleanup)
        self.tmp = Path(os.path.realpath(temporary.name))
        # the real path, as the manifests name it
        self.prefix = self.tmp / "prefix"
        configured = configured_prefix()
        self.assertIsNotNone(configured, "the build tree has a configured install prefix")
        # the configured prefix without its trailing slashes; empty for the root directory
        self.configured = configured.rstrip("/")
        if self.configured:
            self.assertFalse(str(self.prefix).startswith(self.configured + "/"),
                             "the temporary prefix is not below the configured one")

    def install_script(self):
        """Returns a copy of the build tree's install script that writes its manifest to the temporary directory"""
        text = (BUILD_DIR / "cmake_install.cmake").read_text()
        pattern = re.compile(r'file\(WRITE "[^"\n]*/\$\{CMAKE_INSTALL_MANIFEST\}"')
        self.assertEqual(1, len(pattern.findall(text)), "the install script writes one install manifest")
        manifests = self.tmp / "manifests"
        manifests.mkdir()
        text = pattern.sub(lambda m: f'file(WRITE "{manifests}/${{CMAKE_INSTALL_MANIFEST}}"', text)
        script = self.tmp / "cmake_install.cmake"
        script.write_text(text)
        return script, manifests

    def install(self):
        script, manifests = self.install_script()
        installed = []
        log = ""
        for component in COMPONENTS:
            # as "cmake --install <build> --prefix <prefix> --component <component>"
            result = subprocess.run(["cmake", f"-DCMAKE_INSTALL_PREFIX={self.prefix}",
                                     f"-DCMAKE_INSTALL_COMPONENT={component}", "-P", str(script)],
                                    text=True, capture_output=True, timeout=1800)
            self.assertEqual(0, result.returncode, f"{component}: {result.stdout[-4000:]}{result.stderr[-4000:]}")
            log += result.stdout
            manifest = manifests / f"install_manifest_{component}.txt"
            self.assertTrue(manifest.is_file(), f"{component}: the install wrote its manifest")
            installed += manifest.read_text().splitlines()
        return installed, log

    def test_everything_below_the_prefix(self):
        installed, log = self.install()
        self.assertTrue(installed, "files are installed")
        root = str(self.prefix) + "/"
        outside = [path for path in installed if not path.startswith(root)]
        self.assertEqual([], outside[:20], f"{len(outside)} of {len(installed)} files are installed outside the prefix")
        # the paths that the install reports, also those of the install scripts that write files themselves
        reported = re.findall(r"^-- (?:Installing|Up-to-date): (.*)$", log, re.M)
        self.assertEqual([], [path for path in reported if not path.startswith(root)][:20],
                         "the install reports no path outside the prefix")
        # nothing is written below the configured prefix (the root directory, when it is empty, holds everything)
        if self.configured:
            self.assertEqual([], [path for path in installed if path.startswith(self.configured + "/")])

        # aot-link.conf names the library directory of the install-time prefix
        confs = list(self.prefix.glob("**/qore/aot-link.conf"))
        self.assertEqual(1, len(confs), f"one aot-link.conf: {confs}")
        libdir = [line for line in confs[0].read_text().splitlines() if line.startswith("libdir=")]
        self.assertEqual(1, len(libdir))
        self.assertTrue(libdir[0][len("libdir="):].startswith(root), libdir[0])
        self.assertTrue(Path(libdir[0][len("libdir="):]).is_dir(), "the library directory exists")

        # the installed QoreConfig.cmake takes its paths from its location, not from the configured prefix
        configs = list(self.prefix.glob("**/cmake/Qore/QoreConfig.cmake"))
        self.assertEqual(1, len(configs), f"one QoreConfig.cmake: {configs}")
        text = configs[0].read_text()
        self.assertIn('get_filename_component(_qore_install_prefix "${CMAKE_CURRENT_LIST_DIR}/', text)
        if self.configured:
            self.assertNotIn(self.configured + "/", text,
                             "the installed QoreConfig.cmake names no path of the configured prefix")


if __name__ == "__main__":
    unittest.main()
