#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
"""Run an ELF editing command and check that it keeps Qore's AOT module metadata.

Usage: preserve-aot-metadata.py MODULE_ROOT COMMAND [ARG ...]
qcc stores the metadata of an AOT module -- its name, its dependencies and the
optional modules that were unavailable when it was compiled -- in the allocated
ELF note section .note.qore.modinfo, which strip and objcopy keep; the format is
described in include/qore/intern/QoreAOTBinary.h. The loader refuses an AOT
module without it, so packaging fails here instead if Debian's
strip/debuglink operations removed or changed it in a module under MODULE_ROOT.
"""

from pathlib import Path
import struct
import subprocess
import sys


NOTE_NAME = b"Qore\0"
NOTE_TYPE = 0x494D4151  # 'QAMI', the magic number of the record
PT_NOTE = 4
PN_XNUM = 0xFFFF


def _align(value: int, alignment: int) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def read_module_info(path: Path) -> bytes:
    """Return the AOT module metadata record of a module, or empty bytes without one.

    Raises ValueError for a truncated ELF image or for more than one record.
    """
    data = path.read_bytes()
    if len(data) < 16 or data[:4] != b"\x7fELF" or data[5] != 1 or data[4] not in (1, 2):
        return b""
    elf64 = data[4] == 2
    header = struct.Struct("<16xHHIQQQIHHHHHH" if elf64 else "<16xHHIIIIIHHHHHH")
    if len(data) < header.size:
        raise ValueError(f"{path}: truncated ELF header")
    fields = header.unpack_from(data)
    phoff, shoff, phentsize, phnum = fields[4], fields[5], fields[8], fields[9]
    if not phnum:
        return b""
    if phnum == PN_XNUM:
        # the number of program headers is the sh_info field of the first section header
        info_offset = shoff + (44 if elf64 else 28)
        if info_offset + 4 > len(data):
            raise ValueError(f"{path}: truncated ELF section header")
        phnum = struct.unpack_from("<I", data, info_offset)[0]
    phdr = struct.Struct("<IIQQQQQQ" if elf64 else "<IIIIIIII")
    if phentsize < phdr.size or phoff + phnum * phentsize > len(data):
        raise ValueError(f"{path}: truncated or invalid ELF program headers")
    records = []
    for index in range(phnum):
        entry = phdr.unpack_from(data, phoff + index * phentsize)
        if entry[0] != PT_NOTE:
            continue
        if elf64:
            offset, size, alignment = entry[2], entry[5], entry[7]
        else:
            offset, size, alignment = entry[1], entry[4], entry[7]
        if offset + size > len(data):
            raise ValueError(f"{path}: truncated ELF note segment")
        segment = data[offset:offset + size]
        # in a segment aligned to 8 bytes, the name and description start at offsets aligned to 8 bytes
        alignment = 8 if alignment == 8 else 4
        position = 0
        while len(segment) - position >= 12:
            name_size, desc_size, note_type = struct.unpack_from("<III", segment, position)
            name = position + 12
            desc = _align(name + name_size, alignment)
            if desc + desc_size > len(segment):
                break  # the notes of other owners are not checked
            if note_type == NOTE_TYPE and segment[name:name + name_size] == NOTE_NAME:
                records.append(segment[desc:desc + desc_size])
            position = min(_align(desc + desc_size, alignment), len(segment))
    if len(records) > 1:
        raise ValueError(f"{path}: more than one AOT module metadata note")
    return records[0] if records else b""


def preserve(root: Path, command: list[str]) -> None:
    """Run the command, then check the metadata of every module, also when the command fails."""
    saved = {}
    for path in sorted(root.rglob("*.qmod")):
        if path.is_symlink() or not path.is_file():
            continue
        record = read_module_info(path)
        if record:
            saved[path] = record
    errors = []
    try:
        subprocess.run(command, check=True)
    except BaseException as error:
        errors.append(error)
    # A failed check must not prevent the check of the remaining modules.
    for path, record in saved.items():
        try:
            if path.is_symlink():
                raise ValueError(f"{path}: ELF editing replaced the module with a symlink")
            current = read_module_info(path)
            if not current:
                raise ValueError(f"{path}: ELF editing removed the AOT module metadata")
            if current != record:
                raise ValueError(f"{path}: ELF editing changed the AOT module metadata")
        except Exception as error:
            errors.append(error)
    if len(errors) == 1:
        raise errors[0]
    if errors:
        raise BaseExceptionGroup("ELF processing and the AOT module metadata check failed", errors)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    preserve(Path(sys.argv[1]), sys.argv[2:])
