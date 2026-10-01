#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Regression tests for package hardening and reproducibility qualification."""

import copy
import importlib.util
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("artifacts", ROOT / "tools/check-debian-artifacts.py")
artifacts = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(artifacts)


class ArtifactChecksTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="qore-artifact-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def compile(self, *flags):
        if not shutil.which("cc") or not shutil.which("readelf"):
            self.skipTest("cc and readelf are required")
        source = self.root / "main.c"
        source.write_text("int main(void) { return 0; }\n")
        binary = self.root / "binary"
        subprocess.run(["cc", str(source), "-o", str(binary), *flags], check=True, capture_output=True)
        return binary.read_bytes()

    def test_hardened_binary(self):
        data = self.compile("-fPIE", "-pie", "-Wl,-z,relro,-z,now,-z,noexecstack")
        info, failures = artifacts.inspect_elf(data, self.root / "scratch")
        self.assertEqual([], failures)
        self.assertTrue(info["position_independent"])

    def test_insecure_binary_is_rejected(self):
        data = self.compile("-no-pie", "-Wl,-z,norelro,-z,lazy,-z,execstack,-rpath,/build/private")
        _, failures = artifacts.inspect_elf(data, self.root / "scratch")
        self.assertEqual({"non_executable_stack", "relro", "bind_now", "no_rpath", "position_independent"},
                         set(failures))

    def test_private_jvm_policy_is_limited_to_its_package_and_dependency(self):
        if not shutil.which("cc") or not shutil.which("dpkg-deb"):
            self.skipTest("cc and dpkg-deb are required")
        library_source = self.root / "jvm.c"
        library_source.write_text("int jvm_fixture(void) { return 0; }\n")
        subprocess.run(["cc", "-shared", "-fPIC", str(library_source), "-Wl,-soname,libjvm.so",
                        "-o", str(self.root / "libjvm.so")], check=True, capture_output=True)
        package = self.root / "package"
        (package / "DEBIAN").mkdir(parents=True)
        output = self.root / "packages"
        output.mkdir()

        def inspect_package(data, member, name="qore-jni-module", architecture="amd64",
                            depends="openjdk-21-jre-headless", opt_in=True):
            for old in package.rglob("*.qmod"):
                old.unlink()
            target = package / member
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            (package / "DEBIAN/control").write_text(
                f"Package: {name}\nVersion: 1.0-1\nArchitecture: {architecture}\n"
                f"Depends: {depends}\nMaintainer: Qore <david@qore.org>\nDescription: fixture\n")
            subprocess.run(["dpkg-deb", "--root-owner-group", "--build", str(package),
                            str(output / "fixture.deb")], check=True, capture_output=True)
            return artifacts.inspect(output, allow_private_jvm=opt_in)["passed"]

        for architecture, multiarch in (("amd64", "x86_64-linux-gnu"), ("arm64", "aarch64-linux-gnu")):
            path = f"/usr/lib/jvm/java-21-openjdk-{architecture}/lib/server"
            member = f"usr/lib/{multiarch}/qore-modules/jni-api-2.0.qmod"
            flags = ["-fPIE", "-pie", "-Wl,-z,relro,-z,now,-z,noexecstack",
                     "-Wl,--no-as-needed", "-L" + str(self.root), "-ljvm"]
            data = self.compile(*flags, "-Wl,--enable-new-dtags,-rpath," + path)
            with self.subTest(architecture=architecture):
                self.assertTrue(inspect_package(data, member, architecture=architecture))
                self.assertFalse(inspect_package(data, member, architecture=architecture, opt_in=False))
                self.assertFalse(inspect_package(data, member, architecture=architecture, name="other-module"))
                self.assertFalse(inspect_package(data, member, architecture=architecture, depends="libc6"))
                self.assertFalse(inspect_package(data, member, architecture=architecture,
                                                depends="openjdk-21-jre-headless | java-runtime"))
                self.assertFalse(inspect_package(data, "usr/lib/other-api-2.0.qmod", architecture=architecture))
                for extra in ("/tmp", path + ":/tmp", "$ORIGIN", path.replace("21", "25")):
                    bad = self.compile(*flags, "-Wl,--enable-new-dtags,-rpath," + extra)
                    self.assertFalse(inspect_package(bad, member, architecture=architecture))
                bad = self.compile(*flags, "-Wl,--disable-new-dtags,-rpath," + path)
                self.assertFalse(inspect_package(bad, member, architecture=architecture))
                bad = self.compile("-fPIE", "-pie", "-Wl,-z,relro,-z,now,-z,noexecstack,-rpath," + path)
                self.assertFalse(inspect_package(bad, member, architecture=architecture))

    def test_read_execute_stack_is_rejected(self):
        # An executable stack need not also be writable. Exercise a real ELF
        # program header with PF_R | PF_X, which readelf prints as "R E".
        data = bytearray(self.compile("-fPIE", "-pie", "-Wl,-z,relro,-z,now,-z,noexecstack"))
        endian = "<" if data[5] == 1 else ">"
        elf64 = data[4] == 2
        offset = struct.unpack_from(endian + ("Q" if elf64 else "I"), data, 32 if elf64 else 28)[0]
        size, count = struct.unpack_from(endian + "HH", data, 54 if elf64 else 42)
        for index in range(count):
            header = offset + index * size
            if struct.unpack_from(endian + "I", data, header)[0] == 0x6474e551:  # PT_GNU_STACK
                struct.pack_into(endian + "I", data, header + (4 if elf64 else 24), 5)
                break
        else:
            self.fail("Compiler did not emit PT_GNU_STACK")
        _, failures = artifacts.inspect_elf(data, self.root / "scratch")
        self.assertEqual(["non_executable_stack"], failures)

    def test_aot_metadata_corruption(self):
        payload = b"dependencies"
        footer = struct.pack("<Q4sI", len(payload), b"QAMD", 1)
        self.assertIn("QAMD", artifacts.trailers(b"ELF" + payload + footer))
        for invalid in (struct.pack("<Q4sI", 999, b"QAMD", 1),
                        struct.pack("<Q4sI", len(payload), b"QAMD", 2)):
            with self.assertRaises(ValueError):
                artifacts.trailers(payload + invalid)
        with self.assertRaises(ValueError):
            artifacts.trailers((payload + footer) * 2)

    def test_detached_debug_symbols(self):
        if not shutil.which("objcopy") or not shutil.which("strip"):
            self.skipTest("objcopy and strip are required")
        self.compile("-g", "-fPIE", "-pie", "-Wl,--build-id,-z,relro,-z,now,-z,noexecstack")
        binary = self.root / "binary"
        full, _ = artifacts.inspect_elf(binary.read_bytes(), self.root / "scratch")
        build_id = full["build_id"]
        symbols = self.root / (build_id[2:] + ".debug")
        debug_path = f"usr/lib/debug/.build-id/{build_id[:2]}/{symbols.name}"
        subprocess.run(["objcopy", "--only-keep-debug", str(binary), str(symbols)], check=True)
        subprocess.run(["strip", "--strip-unneeded", str(binary)], check=True)
        subprocess.run(["objcopy", "--add-gnu-debuglink=" + str(symbols), str(binary)], check=True)
        runtime, failures = artifacts.inspect_elf(binary.read_bytes(), self.root / "scratch")
        debug, _ = artifacts.inspect_elf(symbols.read_bytes(), self.root / "scratch", debug=True)
        self.assertEqual([], failures)
        packages = {
            "fixture:amd64": {"debug_package": False, "payload": {"usr/bin/fixture": {"elf": runtime}}},
            "fixture-dbgsym:amd64": {"debug_package": True,
                "payload": {debug_path: {"elf": debug}}},
        }
        result, failures = artifacts.check_debug_symbols(packages)
        self.assertEqual({"checked": 1, "passed": True}, result)
        self.assertEqual([], failures)
        for key, value in (("crc32", 0), ("build_id", "wrong"), ("debug_info", False)):
            broken = copy.deepcopy(packages)
            broken["fixture-dbgsym:amd64"]["payload"][debug_path]["elf"][key] = value
            self.assertFalse(artifacts.check_debug_symbols(broken)[0]["passed"])
        misplaced = copy.deepcopy(packages)
        files = misplaced["fixture-dbgsym:amd64"]["payload"]
        files["wrong-directory/" + symbols.name] = files.pop(debug_path)
        self.assertFalse(artifacts.check_debug_symbols(misplaced)[0]["passed"])
        del packages["fixture-dbgsym:amd64"]
        self.assertFalse(artifacts.check_debug_symbols(packages)[0]["passed"])
        self.assertFalse(artifacts.check_debug_symbols({})[0]["passed"])

    def test_real_package_and_changed_payload(self):
        if not shutil.which("dpkg-deb"):
            self.skipTest("dpkg-deb is required")
        package = self.root / "package"
        control = package / "DEBIAN"
        control.mkdir(parents=True)
        (control / "control").write_text("Package: qore-artifact-test\nVersion: 1.0~test1-1\n"
            "Architecture: all\nMaintainer: Qore <david@qore.org>\nDescription: fixture\n")
        payload = package / "usr/share/qore-artifact-test"
        payload.mkdir(parents=True)
        item = payload / "answer"
        item.write_text("42\n")
        debs = self.root / "debs"
        debs.mkdir()
        deb = debs / "fixture.deb"
        def build():
            subprocess.run(["dpkg-deb", "--root-owner-group", "--build", str(package), str(deb)],
                           check=True, capture_output=True)
            return artifacts.inspect(debs)
        left = build()
        self.assertTrue(left["passed"])
        self.assertTrue(artifacts.compare(left, left)["identical"])
        item.write_text("43\n")
        right = build()
        result = artifacts.compare(left, right)
        self.assertFalse(result["identical"])
        self.assertIn("usr/share/qore-artifact-test/answer",
                      result["differences"]["qore-artifact-test:all"]["payload"]["changed"])
        other = copy.deepcopy(right)
        other["packages"]["qore-artifact-test:all"]["version"] = "2.0-1"
        with self.assertRaises(ValueError):
            artifacts.compare(left, other)
        # Ubuntu uses .ddeb for detached symbols, Debian uses -dbgsym .deb;
        # both suffixes must be inspected and participate in duplicate checks.
        shutil.copyfile(deb, debs / "duplicate.ddeb")
        with self.assertRaises(ValueError):
            artifacts.inspect(debs)

    def test_changes_rejects_modified_and_unlisted_packages(self):
        package = self.root / "fixture.deb"
        package.write_bytes(b"package payload")
        manifest = self.root / "fixture.changes"
        manifest.write_text(f"Checksums-Sha256:\n {artifacts.file_digest(package)} {package.stat().st_size} fixture.deb\n")
        self.assertEqual("verified", artifacts.verify_changes(self.root, [package])["status"])
        extra = self.root / "extra.deb"
        extra.write_bytes(b"unlisted")
        with self.assertRaises(ValueError):
            artifacts.verify_changes(self.root, [package, extra])
        package.write_bytes(b"altered payload")
        with self.assertRaises(ValueError):
            artifacts.verify_changes(self.root, [package])


if __name__ == "__main__":
    unittest.main()
