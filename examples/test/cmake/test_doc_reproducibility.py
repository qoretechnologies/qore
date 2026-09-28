#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Generate real library docs in different roots, including external graph links."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")
DOXYGEN = os.environ.get("DOXYGEN_EXECUTABLE", "doxygen")


class DocReproducibilityTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which(DOXYGEN) or not shutil.which("dot"):
            raise unittest.SkipTest("Documentation qualification requires Doxygen and Graphviz (absent in nodoc builds)")

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qore-doc-repro-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def run_tool(self, *args, cwd):
        result = subprocess.run(list(map(str, args)), cwd=cwd, text=True, capture_output=True,
                                timeout=120, env={**os.environ, "SOURCE_DATE_EPOCH": "1700000000"})
        warnings = "\n".join(p.read_text() for p in Path(cwd).glob("warnings-*.log"))
        self.assertEqual(0, result.returncode, result.stdout + result.stderr + warnings)

    def generate(self, name):
        source = self.root / name / "source with spaces"
        build = self.root / name / "build with spaces"
        (source / "include").mkdir(parents=True)
        (source / "provider").mkdir()
        build.mkdir()
        (source / "provider/External.h").write_text("/** External base class. */\nclass ExternalBase {};\n")
        (source / "include/Base.h").write_text("/** @file */\n/** Local base class. */\nclass Base {};\n")
        (source / "include/Derived.h").write_text(
            '/** @file */\n#include "Base.h"\n/** Derived class. */\nclass Derived : public Base, public ExternalBase {};\n')
        script = build / "configure.cmake"
        script.write_text(f'''set(CMAKE_SOURCE_DIR "{source}")
set(CMAKE_BINARY_DIR "{build}")
configure_file("{ROOT}/doxygen/lib/Doxyfile.in" "{build}/Doxyfile.base" @ONLY)
''')
        self.run_tool(CMAKE, "-P", script, cwd=build)
        template = (build / "Doxyfile.base").read_text()
        overrides = '''
HTML_HEADER =
HTML_FOOTER =
HTML_EXTRA_FILES =
HTML_STYLESHEET =
HTML_EXTRA_STYLESHEET =
PROJECT_LOGO =
EXAMPLE_PATH =
IMAGE_PATH =
GENERATE_LATEX = NO
QUIET = YES
DOT_NUM_THREADS = 1
'''
        for kind, inputs, tags, tag_output in (
                ("lang", source / "provider", "", "qore.tag"),
                ("library", source / "include", "qore.tag=../../lang/html", "qore-lib.tag")):
            (build / "docs" / kind).mkdir(parents=True)
            config = build / f"Doxyfile.{kind}"
            config.write_text(template + overrides + f'''
INPUT = "{inputs}"
OUTPUT_DIRECTORY = "{build}/docs/{kind}"
TAGFILES = "{tags}"
GENERATE_TAGFILE = {tag_output}
WARN_LOGFILE = "{build}/warnings-{kind}.log"
''')
            self.run_tool(DOXYGEN, config, cwd=build)
        html = build / "docs/library/html"
        graphs = list(html.glob("*.svg"))
        self.assertTrue(any("inherit" in p.name for p in graphs), "inheritance graphs must be generated")
        self.assertTrue(any("incl" in p.name for p in graphs), "include graphs must be generated")
        links = "\n".join(p.read_text() for p in graphs)
        self.assertIn("../../lang/html/class_external_base.html", links)
        self.assertTrue((build / "docs/lang/html/class_external_base.html").is_file())
        # Exercise the actual packaging cleanup, preserving rendered graph links
        # and unrelated source-map/checksum assets, not filtering the comparison.
        self.assertTrue(list(html.glob("*.md5")))
        (html / "application.js.map").write_text('{"version":3}')
        (html / "download.md5").write_text("0" * 32)
        self.run_tool(sys.executable, ROOT / "debian/clean-doxygen-cache.py", html, cwd=build)
        self.assertEqual(["application.js.map"], sorted(p.name for p in html.glob("*.map")))
        self.assertEqual(["download.md5"], sorted(p.name for p in html.glob("*.md5")))
        self.assertEqual('{"version":3}', (html / "application.js.map").read_text())
        payload = {str(p.relative_to(html)): p.read_bytes() for p in html.rglob("*") if p.is_file()}
        self.assertGreater(len(payload), 20)
        for path, content in payload.items():
            self.assertNotIn(str(source).encode(), content, path)
            self.assertNotIn(str(build).encode(), content, path)
        return payload

    def test_library_html_and_graphs_are_identical_across_roots(self):
        first = self.generate("first checkout")
        second = self.generate("second longer checkout")
        changed = sorted(k for k in first.keys() | second.keys() if first.get(k) != second.get(k))
        self.assertEqual([], changed)


if __name__ == "__main__":
    unittest.main()
