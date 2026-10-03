#!/usr/bin/python3
# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Exercise orig-component extraction with real tar archives."""

import importlib.util
import io
from pathlib import Path
import shutil
import subprocess
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


@unittest.skipUnless(shutil.which("esbuild") and shutil.which("node") and shutil.which("make"),
                     "Node bundle regression requires esbuild, node and make")
class NodeBundleTest(unittest.TestCase):
    def test_bundle_is_independent_of_source_directory(self):
        # Exercise the generated recipe with the real system minimatch and esbuild.
        # Different path depths reproduce the native-versus-OBS embedded-name bug.
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            outputs = []
            for relative in ("native", "obs/different/path/depth"):
                source = root / relative
                (source / "debian").mkdir(parents=True)
                (source / "debian/control").write_text("Source: nodejs\nBuild-Depends:\n esbuild\n")
                (source / "debian/rules").write_text(
                    "export DEB_BUILD_MAINT_OPTIONS = hardening=+all\n"
                    "bundle:\n"
                    "\tmkdir -p deps/minimatch\n"
                    "\tesbuild --platform=node --bundle /usr/share/nodejs/minimatch/index.cjs"
                    " > deps/minimatch/index.js\n"
                    "install:\n\tdh_install\n\noverride_dh_dwz:\n")
                backports.nodejs_packaging(source)
                subprocess.run(["make", "--no-print-directory", "-f", "debian/rules", "bundle"],
                               cwd=source, check=True, capture_output=True, text=True)
                bundle = source / "deps/minimatch/index.js"
                outputs.append(bundle.read_bytes())
                subprocess.run(["node", "-e", """
                    const assert = require('node:assert/strict');
                    const bundled = require(process.argv[1]);
                    const expected = require('minimatch');
                    for (const [path, pattern] of [
                        ['a/b.txt', '**/*.txt'], ['a/b.txt', '*.txt'],
                        ['report.csv', '*.{csv,json}'], ['data.json', '!*.csv']
                    ]) {
                        assert.equal(bundled.minimatch(path, pattern), expected.minimatch(path, pattern));
                    }
                """, str(bundle)], cwd="/usr/share/nodejs", check=True,
                               capture_output=True, text=True)
            self.assertEqual(outputs[0], outputs[1])
            self.assertNotIn(str(root).encode(), outputs[0])


if __name__ == "__main__":
    unittest.main()
