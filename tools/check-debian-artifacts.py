#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Inspect built Qore packages, or compare two inspections for reproducibility.

Requires dpkg-deb and readelf. Archives are streamed, never unpacked into the
host filesystem. A differing comparison exits nonzero even when payloads match:
control metadata and archive encoding are also part of reproducibility.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tarfile
import tempfile
import zlib


FOOTER = struct.Struct("<Q4sI")
MAGICS = {b"QAMD", b"QPCM", b"QAOM"}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def file_digest(path):
    with path.open("rb") as stream:
        result = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def verify_changes(directory, packages):
    """Check artifact identity when a binary .changes manifest is supplied.

    This verifies integrity, not the signer's identity. Authentication of the
    source/archive must be established separately before qualification.
    """
    changes = sorted(directory.glob("*.changes"))
    if not changes:
        return {"status": "not supplied"}
    if len(changes) != 1:
        raise ValueError("Use one binary build per inspection directory")
    lines = changes[0].read_text().splitlines()
    index = lines.index("Checksums-Sha256:") + 1
    expected = {}
    for line in lines[index:]:
        if not line.startswith(" "):
            break
        checksum, size, name = line.split()
        if Path(name).name != name or name in expected:
            raise ValueError("Invalid or duplicate .changes filename")
        path = directory / name
        if not path.is_file() or path.stat().st_size != int(size) or file_digest(path) != checksum:
            raise ValueError(f"Artifact does not match .changes: {name}")
        expected[name] = checksum
    if not all(path.name in expected for path in packages):
        raise ValueError("A binary package is absent from the .changes manifest")
    return {"status": "verified", "manifest": changes[0].name, "sha256": file_digest(changes[0])}


def trailers(data):
    """Validate the footer chain and retain independent payload fingerprints."""
    end = len(data)
    result = {}
    while end >= FOOTER.size:
        size, magic, version = FOOTER.unpack_from(data, end - FOOTER.size)
        if magic not in MAGICS:
            break
        name = magic.decode("ascii")
        if version != 1 or name in result or not size or size > end - FOOTER.size:
            raise ValueError(f"Invalid {name} AOT trailer")
        start = end - FOOTER.size - size
        result[name] = digest(data[start:end])
        end = start
    return result


def inspect_elf(data, scratch, debug=False):
    scratch.write_bytes(data)
    info = subprocess.check_output(
        ["readelf", "--wide", "--file-header", "--program-headers", "--dynamic",
         "--section-headers", "--notes", str(scratch)],
        text=True, stderr=subprocess.STDOUT, env={**os.environ, "LC_ALL": "C"})
    kind = re.search(r"Type:\s+(\w+)", info).group(1)
    build_id = re.search(r"Build ID: ([0-9a-f]+)", info)
    symbols = {"build_id": build_id[1] if build_id else None,
               "debug_info": bool(re.search(r"\]\s+\.(?:debug_info|zdebug_info)\s", info))}
    if debug:
        return {"type": kind, **symbols, "crc32": zlib.crc32(data)}, []
    link = re.search(r"\]\s+\.gnu_debuglink\s+\S+\s+[0-9a-f]+\s+([0-9a-f]+)\s+([0-9a-f]+)\s", info)
    if link:
        offset, size = (int(value, 16) for value in link.groups())
        if size < 8 or offset + size > len(data):
            raise ValueError("Invalid .gnu_debuglink section bounds")
        section = data[offset:offset + size]
        name, separator, _ = section[:-4].partition(b"\0")
        if not separator or not name or (len(name) + 4) // 4 * 4 + 4 != size:
            raise ValueError("Invalid .gnu_debuglink filename or padding")
        symbols["debug_link"] = name.decode("utf-8")
        if Path(symbols["debug_link"]).name != symbols["debug_link"]:
            raise ValueError(".gnu_debuglink must contain a filename without directory components")
        symbols["debug_crc32"] = int.from_bytes(section[-4:], "little" if data[5] == 1 else "big")
    if kind not in ("EXEC", "DYN"):
        return {"type": kind, **symbols}, []
    stack = next((line for line in info.splitlines() if "GNU_STACK" in line), "")
    stack_fields = stack.split()
    checks = {
        "non_executable_stack": len(stack_fields) >= 8 and "E" not in "".join(stack_fields[6:-1]),
        "relro": "GNU_RELRO" in info,
        "bind_now": "BIND_NOW" in info or bool(re.search(r"Flags:.*\bNOW\b", info)),
        "no_rpath": "(RPATH)" not in info and "(RUNPATH)" not in info,
        "no_textrel": "(TEXTREL)" not in info,
    }
    # ET_EXEC is not PIE; shared libraries and PIE executables are ET_DYN.
    checks["position_independent"] = kind == "DYN"
    return {"type": kind, **symbols, **checks}, [key for key, passed in checks.items() if not passed]


