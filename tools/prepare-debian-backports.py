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


def extract_component(archive, destination):
    """Unpack one orig component without retaining its archive root name."""
    if destination.exists() or destination.is_symlink():
        raise RuntimeError(f"refusing to replace source component: {destination}")
    with tempfile.TemporaryDirectory(dir=destination.parent, prefix="unpack-") as directory:
        extract(archive, directory)
        roots = list(Path(directory).iterdir())
        if len(roots) != 1 or not roots[0].is_dir() or roots[0].is_symlink():
            raise RuntimeError("expected one upstream component directory")
        roots[0].rename(destination)


def verify_signature(source, signature, archive):
    """Verify upstream signatures with the key from the pinned Debian packaging."""
    key = source / "debian/upstream/signing-key.asc"
    with tempfile.TemporaryDirectory(prefix="qore-upstream-key-") as directory:
        keyring = Path(directory) / "upstream.gpg"
        subprocess.run(["gpg", "--batch", "--no-options", "--dearmor", "--output", str(keyring),
                        str(key)], check=True)
        subprocess.run(["gpgv", "--homedir", directory, "--keyring", str(keyring),
                        str(signature), str(archive)], check=True)


def nodejs_packaging(source):
    """Adapt the pinned Debian Node recipe without embedding build-directory paths."""
    rules = source / "debian/rules"
    updated = rules.read_text()
    old = "esbuild --platform=node --bundle /usr/share/nodejs/minimatch/index.cjs"
    if updated.count(old) != 1:
        raise RuntimeError("expected one minimatch bundle entry point")
    # Debian 13's minimatch exports index.js; newer packaging uses index.cjs.
    # Resolve the installed CommonJS entry point through Node's package API.
    # esbuild embeds module names relative to its working directory. Keep that
    # directory fixed; the caller's redirection still writes into the source tree.
    updated = updated.replace(old,
        "(cd /usr/share/nodejs && esbuild --platform=node --bundle "
        "\"$$(node -p 'require.resolve(\"minimatch\")')\")")
    old = "export DEB_BUILD_MAINT_OPTIONS = hardening=+all\n"
    if updated.count(old) != 1 or "include /usr/share/dpkg/buildflags.mk" in updated:
        raise RuntimeError("unexpected Node build-flags setup")
    # Its exported CFLAGS/CXXFLAGS otherwise suppress debhelper's defaults,
    # losing both hardening and reproducible debug/source prefix maps.
    updated = updated.replace(old, old +
        "DPKG_EXPORT_BUILDFLAGS = 1\ninclude /usr/share/dpkg/buildflags.mk\n")
    old = "\tdh_install\n\noverride_dh_dwz:"
    if updated.count(old) != 1:
        raise RuntimeError("unexpected Node installation rules")
    # Keep build-tree RUNPATH for upstream tests, but use the normal
    # multiarch loader path in the installed executable. Do this before
    # debhelper separates debug symbols so both artifacts stay consistent.
    rules.write_text(updated.replace(old,
        "\tdh_install\n"
        "\t# libnode is installed in the standard multiarch library directory.\n"
        "\tchrpath --delete debian/nodejs/usr/bin/node\n\noverride_dh_dwz:"))
    control = source / "debian/control"
    updated = control.read_text()
    old = "Build-Depends:\n"
    if updated.count(old) != 1:
        raise RuntimeError("unexpected Node build dependencies")
    control.write_text(updated.replace(old, old + " chrpath,\n"))


