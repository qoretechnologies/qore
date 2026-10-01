#!/usr/bin/python3
# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Prepare pinned, unsigned dependency sources for Debian and Ubuntu.

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


def runtime_packaging(source, template="tree-sitter"):
    """Retain provenance while applying a narrowly scoped library recipe."""
    debian = source / "debian"
    retained = {"copyright", "changelog"}
    if template == "tree-sitter":
        retained.update({"libtree-sitter0.26.symbols", "libtree-sitter0.26.install",
                         "libtree-sitter-dev.install"})
    for path in debian.iterdir():
        if path.name not in retained:
            if path.is_dir():
                shutil.rmtree(path)
            else:
                path.unlink()
    shutil.copytree(CONFIG / template, debian, dirs_exist_ok=True)


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
    elif name == "qore-onnx":
        runtime_packaging(source, "qore-onnx")
    elif name == "onnxruntime":
        # Debian's 1.23.2 recipe still links the Python copy as 1.23.1.
        # Match the shared-library filename generated by upstream CMake.
        rules = source / "debian/rules"
        updated, count = re.subn(r"^FULLVERSION=.+$", "FULLVERSION=$(shell cat VERSION_NUMBER)",
                                 rules.read_text(), flags=re.MULTILINE)
        if count != 1:
            raise RuntimeError("expected one ONNX Runtime FULLVERSION assignment")
        # Match the private ONNX archive's protobuf ABI. Lite protobuf also
        # avoids registering a second copy of Debian ONNX's proto descriptors.
        if updated.count("-Donnxruntime_USE_FULL_PROTOBUF=ON") != 1:
            raise RuntimeError("expected one ONNX Runtime protobuf configuration")
        updated = updated.replace("-Donnxruntime_USE_FULL_PROTOBUF=ON",
                                  "-Donnxruntime_USE_FULL_PROTOBUF=OFF")
        if updated.count("\tdh_auto_configure -- \\\n") != 1:
            raise RuntimeError("expected one ONNX Runtime configure command")
        updated = updated.replace(
            "\tdh_auto_configure -- \\\n",
            "\tdh_auto_configure -- \\\n"
            "\t\t-Donnx_DIR=/usr/lib/$(DEB_HOST_MULTIARCH)/qore-onnx/cmake/ONNX \\\n"
            "\t\t-DFETCHCONTENT_FULLY_DISCONNECTED=ON \\\n", 1)
        # Track the statically incorporated source for rebuilds and carry its
        # complete copyright/license record in every incorporating binary.
        updated += (
            "\n# Keep provider debug information in each ELF; avoid the string-only\n"
            "# multifile produced by dwz 0.15 (Debian bug #1106590).\n"
            "override_dh_dwz:\n"
            "\tdh_dwz --no-dwz-multifile\n"
            "\nexecute_after_dh_installdocs:\n"
            "\tfor p in libonnxruntime1.23 libonnxruntime-providers python3-onnxruntime; do \\\n"
            "\t\tinstall -D -m644 /usr/share/doc/libqore-onnx-dev/copyright \\\n"
            "\t\t  debian/$$p/usr/share/doc/$$p/copyright.qore-onnx; \\\n"
            "\tdone\n"
            "\noverride_dh_gencontrol:\n"
            "\tdh_gencontrol -plibonnxruntime1.23 -plibonnxruntime-providers "
            "-ppython3-onnxruntime -- \\\n"
            "\t\t-Vqore:Static-Built-Using=\"$(shell dpkg-query -W "
            "-f='$${source:Package} (= $${source:Version})' libqore-onnx-dev)\"\n"
            "\tdh_gencontrol --remaining-packages\n")
        rules.write_text(updated)
        shutil.copytree(CONFIG / "onnxruntime/tests", source / "debian/tests", dirs_exist_ok=True)
        with (source / "debian/tests/control").open("a") as tests:
            tests.write(
                "\nTest-Command: python3 debian/tests/coexistence.py && "
                "python3 debian/tests/coexistence.py runtime-first\n"
                "Depends: @, python3-numpy, python3-onnx, python3-torch\n"
                "Restrictions: allow-stderr\n"
                "Architecture: amd64 arm64\n"
                "Features: test-name=stable-onnx-pytorch-coexistence\n")
    added_patches = []
    for relative in package.get("extra_patches", []):
        patch = (CONFIG / relative).resolve()
        if not patch.is_relative_to(CONFIG.resolve()) or not patch.is_file():
            raise ValueError(f"invalid backport patch: {relative}")
        target = source / "debian/patches" / patch.name
        target.parent.mkdir(exist_ok=True)
        if target.exists():
            raise RuntimeError(f"backport patch already exists: {patch.name}")
        shutil.copyfile(patch, target)
        with (target.parent / "series").open("a") as series:
            series.write(patch.name + "\n")
        added_patches.append("debian/patches/" + patch.name)
    control_path = source / "debian/control"
    control = control_path.read_text()
    if name == "onnxruntime":
        if control.count("libonnx-dev (>= 1.20.0)") != 1:
            raise RuntimeError("expected one ONNX Runtime ONNX build dependency")
        control = control.replace("libonnx-dev (>= 1.20.0)", "libqore-onnx-dev (>= 1.20.0)")
        for binary in ("libonnxruntime1.23", "libonnxruntime-providers", "python3-onnxruntime"):
            field = f"Package: {binary}\n"
            if control.count(field) != 1:
                raise RuntimeError(f"expected one binary stanza: {binary}")
            control = control.replace(field, field + "Static-Built-Using: ${qore:Static-Built-Using}\n")
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
    license_name = package.get("metadata_license", "Expat" if "\nLicense: Expat\n" in copyright_text else "MIT")
    if name == "qore-onnx":
        added_patches += ["debian/control", "debian/rules", "debian/source/format", "debian/tests/*"]
    elif name == "onnxruntime":
        added_patches.append("debian/tests/coexistence.py")
        # This patch has joint upstream/Qore copyright, recorded separately.
        added_patches.remove("debian/patches/gemm-dynamic-batch.patch")
    copyright_text += ("\nFiles: debian/qore-backport.json" + "".join("\n " + p for p in added_patches) + "\n"
                       "Copyright: 2026 Qore Technologies, s.r.o.\n"
                       f"License: {license_name}\n")
    if name == "onnxruntime":
        copyright_text += ("\nFiles: debian/patches/gemm-dynamic-batch.patch\n"
                           "Copyright: 2026 Microsoft Corporation\n"
                           " 2026 Qore Technologies, s.r.o.\n"
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
    manifest = json.loads((CONFIG / "sources.json").read_text())
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", default=manifest["distribution"],
                        choices=[manifest["distribution"], *manifest.get("targets", {})],
                        help="target distribution (default: %(default)s)")
    parser.add_argument("--output", type=Path, required=True,
                        help="external workspace for generated sources and uploads")
    parser.add_argument("--cache", type=Path, required=True, help="SHA-256 download cache")
    parser.add_argument("--offline", action="store_true", help="use verified cached downloads only")
    parser.add_argument("packages", nargs="*", help="selected source packages; default: all")
    args = parser.parse_args()
    target = manifest.get("targets", {}).get(args.suite, {})
    packages = dict(manifest["packages"])
    for name, overrides in target.get("packages", {}).items():
        packages[name] = {**packages.get(name, {}), **overrides}
    manifest.update(target)
    manifest["packages"] = packages
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
        package = dict(manifest["packages"][name])
        package["changes"] = [line.format(distribution_name=manifest["distribution_name"])
                              for line in package["changes"]]
        prepare(name, package, manifest, destination, cache, args.offline)


if __name__ == "__main__":
    main()
