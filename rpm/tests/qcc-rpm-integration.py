#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Qualify real qcc output through RPM stripping using an explicitly selected SDK."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
loader = importlib.util.spec_from_file_location("aot", ROOT / "preserve-aot-metadata.py")
aot = importlib.util.module_from_spec(loader)
loader.loader.exec_module(aot)


class QccRpmTest(unittest.TestCase):
    def test_reproducible_loadable_aot_with_debug_sources(self):
        modules = []
        for prefix in ("qore-qcc-first-", "qore-qcc-second-longer-"):
            with tempfile.TemporaryDirectory(prefix=prefix) as temporary:
                modules.append(self.build_fixture(Path(temporary)))
        self.assertEqual(*modules, "Installed qcc output differs between source roots")

    def build_fixture(self, top):
        for name in ("SOURCES", "SPECS"):
            (top / name).mkdir()
        source = ("%modern\nmodule Probe { version = \"1.0\"; desc = \"RPM integration test\"; "
                  "author = \"Qore\"; url = \"https://qore.org\"; license = \"MIT\"; }\n"
                  "public int sub package_answer() { return 42; }\n")
        (top / "SOURCES/Probe.qm").write_text(source)
        recipe = top / "SPECS/probe.spec"
        recipe.write_text(f'''%{{load:{ROOT}/macros.qore}}
%global qore_rpm_helper {ROOT}/preserve-aot-metadata.py
%global _find_debuginfo_dwz_opts %{{nil}}
%qore_enable_aot_post
Name: qore-qcc-fixture
Version: 1.0
Release: 1%{{?dist}}
Summary: qcc RPM integration test
License: MIT
Source0: Probe.qm
%description
Actual AOT metadata, debug source, reproducibility and loading tests.
%prep
%setup -q -c -T
cp %{{SOURCE0}} Probe.qm
%build
"$QORE_TEST_QCC" -m --file-prefix-map="$PWD"="%{{qore_debug_source_dir}}" -o Probe.qmod "$PWD/Probe.qm"
cp Probe.qmod "$QORE_TEST_UNSTRIPPED"
%install
install -Dm755 Probe.qmod %{{buildroot}}%{{_libdir}}/qore-modules/Probe.qmod
python3 {ROOT}/install-aot-sources.py "%{{buildroot}}" "%{{qore_debug_source_dir}}" Probe.qm
%files
%{{_libdir}}/qore-modules/Probe.qmod
%changelog
* Thu Oct 01 2026 David Nichols <david@qore.org> - 1.0-1
- Test real qcc output in RPMs.
''')
        built = top / "unstripped.qmod"
        result = subprocess.run(["rpmbuild", "-bb", "--define", "_topdir " + str(top),
                                 "--define", "_smp_build_ncpus 1", "--define", "_buildhost qore-rpm-builder", str(recipe)],
                                env=dict(os.environ, QORE_TEST_UNSTRIPPED=str(built)),
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.assertEqual(0, result.returncode, result.stdout)
        self.assertNotRegex(result.stdout.lower(), r"\bwarning:", result.stdout)
        trailers = aot.read_trailers(built)
        self.assertTrue(trailers, "qcc did not produce AOT metadata")
        extracted = top / "extracted"
        extracted.mkdir()
        packages = list((top / "RPMS").rglob("*.rpm"))
        self.assertTrue(any("-debuginfo-" in p.name for p in packages))
        for package in packages:
            payload = subprocess.check_output(["rpm2cpio", str(package)])
            subprocess.run(["cpio", "-idm", "--quiet"], input=payload, cwd=extracted,
                           check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        sources = list(extracted.glob("usr/src/debug/**/Probe.qm"))
        self.assertEqual(1, len(sources))
        self.assertEqual(source, sources[0].read_text())
        module = next(extracted.rglob("Probe.qmod"))
        self.assertEqual(trailers, aot.read_trailers(module))
        sections = subprocess.check_output(["readelf", "-S", str(module)], text=True)
        self.assertIn(".gnu_debuglink", sections)
        self.assertNotIn(".debug_info", sections)
        debug_sections = [subprocess.check_output(["readelf", "-S", str(p)], text=True)
                          for p in extracted.rglob("*.debug") if p.is_file()]
        self.assertTrue(any(".debug_info" in value for value in debug_sections))
        env = dict(os.environ, QORE_MODULE_DIR=str(module.parent))
        result = subprocess.check_output([os.environ["QORE_TEST_QORE"], "-b", "--enable-debug", "-l", "Probe",
                                          "-nX", "package_answer()"], env=env, text=True)
        self.assertEqual("42", result.strip())
        self.assertNotIn(str(top).encode(), module.read_bytes())
        return module.read_bytes()


if __name__ == "__main__":
    # No implicit fallback to a potentially different system SDK.
    for variable in ("QORE_TEST_QCC", "QORE_TEST_QORE"):
        if not os.environ.get(variable):
            raise SystemExit(variable + " must select the SDK under qualification")
    unittest.main()
