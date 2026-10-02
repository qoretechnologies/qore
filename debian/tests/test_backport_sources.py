#!/usr/bin/python3
# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Exercise orig-component extraction with real tar archives."""

import importlib.util
import io
from pathlib import Path
import tarfile
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("backports", ROOT / "tools/prepare-debian-backports.py")
backports = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(backports)


class OrigComponentTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.archive = self.root / "component.tar.xz"
        self.destination = self.root / "types-node"

    def archive_files(self, names):
        with tarfile.open(self.archive, "w:xz") as archive:
            for name in names:
                entry = tarfile.TarInfo(name)
                entry.size = len(b"component source\n")
                archive.addfile(entry, io.BytesIO(b"component source\n"))

    def test_component_root_is_stripped_without_changing_source(self):
        self.archive_files(["node-types-22.19/index.d.ts", "node-types-22.19/fs/promises.d.ts"])
        backports.extract_component(self.archive, self.destination)
        self.assertEqual(b"component source\n", (self.destination / "index.d.ts").read_bytes())
        self.assertEqual(b"component source\n", (self.destination / "fs/promises.d.ts").read_bytes())
        self.assertEqual([], list(self.root.glob("unpack-*")))

    def test_existing_component_is_preserved(self):
        self.archive_files(["node-types/index.d.ts"])
        self.destination.mkdir()
        original = self.destination / "existing"
        original.write_text("preserve me")
        with self.assertRaisesRegex(RuntimeError, "refusing to replace"):
            backports.extract_component(self.archive, self.destination)
        self.assertEqual("preserve me", original.read_text())

    def test_dangling_destination_symlink_is_rejected(self):
        self.archive_files(["node-types/index.d.ts"])
        self.destination.symlink_to(self.root / "missing")
        with self.assertRaisesRegex(RuntimeError, "refusing to replace"):
            backports.extract_component(self.archive, self.destination)
        self.assertTrue(self.destination.is_symlink())

    def test_ambiguous_root_and_path_escape_are_rejected(self):
        for names, exception in [
                (["one/index.d.ts", "two/index.d.ts"], RuntimeError),
                (["index.d.ts"], RuntimeError),
                (["../../escaped"], tarfile.FilterError)]:
            with self.subTest(names=names):
                self.archive_files(names)
                with self.assertRaises(exception):
                    backports.extract_component(self.archive, self.destination)
                self.assertFalse(self.destination.exists())
                self.assertEqual([], list(self.root.glob("unpack-*")))


if __name__ == "__main__":
    unittest.main()
