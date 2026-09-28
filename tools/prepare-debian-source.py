#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Prepare a reproducible unsigned testing source package from a Git commit.

Native build dependencies are checked by the binary builder, not this source
archiver. No signing or upload is performed. Use a numeric snapshot serial:
3.0.0~git20260928.1-0qore1~ubuntu26.04, followed by .2 for the next upload.
"""

import argparse
from datetime import datetime, timezone
from email.utils import format_datetime
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tarfile


ROOT = Path(__file__).resolve().parents[1]


def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args], text=True).strip()


def snapshot_changelog(text, version, suite, commit, date, maintainer):
    """Version the pending entry without retaining a future release as history."""
    header = re.match(r"qore \([^)]+\) [^;\n]+;([^\n]+)\n", text)
    signature = re.search(r"\n -- [^\n]+\n", text)
    if not header or not signature or signature.start() < header.end():
        raise ValueError("Expected a complete pending Qore changelog entry")
    history = text[signature.end():]
    for previous in re.findall(r"^qore \(([^)]+)\) ", history, re.MULTILINE):
        if subprocess.run(["dpkg", "--compare-versions", previous, "lt", version]).returncode:
            raise ValueError(f"Snapshot {version} must follow historical version {previous}")
    body = text[header.end():signature.start()].strip("\n")
    return (f"qore ({version}) {suite};{header[1]}\n\n"
            f"  * Testing snapshot from commit {commit}.\n" + body + "\n\n"
            f" -- {maintainer}  {date}\n" + history)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ref", default="HEAD", help="Committed source revision (default: HEAD)")
    parser.add_argument("--version", required=True, help="Complete Debian prerelease version")
    parser.add_argument("--suite", required=True, help="Target distribution codename")
    parser.add_argument("--previous-version", help="Latest accepted version in the target archive")
    parser.add_argument("--maintainer", default="David Nichols <david@qore.org>")
    parser.add_argument("--output", required=True, type=Path, help="New directory outside the checkout")
    args = parser.parse_args()
    if not re.fullmatch(r"[a-z][a-z0-9-]*", args.suite):
        parser.error("Use a distribution codename for --suite")
    match = re.fullmatch(r"(\d+\.\d+\.\d+)~git\d{8}\.\d+-[a-zA-Z0-9.+~]+", args.version)
    if not match:
        parser.error("Use a prerelease version with a numeric date/serial, as shown in --help")
    if "\n" in args.maintainer or "\r" in args.maintainer:
        parser.error("Maintainer must be one line")
    subprocess.run(["dpkg", "--validate-version", args.version], check=True)
    subprocess.run(["dpkg", "--compare-versions", args.version, "lt", match[1]], check=True)
    if args.previous_version:
        subprocess.run(["dpkg", "--compare-versions", args.version, "gt", args.previous_version], check=True)
    output = args.output.resolve()
    if output == ROOT or ROOT in output.parents:
        parser.error("Keep generated sources outside the checkout")
    commit = git("rev-parse", "--verify", args.ref + "^{commit}")
    cmake = git("show", commit + ":CMakeLists.txt")
    core_version = ".".join(re.search(r"set\(VERSION_" + part + r"\s+(\d+)\)", cmake).group(1)
                            for part in ("MAJOR", "MINOR", "PATCH"))
    if match[1] != core_version:
        parser.error(f"The selected commit declares Qore {core_version}, not {match[1]}")
    timestamp = int(git("show", "-s", "--format=%ct", commit))
    output.mkdir(parents=True, exist_ok=False)
    upstream = args.version.rsplit("-", 1)[0]
    prefix = "qore-" + upstream
    archive = output / f"qore_{upstream}.orig.tar.xz"
    process = subprocess.Popen(["git", "-C", str(ROOT), "archive", "--format=tar",
        "--prefix=" + prefix + "/", commit, "--", ".", ":!debian"], stdout=subprocess.PIPE)
    with archive.open("wb") as stream:
        compressed = subprocess.run(["xz", "-T1", "-6"], stdin=process.stdout, stdout=stream)
    process.stdout.close()
    if process.wait():
        raise RuntimeError("git archive failed")
    compressed.check_returncode()
    # Only committed Git files are read; the data filter also rejects archive
    # entries that would write outside the workspace on supporting Python versions.
    with tarfile.open(archive) as tar:
        tar.extractall(output, filter="data")
    source = output / prefix
    packaging = output / "packaging.tar"
    with packaging.open("wb") as stream:
        subprocess.run(["git", "-C", str(ROOT), "archive", commit, "debian"], stdout=stream, check=True)
    with tarfile.open(packaging) as tar:
        tar.extractall(source, filter="data")
    date = format_datetime(datetime.fromtimestamp(timestamp, timezone.utc))
    changelog = source / "debian/changelog"
    changelog.write_text(snapshot_changelog(changelog.read_text(), args.version, args.suite,
                                            commit, date, args.maintainer))
    # Git archive uses the commit timestamp; also normalize the generated changelog.
    os.utime(changelog, (timestamp, timestamp))
    with (output / "source-build.log").open("w") as log:
        subprocess.run(["dpkg-buildpackage", "-S", "-d", "-nc", "-us", "-uc", "-sa"],
                       cwd=source, stdout=log, stderr=subprocess.STDOUT, check=True,
                       env={**os.environ, "SOURCE_DATE_EPOCH": str(timestamp)})
    hashes = {}
    for path in sorted(output.iterdir()):
        if path.is_file() and path.name.startswith("qore_"):
            with path.open("rb") as stream:
                checksum = hashlib.sha256()
                for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                    checksum.update(chunk)
            hashes[path.name] = checksum.hexdigest()
    manifest = {"commit": commit, "version": args.version, "suite": args.suite,
                "source_date_epoch": timestamp, "sha256": hashes}
    (output / "source-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Unsigned source prepared from {commit}: {output}")


if __name__ == "__main__":
    main()
