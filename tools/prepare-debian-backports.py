#!/usr/bin/python3
# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Prepare pinned, unsigned dependency source uploads for Qore's testing PPA.

Downloads happen here, before any package build. This command never signs,
uploads, installs packages, or changes the host's APT configuration.
"""

import argparse
import difflib
from email.utils import parsedate_to_datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile
import textwrap
import urllib.request


ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "debian/backports"


def download(url, digest, cache, offline):
    """Return a cache entry only after validating its pinned SHA-256 digest."""
    path = cache / digest
    if not path.exists():
        if offline:
            raise RuntimeError(f"missing cached source: {url}")
        if not url.startswith("https://"):
            raise ValueError(f"source URL must use HTTPS: {url}")
        with urllib.request.urlopen(url, timeout=60) as response:
            with tempfile.NamedTemporaryFile(dir=cache, delete=False) as output:
                temporary = Path(output.name)
                try:
                    shutil.copyfileobj(response, output)
                except BaseException:
                    temporary.unlink(missing_ok=True)
                    raise
        try:
            with temporary.open("rb") as source:
                if hashlib.file_digest(source, "sha256").hexdigest() != digest:
                    raise RuntimeError(f"SHA-256 mismatch: {url}")
            temporary.replace(path)
        finally:
            temporary.unlink(missing_ok=True)
    with path.open("rb") as source:
        if hashlib.file_digest(source, "sha256").hexdigest() != digest:
            raise RuntimeError(f"corrupt download cache: {path}")
    return path


def extract(archive, destination):
    # The data filter rejects escaping paths and unsafe link targets, and removes
    # privileged permission bits. No downloaded code executes during extraction.
    with tarfile.open(archive) as source:
        source.extractall(destination, filter="data")


def verify_signature(source, signature, archive):
    """Verify upstream signatures with the key from the pinned Debian packaging."""
    key = source / "debian/upstream/signing-key.asc"
    with tempfile.TemporaryDirectory(prefix="qore-upstream-key-") as directory:
        keyring = Path(directory) / "upstream.gpg"
        subprocess.run(["gpg", "--batch", "--no-options", "--dearmor", "--output", str(keyring),
                        str(key)], check=True)
        subprocess.run(["gpgv", "--homedir", directory, "--keyring", str(keyring),
                        str(signature), str(archive)], check=True)


def cares_patch(source):
    """Generate a quilt patch from the same patcher used by FetchContent."""
    relative = Path("src/lib/ares_process.c")
    before = (source / relative).read_text()
    with tempfile.TemporaryDirectory(prefix="qore-cares-patch-") as directory:
        patched = Path(directory) / relative
        patched.parent.mkdir(parents=True)
        patched.write_text(before)
        subprocess.run(["cmake", "-P", str(ROOT / "cmake/PatchCaresLostQuery.cmake")],
                       cwd=directory, check=True)
        after = patched.read_text()
    if before == after:
        raise RuntimeError("c-ares source already contains the fixes; review the backport pin")
    header = (
        "Description: Preserve re-used DNS query IDs and reject freed queries\n"
        " Carry both fixes from Qore's cmake/PatchCaresLostQuery.cmake.\n"
        " A second detach must not remove a newer query's ID mapping. The send\n"
        " path must compare the query identity, not just the existence of an ID.\n"
        "Author: Qore Technologies, s.r.o.\n"
        "Bug: https://github.com/c-ares/c-ares/issues/1280\n"
        "Forwarded: https://github.com/c-ares/c-ares/pull/1256\n"
        "Last-Update: 2026-09-27\n\n"
    )
    delta = "".join(difflib.unified_diff(before.splitlines(keepends=True),
                                      after.splitlines(keepends=True),
                                      fromfile=f"a/{relative}", tofile=f"b/{relative}"))
    patches = source / "debian/patches"
    patches.mkdir(exist_ok=True)
    (patches / "qore-lost-query.patch").write_text(header + delta)
    with (patches / "series").open("a") as series:
        series.write("qore-lost-query.patch\n")


def runtime_packaging(source):
    """Retain the C library's packaging without the unrelated Rust toolchain."""
    debian = source / "debian"
    retained = {"copyright", "changelog", "libtree-sitter0.26.symbols",
                "libtree-sitter0.26.install", "libtree-sitter-dev.install"}
    for path in debian.iterdir():
        if path.name not in retained:
            if path.is_dir():
                shutil.rmtree(path)
            else:
                path.unlink()
    shutil.copytree(CONFIG / "tree-sitter", debian, dirs_exist_ok=True)


