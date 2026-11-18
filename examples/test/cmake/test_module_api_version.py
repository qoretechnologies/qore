#!/usr/bin/env python3
"""Tests that every place that names the binary module API names the one of include/qore/ModuleManager.h.

The module API (QORE_MODULE_API_MAJOR.QORE_MODULE_API_MINOR, design/module-api-version.md) is derived from the header
by CMake, but the packaging cannot read it when its metadata is parsed, so these places repeat it and are checked here:

- qore.spec-multi: "%global module_api", which gives the runtime library "Provides: qore-module(abi) = <api>"
  (rpm/qore-requires.py makes each binary module RPM require the API of the qore that built it);
- rpm/macros.qore: "%qore_module_api", the file name suffix of the modules that module RPMs package;
- debian/control and debian/rules: libqore20 provides qore-module-abi-<api> with the value that debian/module-api.py
  reads from the header, which is tested here with valid and invalid headers.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import importlib.util
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
HEADER = ROOT / "include/qore/ModuleManager.h"

spec = importlib.util.spec_from_file_location("qore_module_api", ROOT / "debian/module-api.py")
module_api = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module_api)


def header_api():
    return module_api.module_api(HEADER.read_text(encoding="utf-8"))


class ModuleApiVersionTest(unittest.TestCase):
    def test_header_api(self):
        api = header_api()
        self.assertRegex(api, r"^[0-9]+\.[0-9]+$")
        # the library loads only this API: the loader's list is built from the header macros alone
        source = (ROOT / "lib/ModuleManager.cpp").read_text(encoding="utf-8")
        self.assertTrue(re.search(r"qore_mod_api_list_l\[\] = \{\s*\{QORE_MODULE_API_MAJOR, QORE_MODULE_API_MINOR\},?\s*\};",
                                  source), "the loader's API list must hold only the API of the header")

    def test_rpm_spec(self):
        text = (ROOT / "qore.spec-multi").read_text(encoding="utf-8")
        values = re.findall(r"^%global module_api[ \t]+(\S+)[ \t]*$", text, re.MULTILINE)
        self.assertEqual(values, [header_api()])
        self.assertRegex(text, r"(?m)^Provides: qore-module\(abi\)%\{\?_isa\} = %\{module_api\}$")
        # the distribution specs are links to the multi-distribution spec
        for name in ("qore.spec-fedora", "qore.spec-opensuse"):
            with self.subTest(spec=name):
                self.assertEqual((ROOT / name).resolve(), (ROOT / "qore.spec-multi").resolve())

    def test_rpm_macros(self):
        text = (ROOT / "rpm/macros.qore").read_text(encoding="utf-8")
        self.assertEqual(re.findall(r"^%qore_module_api[ \t]+(\S+)[ \t]*$", text, re.MULTILINE), [header_api()])

    def test_debian_provides(self):
        control = (ROOT / "debian/control").read_text(encoding="utf-8")
        stanza = re.search(r"(?ms)^Package: libqore20\n(.*?)(?:\n\n|\Z)", control)
        self.assertIsNotNone(stanza)
        self.assertRegex(stanza.group(1), r"(?m)^Provides: \$\{qore:Module-ABI\}$")
        rules = (ROOT / "debian/rules").read_text(encoding="utf-8")
        self.assertIn("python3 debian/module-api.py include/qore/ModuleManager.h", rules)
        self.assertIn('dh_gencontrol -plibqore20 -- "-Vqore:Module-ABI=qore-module-abi-$$api"', rules)
        # the helper as debian/rules runs it
        out = subprocess.run([sys.executable, "-I", str(ROOT / "debian/module-api.py"), str(HEADER)],
                             check=True, capture_output=True, text=True)
        self.assertEqual(out.stdout, header_api() + "\n")
        self.assertEqual(out.stderr, "")

    @unittest.skipUnless(shutil.which("dpkg-gencontrol"), "requires dpkg-gencontrol")
    def test_debian_provides_generated(self):
        # the libqore20 control record that dpkg-gencontrol makes from debian/control with the value of debian/rules
        api = header_api()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "debian").mkdir()
            for name in ("control", "changelog"):
                shutil.copy(ROOT / "debian" / name, root / "debian" / name)
            result = subprocess.run(["dpkg-gencontrol", "-plibqore20", "-O", f"-Vqore:Module-ABI=qore-module-abi-{api}",
                                     "-Vshlibs:Depends=", "-Vmisc:Depends="],
                                    cwd=root, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stderr, "")
            fields = dict(line.split(": ", 1) for line in result.stdout.splitlines() if ": " in line)
            self.assertEqual(fields.get("Package"), "libqore20")
            self.assertEqual(fields.get("Provides"), f"qore-module-abi-{api}")

    def test_helper_parses_header_text(self):
        self.assertEqual(module_api.module_api("#define QORE_MODULE_API_MAJOR 3\n#define QORE_MODULE_API_MINOR 12\n"),
                         "3.12")
        self.assertEqual(module_api.module_api("#define QORE_MODULE_API_MINOR\t07 \n#define QORE_MODULE_API_MAJOR 2\n"),
                         "2.7")

    def test_helper_rejects_invalid_headers(self):
        for text in ("", "#define QORE_MODULE_API_MAJOR 2\n",
                     "#define QORE_MODULE_API_MAJOR 2\n#define QORE_MODULE_API_MINOR x\n",
                     "#define QORE_MODULE_API_MAJOR 2\n#define QORE_MODULE_API_MINOR 1\n#define QORE_MODULE_API_MINOR 2\n",
                     "#define QORE_MODULE_API_MAJOR 2 // comment\n#define QORE_MODULE_API_MINOR 1\n"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                module_api.module_api(text)

    def test_helper_usage(self):
        out = subprocess.run([sys.executable, "-I", str(ROOT / "debian/module-api.py")], capture_output=True, text=True)
        self.assertEqual(out.returncode, 2)
        self.assertIn("usage:", out.stderr)


if __name__ == "__main__":
    unittest.main()
