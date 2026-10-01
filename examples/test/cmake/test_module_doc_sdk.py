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
