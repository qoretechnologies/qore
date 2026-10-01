# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

loader = importlib.util.spec_from_file_location("aot_sources", Path(__file__).resolve().parents[1] / "install-aot-sources.py")
sources = importlib.util.module_from_spec(loader)
loader.loader.exec_module(sources)


class SourceInstallTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / "source"
        self.buildroot = self.root / "buildroot"
        self.buildroot.mkdir()
        (self.source / "qlib/Probe").mkdir(parents=True)
        self.module = self.source / "qlib/Probe/Probe.qm"
        self.module.write_text("%modern\n")
        (self.module.parent / "method.qc").write_text("public int sub answer() { return 42; }\n")
        (self.module.parent / "icon.svg").write_text("<svg/>")
        (self.module.parent / "Probe.qmod").symlink_to("missing-build-output")
        self.prefix = "/usr/src/debug/package-1.0"
        self.destination = self.buildroot / self.prefix.lstrip("/")

    def install(self):
        return sources.install_sources(self.buildroot, self.prefix, ["qlib"], self.source)

    def test_sources_keep_paths_timestamps_and_contents(self):
        os.utime(self.module, (1_600_000_000, 1_600_000_000))
        self.assertEqual(2, self.install())
        installed = self.destination / "qlib/Probe/Probe.qm"
        self.assertEqual(self.module.read_bytes(), installed.read_bytes())
        self.assertEqual(self.module.stat().st_mtime_ns, installed.stat().st_mtime_ns)
        self.assertEqual(0o644, installed.stat().st_mode & 0o777)
        self.assertEqual(2, len([p for p in self.destination.rglob("*") if p.is_file()]))

    def test_source_and_destination_escapes_fail_before_copy(self):
        outside = self.root / "outside.qm"
        outside.write_text("outside")
        (self.module.parent / "escape.qm").symlink_to(outside)
        with self.assertRaisesRegex(ValueError, "inside the source root"):
            self.install()
        self.assertFalse(self.destination.exists())
        (self.module.parent / "escape.qm").unlink()
        (self.buildroot / "usr").symlink_to(self.root)
        with self.assertRaisesRegex(ValueError, "escapes the buildroot"):
            self.install()

    def test_invalid_prefix_and_missing_source_are_rejected(self):
        for prefix in (".", "/usr/src/debug", "/usr/src/debug/../escape", "/etc"):
            with self.subTest(prefix=prefix), self.assertRaises(ValueError):
                sources.install_sources(self.buildroot, prefix, ["qlib"], self.source)
        with self.assertRaisesRegex(ValueError, "Missing"):
            sources.install_sources(self.buildroot, self.prefix, ["absent"], self.source)
        self.assertFalse(self.destination.exists())

    def test_outside_directory_and_parent_components_are_rejected(self):
        outside = self.root / "empty"
        outside.mkdir()
        (self.source / "linked").symlink_to(outside)
        for name in (outside, "../empty", "linked", "qlib/../qlib"):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "inside the source root"):
                sources.install_sources(self.buildroot, self.prefix, [name], self.source)
        self.assertFalse(self.destination.exists())

    def test_failed_replacement_preserves_previous_file(self):
        self.install()
        installed = self.destination / "qlib/Probe/Probe.qm"
        previous = installed.read_bytes()
        self.module.write_text("changed")
        with patch.object(sources.os, "replace", side_effect=OSError("write failed")):
            with self.assertRaisesRegex(OSError, "write failed"):
                self.install()
        self.assertEqual(previous, installed.read_bytes())
        self.assertFalse(list(self.destination.rglob(".qore-source-*")))


if __name__ == "__main__":
    unittest.main()