def prepare(name, package, manifest, destination, cache, offline):
    version = f"{package['upstream_version']}-{package['revision']}{manifest['version_suffix']}"
    subprocess.run(["dpkg", "--compare-versions", version, "gt", package["upstream_version"]],
                   check=True)
    output = destination / name
    if output.exists():
        raise RuntimeError(f"refusing to replace existing package workspace: {output}")
    orig = download(package["orig_url"], package["orig_sha256"], cache, offline)
    packaging = download(package["packaging_url"], package["packaging_sha256"], cache, offline)
    signature = (download(package["signature_url"], package["signature_sha256"], cache, offline)
                 if "signature_url" in package else None)
    output.mkdir()
    source = output / f"{name}-{package['upstream_version']}"
    with tempfile.TemporaryDirectory(dir=output, prefix="unpack-") as directory:
        extract(orig, directory)
        roots = list(Path(directory).iterdir())
        if len(roots) != 1 or not roots[0].is_dir() or roots[0].is_symlink():
            raise RuntimeError(f"expected one upstream source directory: {name}")
        roots[0].rename(source)
    extract(packaging, source)
    extension = ".tar.xz" if package["orig_url"].endswith(".tar.xz") else ".tar.gz"
    orig_path = output / f"{name}_{package['upstream_version']}.orig{extension}"
    shutil.copyfile(orig, orig_path)
    if signature:
        verify_signature(source, signature, orig)
        shutil.copyfile(signature, str(orig_path) + ".asc")
    if name == "c-ares":
        cares_patch(source)
    elif name == "qore-tree-sitter":
        runtime_packaging(source)
    elif name == "nghttp3":
        symbols = source / "debian/libnghttp3-9.symbols"
        lines = symbols.read_text().splitlines()
        lines.append(" nghttp3_conn_close_stream2@Base 1.18.0")
        symbols.write_text(lines[0] + "\n" + "\n".join(sorted(lines[1:])) + "\n")
    control_path = source / "debian/control"
    control = control_path.read_text()
    original = re.search(r"^Maintainer: (.+)$", control, re.MULTILINE).group(1)
    control = re.sub(r"^Maintainer: .+$", f"Maintainer: {manifest['maintainer']}",
                     control, count=1, flags=re.MULTILINE)
    if original != manifest["maintainer"]:
        control = control.replace("\nMaintainer:", f"\nXSBC-Original-Maintainer: {original}\nMaintainer:", 1)
    control = re.sub(r"^Uploaders:.*\n(?:[ \t].*\n)*", "", control, flags=re.MULTILINE)
    control = control.replace("debhelper-compat (= 14)", "debhelper-compat (= 13)")
    control = re.sub(r"^Vcs-Git:.*$", "Vcs-Git: https://github.com/qorelanguage/qore.git",
                     control, flags=re.MULTILINE)
    control = re.sub(r"^Vcs-Browser:.*$",
                     "Vcs-Browser: https://github.com/qorelanguage/qore/tree/develop/debian/backports",
                     control, flags=re.MULTILINE)
    control_path.write_text(control)
    changelog = source / "debian/changelog"
    entry = f"{name} ({version}) {manifest['distribution']}; urgency=medium\n\n"
    entry += "".join(textwrap.fill(line, width=78, initial_indent="  * ", subsequent_indent="    ")
                     + "\n" for line in package["changes"])
    entry += f"\n -- {manifest['maintainer']}  {manifest['date']}\n\n"
    changelog.write_text(entry + changelog.read_text())
    provenance = {"package": name, "version": version, "ppa": manifest["ppa"], **package}
    (source / "debian/qore-backport.json").write_text(json.dumps(provenance, indent=2) + "\n")
    copyright_file = source / "debian/copyright"
    copyright_text = copyright_file.read_text()
    if name == "qore-tree-sitter":
        copyright_text = copyright_text.replace(
            " 2020-2026 James McCoy <jamessan@debian.org>\n",
            " 2020-2026 James McCoy <jamessan@debian.org>\n 2026 Qore Technologies, s.r.o.\n")
    license_name = "Expat" if "\nLicense: Expat\n" in copyright_text else "MIT"
    copyright_text += ("\nFiles: debian/qore-backport.json\n"
                       "Copyright: 2026 Qore Technologies, s.r.o.\n"
                       f"License: {license_name}\n")
    copyright_file.write_text(copyright_text)
    # Stabilize generated packaging timestamps even if the build host's clock
    # precedes the changelog date. Upstream archive contents remain untouched.
    epoch = parsedate_to_datetime(manifest["date"]).timestamp()
    for path in (source / "debian").rglob("*"):
        os.utime(path, (epoch, epoch), follow_symlinks=False)
    os.utime(source / "debian", (epoch, epoch))
    # A pristine source-only build does not need native Build-Depends. -d is
    # restricted to this preparation step; the subsequent binary build must
    # satisfy all declared dependencies and run its tests.
    subprocess.run(["dpkg-buildpackage", "-S", "-sa", "-us", "-uc", "-d", "-nc"],
                   cwd=source, check=True)
    print(f"Prepared {output / (name + '_' + version + '_source.changes')}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True,
                        help="external workspace for generated sources and uploads")
    parser.add_argument("--cache", type=Path, required=True, help="SHA-256 download cache")
    parser.add_argument("--offline", action="store_true", help="use verified cached downloads only")
    parser.add_argument("packages", nargs="*", help="selected source packages; default: all")
    args = parser.parse_args()
    manifest = json.loads((CONFIG / "sources.json").read_text())
    names = args.packages or list(manifest["packages"])
    if len(names) != len(set(names)) or any(name not in manifest["packages"] for name in names):
        parser.error("select distinct package names from " + ", ".join(manifest["packages"]))
    destination = args.output.resolve()
    if destination.is_relative_to(ROOT):
        parser.error("generated packages must be outside the Qore checkout")
    cache = args.cache.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    cache.mkdir(parents=True, exist_ok=True)
    for name in names:
        prepare(name, manifest["packages"][name], manifest, destination, cache, args.offline)


if __name__ == "__main__":
    main()