def doxygen_packaging(source, *, configure_manpages=False):
    """Exercise the patched comparator and generated documentation during builds."""
    rules = source / "debian/rules"
    updated = rules.read_text()
    flags = "export DEB_BUILD_MAINT_OPTIONS=reproducible=+fixfilepath\n"
    if updated.count(flags) != 1:
        raise RuntimeError("unexpected Doxygen build-flags setup")
    updated = updated.replace(flags, flags.rstrip() + " hardening=+all\n")
    old = "\ttouch $@\n\nclean:\n"
    if updated.count(old) != 1:
        raise RuntimeError("expected one Doxygen build-stamp completion")
    updated = updated.replace(old,
        "ifeq (,$(filter nocheck,$(DEB_BUILD_OPTIONS) $(DEB_BUILD_PROFILES)))\n"
        "\tctest --test-dir build --output-on-failure --no-tests=error $(NJOBS)\n"
        "\tpython3 debian/tests/compare-strings.py\n"
        "\tpython3 debian/tests/unicode-search.py --doxygen $(CURDIR)/build/bin/doxygen\n" +
        ("\tpython3 debian/tests/manpages.py --man-dir build/man "
         "--doxygen $(CURDIR)/build/bin/doxygen\n" if configure_manpages else "") +
        "endif\n" + old)
    if configure_manpages:
        # The upstream CMake fix replaces Debian's raw-template fallback.
        start = updated.index("\t: # FIXME: man pages not installed when building without docs\n")
        end = updated.index("\tdh_movefiles -pdoxygen-gui", start)
        updated = updated[:start] + updated[end:]
    rules.write_text(updated)
    control = source / "debian/control"
    updated = control.read_text()
    old = "Build-Depends: debhelper-compat (= 13),\n"
    if updated.count(old) != 1:
        raise RuntimeError("unexpected Doxygen build dependencies")
    # Citation tests need BibTeX and its TeX configuration even in binary-arch
    # builds, where Build-Depends-Indep packages are deliberately not installed.
    control.write_text(updated.replace(old, old +
        "  libxml2-utils <!nocheck>,\n"
        "  texlive-base <!nocheck>,\n"))
    shutil.copytree(CONFIG / "doxygen/tests", source / "debian/tests", dirs_exist_ok=True)
    if configure_manpages:
        shutil.copyfile(CONFIG / "doxygen/trixie-tests/manpages.py", source / "debian/tests/manpages.py")
    copyright_file = source / "debian/copyright"
    updated = copyright_file.read_text()
    stanza = "Files: debian/*\nCopyright:\n"
    if updated.count(stanza) != 1:
        raise RuntimeError("unexpected Doxygen packaging copyright stanza")
    copyright_file.write_text(updated.replace(stanza, stanza + " 2026, Qore Technologies, s.r.o.\n"))
    with (source / "debian/tests/control").open("a") as tests:
        tests.write(
            "\nTest-Command: python3 debian/tests/unicode-search.py\n"
            "Depends: doxygen, python3\n"
            "Restrictions: allow-stderr\n"
            "Features: test-name=unicode-search-ordering\n")
        if configure_manpages:
            tests.write(
                "\nTest-Command: python3 debian/tests/manpages.py\n"
                "Depends: doxygen, doxygen-gui, python3\n"
                "Restrictions: allow-stderr\n"
                "Features: test-name=configured-manpages\n")


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
    for component in package.get("orig_components", {}):
        if not re.fullmatch(r"[a-zA-Z0-9][a-zA-Z0-9-]*", component):
            raise ValueError(f"invalid orig component name: {component}")
    orig = download(package["orig_url"], package["orig_sha256"], cache, offline)
    own_recipe = name in ("pyarrow", "apache-arrow")
    packaging = (None if own_recipe else
                 download(package["packaging_url"], package["packaging_sha256"], cache, offline))
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
    if own_recipe:
        shutil.copytree(CONFIG / name, source / "debian")
    else:
        extract(packaging, source)
    extension = ".tar.xz" if package["orig_url"].endswith(".tar.xz") else ".tar.gz"
    orig_path = output / f"{name}_{package['upstream_version']}.orig{extension}"
    shutil.copyfile(orig, orig_path)
    for component, record in package.get("orig_components", {}).items():
        archive = download(record["url"], record["sha256"], cache, offline)
        extension = ".tar.xz" if record["url"].endswith(".tar.xz") else ".tar.gz"
        component_path = output / f"{name}_{package['upstream_version']}.orig-{component}{extension}"
        shutil.copyfile(archive, component_path)
        extract_component(archive, source / component)
    if signature:
        verify_signature(source, signature, orig)
        shutil.copyfile(signature, str(orig_path) + ".asc")
    if name in ("llhttp", "acorn"):
        # Debian 13's dh-nodejs discovers watch-v4 components but does not
        # understand watch-v5's Deb822 Component fields. Declare the pinned
        # components through its supported, explicit configuration instead.
        components = source / "debian/nodejs/additional_components"
        entries = components.read_text().splitlines() if components.exists() else []
        for component in package["orig_components"]:
            metadata = source / component / "package.json"
            if not json.loads(metadata.read_text()).get("name"):
                raise RuntimeError(f"missing Node package name: {component}")
            if component not in entries:
                entries.append(component)
        components.parent.mkdir(exist_ok=True)
        components.write_text("\n".join(entries) + "\n")
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
    elif name == "node-rollup-plugin-buble":
        # The pinned unstable recipe suppresses a TypeScript 6 deprecation.
        # TypeScript 5.2 rejects that value; neither it nor stable's 4.9 needs
        # the suppression. Retain the other Debian fixes and all tests.
        series = source / "debian/patches/series"
        entries = series.read_text().splitlines()
        if entries.count("ts6-tsconfig.patch") != 1:
            raise RuntimeError("expected one TypeScript 6 compatibility patch")
        entries.remove("ts6-tsconfig.patch")
        series.write_text("\n".join(entries) + "\n")
        (series.parent / "ts6-tsconfig.patch").unlink()
        control = source / "debian/control"
        original = control.read_text()
        dependency = " , node-typescript\n"
        if original.count(dependency) != 1:
            raise RuntimeError("unexpected Rollup TypeScript dependency")
        control.write_text(original.replace(dependency,
            " , node-typescript (>= 4.9)\n , node-typescript (<< 6~)\n"))
    elif name == "nodejs":
        nodejs_packaging(source)
    elif name == "llhttp":
        rules = source / "debian/rules"
        updated = rules.read_text()
        old = "\tln -sf /usr/share/nodejs/@types/markdown-it debian/tests/test_modules/mdgator/node_modules/@types/markdown-it"
        if updated.count(old) != 1:
            raise RuntimeError("expected one markdown-it type declaration link")
        updated = updated.replace(old,
            "\ttype_dir=$$(node -p 'require(\"path\").dirname(require.resolve(\"@types/markdown-it/package.json\", "
            "{paths: [require.resolve(\"markdown-it\")]}))'); \\\n"
            "\t\ttest -r \"$$type_dir/index.d.ts\"; \\\n"
            "\t\tln -sf \"$$type_dir\" debian/tests/test_modules/mdgator/node_modules/@types/markdown-it")
        old = "override_dh_auto_install-indep:\n\tdh_auto_install --buildsystem=nodejs"
        if updated.count(old) != 1:
            raise RuntimeError("expected one llhttp JavaScript install override")
        # dh_install copies the generated release directory into libllhttp-source.
        # The Node runtime needs the native library, not a new JS generator package.
        updated = updated.replace(old, "override_dh_auto_install-indep:\n\t# Installed by libllhttp-source.install.")
        rules.write_text(updated)
        for filename in ("node-llhttp.install", "node-llhttp.examples"):
            (source / "debian" / filename).unlink()
        source_install = source / "debian/libllhttp-source.install"
        if source_install.read_text().strip() != "release/* /usr/src/llhttp/":
            raise RuntimeError("unexpected llhttp generated-source install manifest")
        # A full build also writes a host-specific pkg-config file into release/.
        # Architecture: all must contain only the portable generated sources.
        source_install.write_text("".join(f"release/{path} /usr/src/llhttp/\n" for path in (
            "CMakeLists.txt", "LICENSE", "README.md", "common.gypi", "include",
            "libllhttp.map", "libllhttp.pc.in", "llhttp.gyp", "src")))
        overrides = source / "debian/source/lintian-overrides"
        if overrides.read_text().strip() != "llhttp: inconsistency-debian-watch":
            raise RuntimeError("unexpected llhttp source Lintian overrides")
        overrides.unlink()  # This retired tag is itself an error in current Lintian.
        shutil.copytree(CONFIG / "llhttp/tests", source / "debian/tests", dirs_exist_ok=True)
    elif name in ("ada-url", "libuv1"):
        shutil.copytree(CONFIG / name / "tests", source / "debian/tests", dirs_exist_ok=True)
        if name == "libuv1":
            rules = source / "debian/rules"
            updated = rules.read_text()
            old = "\nDEB_BUILD_MAINT_OPTIONS=hardening=+all\n"
            if updated.count(old) != 1:
                raise RuntimeError("expected one unexported libuv hardening setting")
            rules.write_text(updated.replace(old, "\nexport DEB_BUILD_MAINT_OPTIONS=hardening=+all\n"))
    elif name == "cython":
        rules = source / "debian/rules"
        updated = rules.read_text()
        ignored = 'echo "=============== $$P done (FAILURES IGNORED) ===============";'
        if updated.count(ignored) != 1:
            raise RuntimeError("expected one Cython test failure handler")
        if not updated.startswith("#!/usr/bin/make -f\n"):
            raise RuntimeError("expected Cython's executable makefile")
        updated = updated.replace(ignored, 'echo "=============== $$P failed ==============="; exit 1;')
        rules.write_text(updated.replace("#!/usr/bin/make -f\n", "#!/usr/bin/make -f\n"
                                        "export DEB_BUILD_MAINT_OPTIONS = hardening=+all\n", 1))
    elif name == "simdutf":
        # CMake removes the tools' build RPATH during installation, after the
        # linker has hashed it into their build IDs. Make that path relative
        # so independent build directories produce identical tools/debug files.
        rules = source / "debian/rules"
        updated = rules.read_text()
        old = "dh_auto_configure -- -DBUILD_SHARED_LIBS=ON"
        if updated.count(old) != 1:
            raise RuntimeError("expected one simdutf shared-library configuration")
        rules.write_text(updated.replace(old, old + " -DCMAKE_BUILD_RPATH_USE_ORIGIN=ON"))
    elif name == "boost1.90":
        # The engine's gcc bootstrap adds -s and ignores Debian's flags.
        # Its cxx toolset accepts complete caller-supplied flags without
        # stripping, leaving hardening and debug splitting to Debian.
        rules = source / "debian/rules"
        updated = rules.read_text()
        bootstrap = ('cd tools/build && ./bootstrap.sh cxx --cxx="$(CXX)" '
                     '--cxxflags="$(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS)"')
        old = "cd $(bbv2dir) && ./bootstrap.sh --with-toolset=gcc"
        if updated.count(old) != 1:
            raise RuntimeError("expected one Boost.Build tools bootstrap")
        updated = updated.replace(old, bootstrap)
        old = "\t./bootstrap.sh --with-icu=/usr --prefix=$(CURDIR)/debian/tmp/usr"
        if updated.count(old) != 1:
            raise RuntimeError("expected one Boost root bootstrap")
        updated = updated.replace(old, '\t' + bootstrap + '\n\tcp tools/build/b2 $(b2)\n'
                                  + old.replace("./bootstrap.sh ", './bootstrap.sh --with-bjam="$(b2)" '))
        if updated.count("|| cat bootstrap.log") != 1:
            raise RuntimeError("expected one Boost bootstrap error handler")
        updated = updated.replace("|| cat bootstrap.log", "|| { cat bootstrap.log; exit 1; }")
        old = '<cflags>"$(CFLAGS)"'
        if updated.count(old) != 1:
            raise RuntimeError("expected one Boost compiler flags configuration")
        # Context's preprocessed assembly also needs Debian's flags and an
        # assembler debug-prefix map; C/C++ flags do not reach these objects.
        updated = updated.replace(old, old + ' <asmflags>"$(CFLAGS) '
                                  '-Wa,--debug-prefix-map=$(CURDIR)=."')
        rules.write_text(updated)
    elif name == "nats.c":
        # Debian disables the network-dependent upstream tests. Exercise the
        # newly built client with an isolated loopback broker instead.
        rules = source / "debian/rules"
        old = ('override_dh_auto_test:\n'
               '\t# disable tests: they required network\n'
               '\techo "Tests disabled"\n')
        updated = rules.read_text()
        if updated.count(old) != 1:
            raise RuntimeError("expected one NATS disabled-test override")
        updated = updated.replace(old, (
            'include /usr/share/dpkg/architecture.mk\n\n'
            'override_dh_auto_test:\n'
            'ifeq (,$(filter nocheck,$(DEB_BUILD_OPTIONS)))\n'
            '\tdebian/tests/runtime "$(CURDIR)/obj-$(DEB_HOST_GNU_TYPE)"\n'
            'endif\n'))
        rules.write_text(updated)
        shutil.copytree(CONFIG / "nats.c/tests", source / "debian/tests", dirs_exist_ok=True)
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
            "\t\t-DFETCHCONTENT_FULLY_DISCONNECTED=ON \\\n"
            # Python extensions are copied from the build tree by pybuild;
            # disabling only install RPATH would not cover those libraries.
            "\t\t-DCMAKE_SKIP_RPATH=ON \\\n", 1)
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
    if name == "doxygen":
        configure_manpages = "doxygen/configure-manpages-without-docs.patch" in package.get("extra_patches", [])
        doxygen_packaging(source, configure_manpages=configure_manpages)
        added_patches += ["debian/tests/compare-strings.cpp", "debian/tests/compare-strings.py",
                          "debian/tests/unicode-search.py"]
        if configure_manpages:
            added_patches.append("debian/tests/manpages.py")
    if name in ("nghttp2", "nghttp3", "ngtcp2"):
        rules = source / "debian/rules"
        original = rules.read_text()
        header = "#!/usr/bin/make -f\n"
        if not original.startswith(header) or "DEB_BUILD_MAINT_OPTIONS" in original:
            raise RuntimeError(f"unexpected {name} hardening setup")
        # Debian does not enable immediate symbol binding by default. Put the
        # policy in the recipe so OBS and local builders produce the same flags.
        rules.write_text(original.replace(header, header +
                         "\nexport DEB_BUILD_MAINT_OPTIONS = hardening=+all\n", 1))
        added_patches.append("debian/rules")
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
    if name == "llhttp":
        stanzas = control.split("\n\n")
        selected = [s for s in stanzas if not s.startswith("Package: node-llhttp\n")]
        if len(selected) != len(stanzas) - 1:
            raise RuntimeError("expected one llhttp JavaScript package stanza")
        control = "\n\n".join(selected)
    if name == "boost1.90":
        # g++ is already supplied by build-essential; an unversioned duplicate
        # in Build-Depends is rejected by Lintian.
        control, count = re.subn(r"(?<=,)\s*g\+\+,", "", control, count=1)
        if count != 1:
            raise RuntimeError("expected one redundant Boost g++ build dependency")
    if name == "nats.c":
        control = control.replace("Build-Depends:\n", "Build-Depends:\n"
                                  " nats-server <!nocheck>,\n"
                                  " pkgconf <!nocheck>,\n"
                                  " python3 <!nocheck>,\n", 1)
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
    date = package.get("date", manifest["date"])
    entry += f"\n -- {manifest['maintainer']}  {date}\n\n"
    changelog.write_text(entry + (changelog.read_text() if changelog.exists() else ""))
    provenance = {"package": name, "version": version, "ppa": manifest["ppa"], **package}
    (source / "debian/qore-backport.json").write_text(json.dumps(provenance, indent=2) + "\n")
    copyright_file = source / "debian/copyright"
    copyright_text = copyright_file.read_text()
    if name == "qore-tree-sitter":
        copyright_text = copyright_text.replace(
            " 2020-2026 James McCoy <jamessan@debian.org>\n",
            " 2020-2026 James McCoy <jamessan@debian.org>\n 2026 Qore Technologies, s.r.o.\n")
    license_name = package.get("metadata_license", "Expat" if "\nLicense: Expat\n" in copyright_text else "MIT")
    if name in ("llhttp", "acorn"):
        added_patches.append("debian/nodejs/additional_components")
    if name == "llhttp":
        added_patches += ["debian/tests/control", "debian/tests/runtime*"]
    if name == "qore-onnx":
        added_patches += ["debian/control", "debian/rules", "debian/source/format", "debian/tests/*"]
    elif name in ("nats.c", "ada-url", "libuv1"):
        added_patches.append("debian/tests/*")
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
    epoch = parsedate_to_datetime(date).timestamp()
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
