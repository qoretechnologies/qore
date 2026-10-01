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

    def configure(self, root, *, language_tag=True, image_dirs=()):
        source = root / "module source"
        source.mkdir(exist_ok=True)
        for name in image_dirs:
            (source / name).mkdir(exist_ok=True)
        language = self.make_tag(root, "language") if language_tag else root / "absent.tag"
        native = self.make_tag(root, "native")
        input_file = source / "module.dox"
        input_file.write_text("/** @page module Module\n"
                              + ("See @ref language and " if language_tag else "See ")
                              + "@ref native.\n*/\n")
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
set(TAGFILES "\\\"{native}=https://example.invalid/native\\\"")
set(_dox_input "{input_file}")
qore_configure_module_doxygen("{template}" "{root}/Doxyfile")
file(WRITE "{root}/tagfiles-after" "${{TAGFILES}}")
''')
        self.run_command([CMAKE, "-P", str(script)], root)
        self.assertEqual(f'"{native}=https://example.invalid/native"',
                         (root / "tagfiles-after").read_text())
        self.run_command(["doxygen", str(root / "Doxyfile")], root)
        html = (root / "output/html/module.html").read_text()
        self.assertIn("https://example.invalid/native/native.html", html)
        return html, (root / "Doxyfile").read_text(), source

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


if __name__ == "__main__":
    unittest.main()
