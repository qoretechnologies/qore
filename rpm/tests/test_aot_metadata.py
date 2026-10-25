#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
"""Check actual ELF stripping and failure handling for Qore AOT module metadata."""

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


def module_info_record(name: bytes) -> bytes:
    """Return a module metadata record: the module name, no dependencies and no optional modules."""
    payload = struct.pack("<I", len(name)) + name + struct.pack("<II", 0, 0)
    return struct.pack("<III", 0x494D4151, 1, len(payload)) + payload


def module_info_note(record: bytes) -> bytes:
    """Return the ELF note holding a module metadata record, as qcc emits it."""
    note = struct.pack("<III", 5, len(record), 0x494D4151) + b"Qore\0\0\0\0" + record
    return note + b"\0" * (-len(note) % 4)


class AotMetadataTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.record = module_info_record(b"Probe")
        self.module = self.root / "Probe.qmod"
        self.build(self.module, self.record)
        self.original_size = self.module.stat().st_size

    def build(self, module: Path, record: bytes) -> None:
        """Build a shared object that holds the record in its module metadata note section, if not empty."""
        source = module.with_suffix(".c")
        text = "int package_probe(void) { return 42; }\n"
        if record:
            data = ", ".join(str(value) for value in module_info_note(record))
            text += ("__attribute__((used, section(\".note.qore.modinfo\"), aligned(4)))\n"
                     f"static const unsigned char module_info[] = {{ {data} }};\n")
        source.write_text(text)
        subprocess.run(["cc", "-shared", "-fPIC", "-g", "-o", str(module), str(source)], check=True)
        source.unlink()

    def test_section_is_a_loaded_note(self):
        self.assertEqual(metadata.read_module_info(self.module), self.record)
        notes = subprocess.check_output(["readelf", "--wide", "--section-headers", "--notes", str(self.module)],
                                        text=True, env=dict(os.environ, LC_ALL="C"))
        self.assertRegex(notes, r"\.note\.qore\.modinfo\s+NOTE\s+\S+\s+\S+\s+\S+\s+00\s+A")
        self.assertRegex(notes, r"Qore\s+0x[0-9a-f]+\s+.*0x494d4151")

    def test_strip_and_debuglink(self):
        nested = self.root / "Separated" / "Separated.qmod"
        nested.parent.mkdir()
        nested.write_bytes(self.module.read_bytes())
        debug = self.root / "Probe.debug"
        subprocess.run(["objcopy", "--only-keep-debug", str(self.module), str(debug)], check=True)
        metadata.preserve(self.root, ["strip", "--strip-unneeded", str(self.module), str(nested)])
        self.assertLess(self.module.stat().st_size, self.original_size)
        self.assertLess(nested.stat().st_size, self.original_size)
        self.assertEqual(metadata.read_module_info(nested), self.record)
        metadata.preserve(self.root, ["objcopy", "--add-gnu-debuglink=" + str(debug), str(self.module)])
        self.assertEqual(metadata.read_module_info(self.module), self.record)
        sections = subprocess.check_output(["readelf", "-S", str(self.module)], text=True)
        self.assertIn(".gnu_debuglink", sections)
        self.assertNotIn(".debug_info", sections)
        # every section that is not loaded goes, but the note is loaded
        metadata.preserve(self.root, ["strip", "--strip-all", str(self.module)])
        self.assertEqual(metadata.read_module_info(self.module), self.record)

    def test_check_does_not_modify_modules(self):
        timestamp = 1600000000123456789
        os.utime(self.module, ns=(timestamp, timestamp))
        original = self.module.read_bytes()
        metadata.preserve(self.root, ["true"])
        self.assertEqual(self.module.read_bytes(), original)
        self.assertEqual(self.module.stat().st_mtime_ns, timestamp)

    def test_native_module_and_symlink(self):
        native = self.root / "native-api-2.0.qmod"
        self.build(native, b"")
        self.assertEqual(metadata.read_module_info(native), b"")
        (self.root / "Alias.qmod").symlink_to(self.module)
        metadata.preserve(self.root, ["strip", "--strip-unneeded", str(native)])
        self.assertEqual(metadata.read_module_info(native), b"")

    def test_removed_metadata_fails(self):
        with self.assertRaisesRegex(ValueError, "removed the AOT module metadata"):
            metadata.preserve(self.root, ["objcopy", "--remove-section=.note.qore.modinfo", str(self.module)])

    def test_changed_metadata_fails(self):
        data = self.module.read_bytes()
        offset = data.index(self.record)

        def change(*args, **kwargs):
            with self.module.open("r+b") as stream:
                stream.seek(offset + len(self.record) - 1)
                stream.write(b"\1")

        with patch.object(metadata.subprocess, "run", side_effect=change):
            with self.assertRaisesRegex(ValueError, "changed the AOT module metadata"):
                metadata.preserve(self.root, ["post-processing"])

    def test_command_failure_is_reported(self):
        with self.assertRaises(subprocess.CalledProcessError):
            metadata.preserve(self.root, ["sh", "-c", 'strip --strip-unneeded "$1"; exit 7', "sh", str(self.module)])
        self.assertEqual(metadata.read_module_info(self.module), self.record)

    def test_invalid_module_rejected_before_command(self):
        marker = self.root / "ran"
        data = self.module.read_bytes()
        for size in (20, 64 + 4):
            with self.subTest(size=size):
                self.module.write_bytes(data[:size])
                with self.assertRaises(ValueError):
                    metadata.preserve(self.root, ["touch", str(marker)])
                self.assertFalse(marker.exists())

    def test_check_failure_does_not_abandon_remaining_modules(self):
        second = self.root / "Second.qmod"
        second.write_bytes(self.module.read_bytes())
        run = subprocess.run

        def damage(*args, **kwargs):
            run(["objcopy", "--remove-section=.note.qore.modinfo", str(self.module)], check=True)
            run(["objcopy", "--remove-section=.note.qore.modinfo", str(second)], check=True)
            raise subprocess.CalledProcessError(7, "post-processing")

        with patch.object(metadata.subprocess, "run", side_effect=damage):
            with self.assertRaises(ExceptionGroup) as raised:
                metadata.preserve(self.root, ["post-processing"])
        self.assertEqual(len(raised.exception.exceptions), 3)
        self.assertIsInstance(raised.exception.exceptions[0], subprocess.CalledProcessError)

    def test_post_processing_symlink_is_reported(self):
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
