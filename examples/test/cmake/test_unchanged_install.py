#!/usr/bin/env python3
"""Verify that installing unchanged content leaves installed files alone, with CMAKE_EXECUTABLE (or cmake).

An install rewrote every installed AOT .qmod with a new inode and modification time even when it was byte-identical
to the installed one, so anything that decides from file times whether a module changed saw a change after every
install.  These tests install tiny fixture projects twice and require the second install to leave the installed
files, links and manifests exactly as they were, while a changed module is still published through a new inode.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")
# a fixed modification time far in the past: an install that rewrites a file cannot keep it by accident
OLD_MTIME_NS = 978307200 * 1000000000


def cmake_version():
    result = subprocess.run([CMAKE, "--version"], capture_output=True, text=True, timeout=30)
    match = re.search(r"version (\d+)\.(\d+)", result.stdout)
    if result.returncode != 0 or not match:
        raise RuntimeError("cannot determine the CMake version: " + result.stdout + result.stderr)
    return int(match.group(1)), int(match.group(2))


class UnchangedInstallTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        version = cmake_version()
        # cmake --install appeared in CMake 3.15; the declared floor is 3.14, where the install script is run directly
        cls.has_install_option = version >= (3, 15)
        # the AOT user module rules declare DEPFILEs, which Makefile generators support from CMake 3.20 on
        cls.aot_generator = []
        if version < (3, 20):
            ninja = shutil.which("ninja")
            cls.aot_generator = ["-G", "Ninja", f"-DCMAKE_MAKE_PROGRAM={ninja}"] if ninja else None

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qore-unchanged-install-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def run_cmake(self, *args, env=None):
        result = subprocess.run([CMAKE, *map(str, args)], capture_output=True, text=True, timeout=60, env=env)
        output = result.stdout + result.stderr
        self.assertEqual(0, result.returncode, output)
        self.assertNotIn("CMake Warning", output)
        return output

    def configure(self, source, build, prefix, body, aot_rules=False):
        generator = []
        if aot_rules:
            if self.aot_generator is None:
                self.skipTest("AOT user module rules need CMake 3.20 with Makefile generators, or Ninja")
            generator = self.aot_generator
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.14...3.31)\n"
            "project(UnchangedInstall NONE)\n"
            f'include("{ROOT.as_posix()}/cmake/QoreMacros.cmake")\n' + body)
        return self.run_cmake("-S", source, "-B", build, f"-DCMAKE_INSTALL_PREFIX={prefix}", *generator)

    def install(self, build, destdir=None, component=None):
        env = {k: v for k, v in os.environ.items() if k != "DESTDIR"}
        if destdir is not None:
            env["DESTDIR"] = str(destdir)
        if self.has_install_option:
            args = ["--install", build]
            if component is not None:
                args += ["--component", component]
        else:
            args = ([f"-DCMAKE_INSTALL_COMPONENT={component}"] if component is not None else []) \
                + ["-P", build / "cmake_install.cmake"]
        return self.run_cmake(*args, env=env)

    @staticmethod
    def manifest(build, component=None):
        name = f"install_manifest_{component}.txt" if component else "install_manifest.txt"
        return (build / name).read_text().splitlines()

    @staticmethod
    def age(path):
        """Gives an installed file a fixed old modification time and returns its identity"""
        os.utime(path, ns=(OLD_MTIME_NS, OLD_MTIME_NS))
        info = path.stat()
        return info.st_ino, info.st_mtime_ns

    @staticmethod
    def identity(path):
        info = path.stat()
        return info.st_ino, info.st_mtime_ns

    @staticmethod
    def link_identity(path):
        info = path.lstat()
        return info.st_ino, info.st_mtime_ns

    def test_qmod_reinstall_keeps_unchanged_files(self):
        source, build, prefix = self.root / "source with spaces", self.root / "build", self.root / "prefix"
        source.mkdir()
        qmod = source / "Fixture.qmod"
        qmod.write_bytes(b"AOT module generation 1\n")
        self.configure(source, build, prefix,
                       f'QORE_INSTALL_QMOD_ATOMIC("{qmod.as_posix()}" "lib/qore-modules" "Fixture.qmod" "")\n')
        installed = prefix / "lib/qore-modules/Fixture.qmod"

        output = self.install(build)
        self.assertEqual(qmod.read_bytes(), installed.read_bytes())
        self.assertIn(f"-- Installing: {installed}", output)
        self.assertEqual([str(installed)], self.manifest(build))

        before = self.age(installed)
        output = self.install(build)
        self.assertEqual(before, self.identity(installed), "an unchanged module is neither copied nor renamed")
        self.assertIn(f"-- Up-to-date: {installed}", output)
        self.assertNotIn("Installing:", output)
        # make uninstall reads the manifest, so a module left alone must still be listed
        self.assertEqual([str(installed)], self.manifest(build))

        # same size, different content: the comparison is by content, not by size or time
        qmod.write_bytes(b"AOT module generation 2\n")
        output = self.install(build)
        self.assertNotEqual(before[0], installed.stat().st_ino, "a changed module is published through a new inode")
        self.assertNotEqual(OLD_MTIME_NS, installed.stat().st_mtime_ns)
        self.assertEqual(qmod.read_bytes(), installed.read_bytes())
        self.assertIn(f"-- Installing: {installed}", output)
        self.assertEqual([str(installed)], self.manifest(build))
        self.assertEqual(["Fixture.qmod"], sorted(p.name for p in installed.parent.iterdir()))

    def test_qmod_reinstall_with_destdir(self):
        source, build, stage = self.root / "source", self.root / "build", self.root / "stage"
        source.mkdir()
        qmod = source / "Fixture.qmod"
        qmod.write_bytes(b"staged AOT module\n")
        # an absolute module dir that cannot exist, so only the DESTDIR-staged installation can satisfy the checks
        module_dir = "/opt/qore-unchanged-install-fixture/modules"
        self.configure(source, build, self.root / "unused-prefix",
                       f'QORE_INSTALL_QMOD_ATOMIC("{qmod.as_posix()}" "{module_dir}" "Fixture.qmod" "")\n')
        installed = Path(str(stage) + module_dir) / "Fixture.qmod"

        output = self.install(build, destdir=stage)
        self.assertEqual(qmod.read_bytes(), installed.read_bytes())
        self.assertIn(f"-- Installing: {installed}", output)
        # like file(INSTALL), the manifest lists the installed path without DESTDIR
        self.assertEqual([module_dir + "/Fixture.qmod"], self.manifest(build))

        before = self.age(installed)
        output = self.install(build, destdir=stage)
        self.assertEqual(before, self.identity(installed))
        self.assertIn(f"-- Up-to-date: {installed}", output)
        self.assertEqual([module_dir + "/Fixture.qmod"], self.manifest(build))

        qmod.write_bytes(b"restaged AOT module\n")
        self.install(build, destdir=stage)
        self.assertNotEqual(before[0], installed.stat().st_ino)
        self.assertEqual(qmod.read_bytes(), installed.read_bytes())
        self.assertFalse(Path("/opt/qore-unchanged-install-fixture").exists(), "DESTDIR staging stays below DESTDIR")

    def test_qmod_install_replaces_what_is_not_the_module(self):
        source, build, prefix = self.root / "source", self.root / "build", self.root / "prefix"
        source.mkdir()
        qmod = source / "Fixture.qmod"
        qmod.write_bytes(b"AOT module\n")
        self.configure(source, build, prefix,
                       f'QORE_INSTALL_QMOD_ATOMIC("{qmod.as_posix()}" "lib" "Fixture.qmod" "")\n')
        installed = prefix / "lib/Fixture.qmod"
        installed.parent.mkdir(parents=True)

        # a link to an identical file is replaced by the module, not left in place
        twin = self.root / "twin.qmod"
        twin.write_bytes(qmod.read_bytes())
        installed.symlink_to(twin)
        output = self.install(build)
        self.assertFalse(installed.is_symlink())
        self.assertEqual(qmod.read_bytes(), installed.read_bytes())
        self.assertEqual(qmod.read_bytes(), twin.read_bytes(), "the link's target is not written through")
        self.assertIn(f"-- Installing: {installed}", output)

        # the staging file of an interrupted install is removed, also when the module is up to date
        staged = installed.with_name("Fixture.qmod.inst.tmp")
        staged.write_bytes(b"partial")
        before = self.age(installed)
        self.install(build)
        self.assertFalse(staged.exists())
        self.assertEqual(before, self.identity(installed))

    def test_qmod_install_messages_follow_cmake_install_message(self):
        for setting, first, second in (("LAZY", "Installing", None), ("NEVER", None, None),
                                       ("ALWAYS", "Installing", "Up-to-date")):
            with self.subTest(setting=setting):
                source, build, prefix = (self.root / setting / name for name in ("source", "build", "prefix"))
                source.mkdir(parents=True)
                qmod = source / "Fixture.qmod"
                qmod.write_bytes(b"AOT module\n")
                self.configure(source, build, prefix,
                               f"set(CMAKE_INSTALL_MESSAGE {setting})\n"
                               f'QORE_INSTALL_QMOD_ATOMIC("{qmod.as_posix()}" "lib" "Fixture.qmod" "")\n')
                installed = prefix / "lib/Fixture.qmod"
                for expected, output in ((first, self.install(build)), (second, self.install(build))):
                    reported = re.findall(r"-- (Installing|Up-to-date): " + re.escape(str(installed)), output)
                    self.assertEqual([expected] if expected else [], reported, output)
                self.assertEqual([str(installed)], self.manifest(build))

    def write_modules(self, source):
        """Writes a flat module and a split-dir module with a jar/ dir and a resource dir"""
        (source / "qlib/Split/jar").mkdir(parents=True)
        (source / "qlib/Split/schemas").mkdir()
        (source / "qlib/Flat.qm").write_text("%modern\n")
        (source / "qlib/Split/Split.qm").write_text("%modern\n")
        (source / "qlib/Split/Part.qc").write_text("# part\n")
        (source / "qlib/Split/logo.svg").write_text("<svg/>\n")
        (source / "qlib/Split/jar/fixture.jar").write_bytes(b"jar\n")
        (source / "qlib/Split/schemas/schema.json").write_text("{}\n")

    @staticmethod
    def module_project(registration, user_dir, aot_dir, components=None, aot=True):
        body = (f"set(QORE_BUILD_AOT_MODULES {'ON' if aot else 'OFF'})\n"
                # the install tests stage the built .qmods themselves, so qcc never runs
                'set(QORE_QCC_EXECUTABLE "${CMAKE_COMMAND}")\n'
                "set(QORE_AOT_LINK_SOURCE_MODULES OFF)\n"
                "set(QORE_GENERATE_JAVA_BINDINGS OFF)\n"
                "set(DOXYGEN_FOUND FALSE)\n"
                "set(CMAKE_DISABLE_FIND_PACKAGE_Doxygen TRUE)\n"
                f'set(QORE_USER_MODULES_DIR "{user_dir}")\n'
                f'set(QORE_AOT_MODULES_DIR "{aot_dir}")\n')
        if components:
            body += (f"set(QORE_QM_SOURCE_INSTALL_COMPONENT {components[0]})\n"
                     f"set(QORE_QMOD_INSTALL_COMPONENT {components[1]})\n")
        return body + "".join(registration.format(module) for module in ("qlib/Flat.qm", "qlib/Split"))

    @staticmethod
    def stage_qmods(build, generation):
        qmods = {"Flat": build / "qlib-qmod/Flat.qmod", "Split": build / "qlib-qmod/Split/Split.qmod"}
        for name, path in qmods.items():
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(f"{name} AOT module generation {generation}\n".encode())
        return qmods

    def test_module_reinstall_keeps_unchanged_qmods_and_links(self):
        registrations = ('qore_user_module("{}")\n', 'qore_external_user_module("{}" "")\n')
        # qore's own layout: sources and qmods in separate directories and components, relative to the prefix
        for registration in registrations:
            with self.subTest(registration=registration.strip()), tempfile.TemporaryDirectory(
                    prefix="qore-unchanged-modules-") as directory:
                root = Path(directory)
                source, build, prefix = root / "source", root / "build", root / "prefix"
                self.write_modules(source)
                self.configure(source, build, prefix, self.module_project(
                    registration, "share/qore-modules", "lib/qore-modules", ("sources", "qmods")), aot_rules=True)
                built = self.stage_qmods(build, 1)
                aot_dir = prefix / "lib/qore-modules"
                installed = {"Flat": aot_dir / "Flat.qmod", "Split": aot_dir / "Split/Split.qmod"}
                links = [aot_dir / "Split/schemas"]
                if "external" in registration:
                    # only the external module macro installs a module's jar/ dir beside its sources
                    links.append(aot_dir / "Split/jar")

                self.install(build)
                for name in installed:
                    self.assertEqual(built[name].read_bytes(), installed[name].read_bytes())
                for link in links:
                    self.assertTrue(link.is_symlink(), link)
                    self.assertEqual(os.path.join("../../../share/qore-modules/Split", link.name),
                                     os.readlink(link))
                self.assertTrue((aot_dir / "Split/schemas/schema.json").is_file())
                manifest = self.manifest(build)
                for path in [*installed.values(), *links]:
                    self.assertIn(str(path), manifest)

                before = {name: self.age(path) for name, path in installed.items()}
                links_before = {link: self.link_identity(link) for link in links}
                output = self.install(build)
                for name, path in installed.items():
                    self.assertEqual(before[name], self.identity(path),
                                     f"reinstalling an unchanged {name}.qmod leaves it alone")
                    self.assertIn(f"-- Up-to-date: {path}", output)
                for link in links:
                    self.assertEqual(links_before[link], self.link_identity(link),
                                     "a link that is already correct is left alone")
                    self.assertIn(f"-- Up-to-date: {link}", output)
                self.assertEqual(manifest, self.manifest(build))

                # only the changed module is published again
                built["Split"].write_bytes(b"Split AOT module generation 2\n")
                output = self.install(build)
                self.assertEqual(before["Flat"], self.identity(installed["Flat"]))
                self.assertNotEqual(before["Split"][0], installed["Split"].stat().st_ino)
                self.assertEqual(built["Split"].read_bytes(), installed["Split"].read_bytes())
                self.assertIn(f"-- Installing: {installed['Split']}", output)

                # installing the sources alone still removes the qmods that would be loaded instead of them...
                self.install(build, component="sources")
                for path in installed.values():
                    self.assertFalse(path.exists(), path)
                self.assertTrue((prefix / "share/qore-modules/Flat.qm").is_file())
                # ...and installing the qmods publishes them again
                self.install(build, component="qmods")
                for name, path in installed.items():
                    self.assertEqual(built[name].read_bytes(), path.read_bytes())

    def test_module_reinstall_in_one_component_with_destdir(self):
        # the external module default: sources and qmods share one component, below absolute directories
        source, build, stage = self.root / "source", self.root / "build", self.root / "stage"
        self.write_modules(source)
        user_dir = "/opt/qore-unchanged-install-fixture/share/qore-modules"
        aot_dir = "/opt/qore-unchanged-install-fixture/lib/qore-modules"
        self.configure(source, build, self.root / "unused-prefix", self.module_project(
            'qore_external_user_module("{}" "")\n', user_dir, aot_dir), aot_rules=True)
        built = self.stage_qmods(build, 1)
        staged = Path(str(stage) + aot_dir)
        installed = {"Flat": staged / "Flat.qmod", "Split": staged / "Split/Split.qmod"}
        links = (staged / "Split/jar", staged / "Split/schemas")

        self.install(build, destdir=stage)
        for name, path in installed.items():
            self.assertEqual(built[name].read_bytes(), path.read_bytes())
        for link in links:
            self.assertEqual(os.path.join("../../../share/qore-modules/Split", link.name), os.readlink(link))
        manifest = self.manifest(build)
        self.assertIn(aot_dir + "/Split/Split.qmod", manifest)
        self.assertIn(aot_dir + "/Split/jar", manifest)

        for component in (None, "Unspecified"):
            with self.subTest(component=component):
                before = {name: self.age(path) for name, path in installed.items()}
                links_before = {link: self.link_identity(link) for link in links}
                self.install(build, destdir=stage, component=component)
                for name, path in installed.items():
                    self.assertEqual(before[name], self.identity(path))
                for link in links:
                    self.assertEqual(links_before[link], self.link_identity(link))
        self.assertFalse(Path("/opt/qore-unchanged-install-fixture").exists())

    def test_qmods_beside_sources_are_kept(self):
        source, build, prefix = self.root / "source", self.root / "build", self.root / "prefix"
        self.write_modules(source)
        self.configure(source, build, prefix, self.module_project(
            'qore_user_module("{}")\n', "share/qore-modules", "share/qore-modules"), aot_rules=True)
        built = self.stage_qmods(build, 1)
        module_dir = prefix / "share/qore-modules"
        installed = {"Flat": module_dir / "Flat.qmod", "Split": module_dir / "Split/Split.qmod"}

        self.install(build)
        for name, path in installed.items():
            self.assertEqual(built[name].read_bytes(), path.read_bytes())
        before = {name: self.age(path) for name, path in installed.items()}
        self.install(build)
        for name, path in installed.items():
            self.assertEqual(before[name], self.identity(path),
                             "a .qmod installed beside its sources is not removed as a stale one first")

    def test_source_install_without_aot_removes_installed_qmods(self):
        # a build that no longer compiles the modules must not leave older qmods to be loaded instead of the sources
        source, build, prefix = self.root / "source", self.root / "build", self.root / "prefix"
        self.write_modules(source)
        self.configure(source, build, prefix, self.module_project(
            'qore_user_module("{}")\n', "share/qore-modules", "lib/qore-modules", aot=False))
        stale = [prefix / "lib/qore-modules/Flat.qmod", prefix / "lib/qore-modules/Split/Split.qmod",
                 prefix / "share/qore-modules/Flat.qmod", prefix / "share/qore-modules/Split/Split.qmod"]
        for path in stale:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"stale AOT module\n")
        self.install(build)
        for path in stale:
            self.assertFalse(path.exists(), path)
        self.assertTrue((prefix / "share/qore-modules/Split/Split.qm").is_file())

    def test_unchanged_catalog_manifest_is_kept(self):
        source, build, prefix = self.root / "source", self.root / "build", self.root / "prefix"
        catalog = '{"schema_version":1,"locales":{"root":{"messages":{}}}}\n'
        (source / "catalogs/provider.first").mkdir(parents=True)
        (source / "catalogs/provider.first/root.json").write_text(catalog)
        self.configure(source, build, prefix,
                       'set(QORE_CATALOG_DIR "${CMAKE_INSTALL_PREFIX}/share/qore/i18n")\n'
                       'QORE_INSTALL_USER_MODULE_CATALOGS(Fixture "${CMAKE_SOURCE_DIR}/catalogs" Catalogs)\n'
                       "QORE_FINALIZE_CATALOG_INSTALL()\n")
        root = prefix / "share/qore/i18n"
        manifest = root / ".qore-catalog-manifests/UnchangedInstall.manifest"

        self.install(build, component="Catalogs")
        self.assertIn("provider.first/root/Fixture.json", manifest.read_text())
        before = self.age(manifest)
        self.install(build, component="Catalogs")
        self.assertEqual(before, self.identity(manifest), "an unchanged catalog manifest is not rewritten")
        self.assertEqual([], [p.name for p in manifest.parent.iterdir() if p.name.endswith(".tmp")])

        # a new generation is published through a new inode and reconciled against
        (source / "catalogs/provider.second").mkdir()
        (source / "catalogs/provider.second/root.json").write_text(catalog)
        self.run_cmake("-S", source, "-B", build)
        self.install(build, component="Catalogs")
        self.assertNotEqual(before[0], manifest.stat().st_ino)
        self.assertIn("provider.second/root/Fixture.json", manifest.read_text())
        self.assertTrue((root / "provider.second/root/Fixture.json").is_file())


if __name__ == "__main__":
    unittest.main()
