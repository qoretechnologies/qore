#!/usr/bin/env python3
"""Validate installed documentation inputs with real CMake and Doxygen.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")


@unittest.skipUnless(shutil.which("doxygen"), "Doxygen is required")
class ModuleDocSdkTest(unittest.TestCase):
    def run_command(self, command, cwd):
        result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, timeout=60)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertNotIn("warning:", result.stderr.lower())
        self.assertNotIn("error:", result.stderr.lower())
        return result

    def make_tag(self, root, name):
        source = root / (name + ".dox")
        source.write_text(f"/** @page {name} {name} reference */\n")
        config = root / (name + ".Doxyfile")
        tag = root / (name + ".tag")
        config.write_text(f'''QUIET = YES
INPUT = "{source}"
GENERATE_HTML = NO
GENERATE_LATEX = NO
GENERATE_TAGFILE = "{tag}"
''')
        self.run_command(["doxygen", str(config)], root)
        return tag

    def configure_installed_assets(self, root, *, missing_header=False, relative_template=False):
        source = root / "module source"
        (source / "docs").mkdir(parents=True)
        sdk = root / "sdk prefix/custom data/qore"
        sdk.mkdir(parents=True)
        for name in ("Doxyfile.in", "header_template.html", "dox_qore.css", "Qore-Q.ico",
                     "qore-logo-55x151-white.png"):
            if name != "header_template.html" or not missing_header:
                shutil.copyfile(ROOT / "doxygen" / name, sdk / name)
        shutil.copyfile(ROOT / "doxygen/footer_template.html", source / "docs/footer_template.html")
        (source / "fixture.dox").write_text("/** @mainpage SDK asset fixture */\n")
        template = sdk / "Doxyfile.in"
        if relative_template:
            template = Path(os.path.relpath(template, source))
        (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(InstalledDocAssets NONE)
include("{ROOT}/cmake/QoreMacros.cmake")
set(module_name fixture)
set(CURRENT_MODULE_NAME fixture)
set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)
set(_dox_input "\\\"${{CMAKE_SOURCE_DIR}}/fixture.dox\\\"")
set(_dox_output "${{CMAKE_BINARY_DIR}}/docs")
qore_configure_module_doxygen("{template}" "${{CMAKE_BINARY_DIR}}/Doxyfile")
file(APPEND "${{CMAKE_BINARY_DIR}}/Doxyfile" "\\nWARN_AS_ERROR = YES\\n")
''')
        build = root / "build"
        for prefix in (root / "consumer prefix", root / "another prefix"):
            self.run_command([CMAKE, "-S", str(source), "-B", str(build),
                              f"-DCMAKE_INSTALL_PREFIX={prefix}"], root)
            config = (build / "Doxyfile").read_text()
            self.assertNotIn(str(prefix), config)
            self.assertIn(f'HTML_HEADER            = "{sdk}/header_template.html"', config)
        return build, sdk

    def test_installed_assets_ignore_consumer_prefix_and_support_spaces(self):
        for relative in (False, True):
            with self.subTest(relative=relative), tempfile.TemporaryDirectory(
                    prefix="qore doc assets ") as directory:
                build, sdk = self.configure_installed_assets(Path(directory), relative_template=relative)
                self.run_command(["doxygen", "Doxyfile"], build)
                html = build / "docs/html"
                self.assertIn("SDK asset fixture", (html / "index.html").read_text())
                for name in ("dox_qore.css", "Qore-Q.ico", "qore-logo-55x151-white.png"):
                    self.assertEqual((sdk / name).read_bytes(), (html / name).read_bytes())

    def test_missing_sdk_header_fails_instead_of_using_consumer_assets(self):
        with tempfile.TemporaryDirectory(prefix="qore missing doc asset ") as directory:
            build, sdk = self.configure_installed_assets(Path(directory), missing_header=True)
            result = subprocess.run(["doxygen", "Doxyfile"], cwd=build,
                                    capture_output=True, text=True, timeout=60)
            self.assertNotEqual(0, result.returncode)
            self.assertIn(str(sdk / "header_template.html"), result.stderr)
            self.assertIn("does not exist", result.stderr)

    def configure(self, root, *, language_tag=True, image_dirs=(), two_phase=False, module_indexes=False):
        source = root / "module source"
        source.mkdir(exist_ok=True)
        for name in image_dirs:
            (source / name).mkdir(exist_ok=True)
        language = self.make_tag(root, "language") if language_tag else root / "absent.tag"
        native = self.make_tag(root, "native")
        if two_phase:
            self.make_tag(root, "child")
        if module_indexes:
            self.make_tag(root, "SdkDependency")
        input_file = source / "module.dox"
        input_file.write_text("/** @page module Module\n"
                              + ("See @ref language and " if language_tag else "See ")
                              + "@ref native.\n"
                              + ("See @ref child.\n" if two_phase else "")
                              + ("See @ref SdkDependency.\n" if module_indexes else "") + "*/\n")
        template = root / "Doxyfile.in"
        template.write_text('''QUIET = YES
INPUT = "@_dox_input@"
OUTPUT_DIRECTORY = "@CMAKE_BINARY_DIR@/output"
GENERATE_LATEX = NO
WARN_AS_ERROR = YES
IMAGE_PATH = @QORE_MODULE_DOXYGEN_IMAGE_PATH@
TAGFILES = @TAGFILES@ @QORE_CORE_DOC_TAGFILES@
''')
        script = root / "configure.cmake"
        script.write_text(f'''set(CMAKE_SOURCE_DIR "{source}")
set(CMAKE_BINARY_DIR "{root}")
include("{ROOT}/cmake/QoreMacros.cmake")
set(QORE_DOXYGEN_TAGFILE "{language}")
set(QORE_DOXYGEN_TAG_URL "https://example.invalid/language")
set(QORE_DOXYGEN_MODULE_TAG_DIR "{root}")
set(QORE_DOXYGEN_MODULE_URL "https://example.invalid/modules")
set(QORE_DOXYGEN_MODULES "{'SdkDependency;Unavailable' if module_indexes else ''}")
set(TAGFILES "\\\"{native}=https://example.invalid/native\\\"")
set(_dox_input "{input_file}")
qore_configure_module_doxygen("{template}" "{root}/Doxyfile")
file(WRITE "{root}/tagfiles-after" "${{TAGFILES}}")
''')
        config = root / "Doxyfile"
        if two_phase:
            (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(TwoPhaseDocSdk NONE)
include("{script}")
set(DOXYGEN_FOUND TRUE)
set(DOXYGEN_EXECUTABLE doxygen)
add_custom_target(docs)
add_custom_target(docs-child)
qore_binary_module_two_phase_docs(module "child")
''')
            self.run_command([CMAKE, "-S", str(source), "-B", str(root)], root)
            config = root / "Doxyfile.final"
        else:
            self.run_command([CMAKE, "-P", str(script)], root)
        self.assertEqual(f'"{native}=https://example.invalid/native"',
                         (root / "tagfiles-after").read_text())
        self.run_command(["doxygen", str(config)], root)
        html = (root / "output/html/module.html").read_text()
        self.assertIn("https://example.invalid/native/native.html", html)
        return html, config.read_text(), source

    def test_two_phase_docs_retain_language_and_caller_indexes(self):
        with tempfile.TemporaryDirectory(prefix="qore doc two phase ") as directory:
            html, _, _ = self.configure(Path(directory), two_phase=True)
            self.assertIn("https://example.invalid/language/language.html", html)
            self.assertIn("../../child/html/child.html", html)

    def test_selected_installed_module_indexes_resolve_links(self):
        with tempfile.TemporaryDirectory(prefix="qore installed module indexes ") as directory:
            html, config, _ = self.configure(Path(directory), module_indexes=True, two_phase=True)
            self.assertIn("https://example.invalid/modules/SdkDependency/html/SdkDependency.html", html)
            self.assertNotIn("Unavailable.tag", config)

    def test_module_index_installation_is_optional_and_preserves_content(self):
        with tempfile.TemporaryDirectory(prefix="qore install module indexes ") as directory:
            root = Path(directory)
            tag = self.make_tag(root, "available")
            prefix = root / "sdk"
            (root / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(InstallDocIndex NONE)
include("{ROOT}/cmake/QoreMacros.cmake")
set(CMAKE_INSTALL_FULL_DATADIR "{prefix}/share")
set(QORE_INSTALL_COMPONENT_BOOTSTRAP sdk)
qore_install_module_doxygen_tag(Module "{tag}")
qore_install_module_doxygen_tag(Unavailable "{root}/absent.tag")
''')
            self.run_command([CMAKE, "-S", str(root), "-B", str(root / "build")], root)
            self.run_command([CMAKE, "--install", str(root / "build"), "--component", "sdk"], root)
            self.assertEqual(tag.read_bytes(), (prefix / "share/qore/module-tags/Module.tag").read_bytes())
            self.assertFalse((prefix / "share/qore/module-tags/Unavailable.tag").exists())

    def test_invalid_installed_module_index_name_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            script = root / "invalid.cmake"
            script.write_text(f'''include("{ROOT}/cmake/QoreMacros.cmake")
set(QORE_DOXYGEN_MODULES "../outside")
qore_configure_module_doxygen("absent.in" "output")
''')
            result = subprocess.run([CMAKE, "-P", str(script)], cwd=root, capture_output=True, text=True,
                                    timeout=60)
            self.assertNotEqual(0, result.returncode)
            self.assertIn("Invalid QORE_DOXYGEN_MODULES name", result.stderr)

    def test_external_native_module_installs_index_only_when_built(self):
        with tempfile.TemporaryDirectory(prefix="qore external index ") as directory:
            root = Path(directory)
            source = root / "source"
            build = root / "build"
            prefix = root / "sdk"
            (source / "cmake").mkdir(parents=True)
            (source / "cmake/cmake_uninstall.cmake.in").write_text("# fixture\n")
            (source / "fixture.c").write_text("void fixture(void) {}\n")
            (source / "Doxyfile.in").write_text("PROJECT_NAME = Fixture\n")
            (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(ExternalNativeIndex C)
include("{ROOT}/cmake/QoreMacros.cmake")
set(CMAKE_INSTALL_FULL_DATADIR "{prefix}/custom data")
set(QORE_INSTALL_COMPONENT_BOOTSTRAP doc-index)
set(QORE_USERMODULE_DOXYGEN_TEMPLATE "${{CMAKE_SOURCE_DIR}}/Doxyfile.in")
set(QORE_MODULES_DIR lib/qore-modules)
set(QORE_API_VERSION 2.0)
set(QORE_GENERATE_JAVA_BINDINGS OFF)
add_library(fixture MODULE fixture.c)
qore_binary_module_intern2(fixture 1.0 "" "2")
''')
            self.run_command([CMAKE, "-S", str(source), "-B", str(build)], root)
            install = [CMAKE, "--install", str(build), "--component", "doc-index"]
            self.run_command(install, root)
            installed = prefix / "custom data/qore/module-tags/fixture.tag"
            self.assertFalse(installed.exists())
            tag = self.make_tag(build, "fixture")
            self.run_command(install, root)
            self.assertEqual(tag.read_bytes(), installed.read_bytes())

    def test_language_and_module_references_with_space_paths(self):
        with tempfile.TemporaryDirectory(prefix="qore doc sdk ") as directory:
            html, config, source = self.configure(Path(directory), image_dirs=("docs", "doxygen"))
            self.assertIn("https://example.invalid/language/language.html", html)
            for name in ("docs", "doxygen"):
                self.assertIn(f'"{source / name}"', config)

    def test_sdk_without_language_index_or_image_directories(self):
        with tempfile.TemporaryDirectory(prefix="qore doc minimal ") as directory:
            html, config, _ = self.configure(Path(directory), language_tag=False)
            self.assertNotIn("example.invalid/language", html)
            self.assertNotIn("absent.tag", config)
            self.assertIn("IMAGE_PATH = \n", config)

    def test_external_user_modules_only_reference_registered_indexes(self):
        for separated, native in ((False, False), (True, False), (True, True)):
            with self.subTest(separated=separated, native=native), tempfile.TemporaryDirectory(
                    prefix="qore user doc sdk ") as directory:
                root = Path(directory)
                source = root / "source"
                build = root / "build"
                build.mkdir()
                module = source / ("qlib/Fixture/Fixture.qm" if separated else "qlib/Fixture.qm")
                module.parent.mkdir(parents=True)
                module.write_text("%modern\nmodule Fixture { version = \"1.0\"; }\n")
                self.make_tag(build, "dependency")
                if native:
                    self.make_tag(build, "native")
                template = source / "Doxyfile.in"
                template.write_text('''QUIET = YES
INPUT = "@_dox_input@"
OUTPUT_DIRECTORY = "@CMAKE_BINARY_DIR@/output"
GENERATE_LATEX = NO
WARN_AS_ERROR = YES
TAGFILES = @TAGFILES@ @QORE_CORE_DOC_TAGFILES@
''')
                (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.14...3.31)
project(UserModuleDocSdk NONE)
include("{ROOT}/cmake/QoreMacros.cmake")
set(QORE_BUILD_AOT_MODULES OFF)
set(DOXYGEN_FOUND TRUE)
set(QORE_USERMODULE_DOXYGEN_TEMPLATE "{template}")
set(QORE_QDX_COMMAND "${{CMAKE_COMMAND}}" -E true)
set(DOXYGEN_EXECUTABLE doxygen)
set(QORE_USER_MODULES_DIR share/qore-modules)
set(QORE_DOXYGEN_TAGFILE "{root}/absent.tag")
{'set(_external_module_name native)' if native else ''}
add_custom_target(docs)
add_custom_target(docs-module)
add_custom_target(docs-dependency)
qore_external_user_module("{'qlib/Fixture' if separated else 'qlib/Fixture.qm'}" "dependency")
''')
                self.run_command([CMAKE, "-S", str(source), "-B", str(build)], root)
                header = build / "doxygen/qlib/Fixture/Fixture.qm.dox.h"
                header.write_text("/** @page fixture Fixture\nSee @ref dependency.\n"
                                  + ("See @ref native.\n" if native else "") + "*/\n")
                config = build / "doxygen/Doxyfile.Fixture"
                self.run_command(["doxygen", str(config)], build)
                html = (build / "output/html/fixture.html").read_text()
                self.assertIn("../../dependency/html/dependency.html", html)
                if native:
                    self.assertIn("../../native/html/native.html", html)
                else:
                    self.assertNotIn(str(build / ".tag"), config.read_text())


if __name__ == "__main__":
    unittest.main()