def archive_members(path, control=False, scratch=None, debug=False):
    flag = "--ctrl-tarfile" if control else "--fsys-tarfile"
    process = subprocess.Popen(["dpkg-deb", flag, str(path)], stdout=subprocess.PIPE)
    members, findings = {}, []
    try:
        with tarfile.open(fileobj=process.stdout, mode="r|") as archive:
            for member in archive:
                name = member.name.removeprefix("./")
                if name in members:
                    raise ValueError(f"Duplicate archive member: {name}")
                entry = {
                    "mode": member.mode, "uid": member.uid, "gid": member.gid,
                    "mtime": member.mtime, "type": member.type.decode("ascii"),
                    "link": member.linkname,
                }
                if member.isfile():
                    data = archive.extractfile(member).read()
                    entry["sha256"] = digest(data)
                    entry["size"] = len(data)
                    if not control and data.startswith(b"\x7fELF"):
                        entry["elf"], failures = inspect_elf(data, scratch, debug=debug)
                        findings.extend(f"{name}: {failure}" for failure in failures)
                    if not control and name.endswith(".qmod") and "-api-" not in name:
                        entry["trailers"] = trailers(data)
                        if "QAMD" not in entry["trailers"]:
                            findings.append(f"{name}: missing AOT dependency metadata")
                members[name] = entry
    finally:
        process.stdout.close()
        status = process.wait()
    if status:
        raise subprocess.CalledProcessError(status, process.args)
    return members, findings


def check_debug_symbols(packages):
    """Match each shipped runtime ELF to its actual detached debug file."""
    debug_files = set()
    for package in packages.values():
        if not package["debug_package"]:
            continue
        for name, member in package["payload"].items():
            elf = member.get("elf", {})
            if elf.get("build_id") and elf.get("debug_info"):
                debug_files.add((elf["build_id"], name, elf["crc32"]))
    findings, checked = [], 0
    for key, package in packages.items():
        if package["debug_package"]:
            continue
        for name, member in package["payload"].items():
            elf = member.get("elf", {})
            if elf.get("type") not in ("EXEC", "DYN"):
                continue
            checked += 1
            build_id = elf.get("build_id") or ""
            expected_path = f"usr/lib/debug/.build-id/{build_id[:2]}/{build_id[2:]}.debug"
            valid = (build_id, expected_path, elf.get("debug_crc32")) in debug_files \
                and Path(expected_path).name == elf.get("debug_link")
            if not valid:
                findings.append(f"{key}: {name}: no matching debug file (build ID, debuglink, CRC and DWARF required)")
    if not checked:
        findings.append("No runtime ELF files found for debug-symbol qualification")
    return {"checked": checked, "passed": not findings}, findings


def inspect(directory, require_debug=False):
    packages, findings = {}, []
    inputs = sorted([*directory.glob("*.deb"), *directory.glob("*.ddeb")])
    if not inputs:
        raise ValueError(f"No binary packages in {directory}")
    manifest = verify_changes(directory, inputs)
    with tempfile.TemporaryDirectory(prefix="qore-elf-check-") as temporary:
        scratch = Path(temporary) / "object"
        for path in inputs:
            fields = subprocess.check_output(
                ["dpkg-deb", "--show", "--showformat=${Package}\t${Version}\t${Architecture}", str(path)],
                text=True).split("\t")
            package, version, architecture = fields
            key = f"{package}:{architecture}"
            if key in packages:
                raise ValueError(f"Multiple versions of {key} in input")
            debug = package.endswith("-dbgsym")
            payload, errors = archive_members(path, scratch=scratch, debug=debug)
            control, _ = archive_members(path, control=True)
            findings.extend(f"{key}: {error}" for error in errors)
            packages[key] = {"version": version, "sha256": file_digest(path), "debug_package": debug,
                             "payload": payload, "control": control}
    symbols = {"status": "not requested"}
    if require_debug:
        symbols, errors = check_debug_symbols(packages)
        findings.extend(errors)
    return {"schema": 1, "packages": packages, "findings": findings, "checksums": manifest,
            "debug_symbols": symbols, "passed": not findings}


def compare(left, right):
    if left.get("schema") != 1 or right.get("schema") != 1:
        raise ValueError("Unsupported inspection schema")
    a, b = left["packages"], right["packages"]
    if set(a) != set(b) or any(a[key]["version"] != b[key]["version"] for key in a.keys() & b.keys()):
        raise ValueError("Compare the same package versions, architectures and build profiles")
    differences = {}
    for key in a:
        if a[key]["sha256"] == b[key]["sha256"]:
            continue
        package = {}
        for section in ("payload", "control"):
            first, second = a[key][section], b[key][section]
            changed = sorted(name for name in first.keys() & second.keys() if first[name] != second[name])
            package[section] = {"added": sorted(second.keys() - first.keys()),
                                "removed": sorted(first.keys() - second.keys()), "changed": changed}
        differences[key] = package
    return {"schema": 1, "identical": not differences, "differences": differences,
            "left_inspection_passed": left["passed"], "right_inspection_passed": right["passed"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    check = commands.add_parser("inspect", help="Inspect all .deb files in a directory")
    check.add_argument("directory", type=Path)
    check.add_argument("--require-debug-symbols", action="store_true",
                       help="Require matching detached DWARF, build IDs and debuglink CRCs for every runtime ELF")
    diff = commands.add_parser("compare", help="Compare JSON reports from two builds")
    diff.add_argument("left", type=Path)
    diff.add_argument("right", type=Path)
    for command in (check, diff):
        command.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "inspect":
        result = inspect(args.directory, require_debug=args.require_debug_symbols)
        passed = result["passed"]
        print(f"Inspected {len(result['packages'])} packages; {len(result['findings'])} findings")
    else:
        result = compare(json.loads(args.left.read_text()), json.loads(args.right.read_text()))
        passed = result["identical"] and result["left_inspection_passed"] and result["right_inspection_passed"]
        print(f"Packages differing between builds: {len(result['differences'])}")
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
