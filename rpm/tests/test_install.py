# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("finalize", Path(__file__).resolve().parents[1] / "finalize-install.py")
install = importlib.util.module_from_spec(spec)
spec.loader.exec_module(install)


class InstallTest(unittest.TestCase):
    def test_manifest_and_interpreters(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in install.TOOLS:
                for path in [root / f"usr/bin/{name}", root / f"usr/share/man/man1/{name}.1"]:
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text("#!/usr/bin/env qore\nbody\n")
            for name in install.TEMPLATES:
                path = root / f"usr/share/qore/{name}"
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("template\n")
            (root / "usr/lib64/cmake/Qore").mkdir(parents=True)
            install.finalize(root, "/usr/lib64", root)
            self.assertEqual((root / "usr/bin/qdp").read_text(), "#!/usr/bin/qore\nbody\n")
            self.assertIn("/usr/share/man/man1/qdp.1*\n", (root / "rpm-tools.files").read_text())
            self.assertEqual(len((root / "rpm-devel-extra.files").read_text().splitlines()), 11)

    def test_missing_inventory_and_invalid_libdir(self):
        with tempfile.TemporaryDirectory() as tmp:
            for libdir in ["/usr/lib64", "/../../etc"]:
                with self.assertRaises(ValueError):
                    install.finalize(tmp, libdir, tmp)
            self.assertFalse((Path(tmp) / "rpm-tools.files").exists())
