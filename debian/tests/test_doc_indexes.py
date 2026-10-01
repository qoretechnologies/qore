#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Exercise the actual optional SDK-index installation rule with debhelper."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("dh_install"), "requires debhelper")
class DocIndexesTest(unittest.TestCase):
    def test_optional_indexes_are_packaged_without_unclaimed_files(self):
        rules = (ROOT / "debian/rules").read_text()
        recipe = re.search(r"^override_dh_install:\n(?:\t[^\n]*\n)+", rules, re.MULTILINE)
        self.assertIsNotNone(recipe)
        for selector in ("", "-a", "-i"):
            for present in (False, True):
                with self.subTest(selector=selector, present=present), tempfile.TemporaryDirectory() as directory:
                    root = Path(directory)
                    debian = root / "debian"
                    debian.mkdir()
                    (debian / "rules").write_text(recipe.group(0))
                    (debian / "control").write_text(
                        "Source: qore\nSection: interpreters\nPriority: optional\n"
                        "Maintainer: Test <test@example.invalid>\nBuild-Depends: debhelper-compat (= 13)\n\n"
                        "Package: libqore-dev\nArchitecture: any\nDescription: SDK fixture\n Test SDK.\n\n"
                        "Package: qore-doc\nArchitecture: all\nDescription: Documentation fixture\n Test docs.\n")
                    (debian / "changelog").write_text(
                        "qore (3.0.0-1) unstable; urgency=medium\n\n  * Fixture.\n\n"
                        " -- Test <test@example.invalid>  Thu, 01 Oct 2026 12:00:00 +0000\n")
                    content = {"qore.tag": b'<tagfile><compound kind="page"/></tagfile>\n',
                               "module-tags/DataProvider.tag": b'<tagfile><compound kind="class"/></tagfile>\n'}
                    if present:
                        for name, data in content.items():
                            path = debian / "tmp/usr/share/qore" / name
                            path.parent.mkdir(parents=True, exist_ok=True)
                            path.write_bytes(data)
                    env = dict(os.environ, DH_OPTIONS=selector)
                    for command in (["make", "-f", "debian/rules", "override_dh_install"],
                                    ["dh_missing", "--fail-missing"]):
                        result = subprocess.run(command, cwd=root, env=env, capture_output=True,
                                                text=True, timeout=60)
                        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                        self.assertEqual("", result.stderr)
                    for name, data in content.items():
                        path = debian / "libqore-dev/usr/share/qore" / name
                        if present and selector != "-i":
                            self.assertEqual(data, path.read_bytes())
                        else:
                            self.assertFalse(path.exists())


if __name__ == "__main__":
    unittest.main()
