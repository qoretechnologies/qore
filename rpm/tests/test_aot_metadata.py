#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
"""Check actual ELF stripping and failure handling for Qore AOT trailers."""

import importlib.util
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch


spec = importlib.util.spec_from_file_location(
    "preserve_aot_metadata", Path(__file__).resolve().parents[1] / "preserve-aot-metadata.py"
)
metadata = importlib.util.module_from_spec(spec)
spec.loader.exec_module(metadata)


class AotMetadataTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        source = self.root / "probe.c"
        source.write_text("int package_probe(void) { return 42; }\n")
        self.module = self.root / "Probe.qmod"
        subprocess.run(["cc", "-shared", "-fPIC", "-g", "-o", str(self.module), str(source)], check=True)
        self.original_size = self.module.stat().st_size
        # The packaging layer preserves payloads opaquely, in their original order.
        self.trailers = b"".join(
            payload + struct.pack("<Q4sI", len(payload), magic, 1)
            for magic, payload in [(b"QAOM", b"optional"), (b"QPCM", b"locations"), (b"QAMD", b"dependencies")]
        )
        with self.module.open("ab") as stream:
            stream.write(self.trailers)

    def test_strip_and_debuglink(self):
        nested = self.root / "Separated" / "Separated.qmod"
        nested.parent.mkdir()
        nested.write_bytes(self.module.read_bytes())
        debug = self.root / "Probe.debug"
        subprocess.run(["objcopy", "--only-keep-debug", str(self.module), str(debug)], check=True)
        metadata.preserve(self.root, ["strip", "--strip-unneeded", str(self.module), str(nested)])
        self.assertLess(self.module.stat().st_size, self.original_size)
        self.assertLess(nested.stat().st_size, self.original_size)
        self.assertEqual(metadata.read_trailers(nested), self.trailers)
        metadata.preserve(self.root, ["objcopy", "--add-gnu-debuglink=" + str(debug), str(self.module)])
        self.assertEqual(metadata.read_trailers(self.module), self.trailers)
        sections = subprocess.check_output(["readelf", "-S", str(self.module)], text=True)
        self.assertIn(".gnu_debuglink", sections)
        self.assertNotIn(".debug_info", sections)

    def test_noop_does_not_duplicate(self):
        original = self.module.read_bytes()
        metadata.preserve(self.root, ["true"])
        self.assertEqual(self.module.read_bytes(), original)

    def test_native_module_and_symlink(self):
        subprocess.run(["strip", "--strip-unneeded", str(self.module)], check=True)
        self.assertEqual(metadata.read_trailers(self.module), b"")
        (self.root / "Alias.qmod").symlink_to(self.module)
        metadata.preserve(self.root, ["true"])
        self.assertEqual(metadata.read_trailers(self.module), b"")

    def test_command_failure_restores_metadata(self):
        with self.assertRaises(subprocess.CalledProcessError):
            metadata.preserve(self.root, ["sh", "-c", 'strip --strip-unneeded "$1"; exit 7', "sh", str(self.module)])
        self.assertEqual(metadata.read_trailers(self.module), self.trailers)

    def test_invalid_footer_rejected_before_command(self):
        for size, magic, version in [(2**64 - 1, b"QAMD", 1), (4, b"QAOM", 2), (0, b"QPCM", 1)]:
            with self.subTest(size=size, magic=magic, version=version):
                self.module.write_bytes(b"prefix" + struct.pack("<Q4sI", size, magic, version))
                with self.assertRaises(ValueError):
                    metadata.preserve(self.root, ["false"])

    def test_restoration_keeps_post_processing_mtime(self):
        timestamp = 1600000000123456789
        os.utime(self.module, ns=(timestamp, timestamp))
        metadata.preserve(self.root, ["strip", "--preserve-dates", "--strip-unneeded", str(self.module)])
        self.assertEqual(self.module.stat().st_mtime_ns, timestamp)
        self.assertEqual(metadata.read_trailers(self.module), self.trailers)

    def test_restore_failure_does_not_abandon_remaining_modules(self):
        second = self.root / "Second.qmod"
        second.write_bytes(self.module.read_bytes())
        run = subprocess.run

        def damage(*args, **kwargs):
            run(["strip", "--strip-unneeded", str(self.module), str(second)], check=True)
            with self.module.open("ab") as stream:
                stream.write(struct.pack("<Q4sI", 4, b"QAMD", 2))
            raise subprocess.CalledProcessError(7, "post-processing")

        with patch.object(metadata.subprocess, "run", side_effect=damage):
            with self.assertRaises(ExceptionGroup) as raised:
                metadata.preserve(self.root, ["post-processing"])
        self.assertEqual(len(raised.exception.exceptions), 2)
        self.assertIsInstance(raised.exception.exceptions[0], subprocess.CalledProcessError)
        self.assertEqual(metadata.read_trailers(second), self.trailers)

    def test_post_processing_symlink_cannot_modify_another_file(self):
        victim = self.root / "unrelated"
        victim.write_bytes(b"untouched")

        def replace(*args, **kwargs):
            self.module.rename(self.root / "saved-original")
            self.module.symlink_to(victim)

        with patch.object(metadata.subprocess, "run", side_effect=replace):
            with self.assertRaisesRegex(ValueError, "symlink"):
                metadata.preserve(self.root, ["post-processing"])
        self.assertEqual(victim.read_bytes(), b"untouched")


if __name__ == "__main__":
    unittest.main()
