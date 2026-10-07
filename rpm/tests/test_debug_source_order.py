# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Exercise the core RPM post-install hook with real debug-source extraction."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


REPOSITORY = Path(__file__).resolve().parents[2]


class DebugSourceOrderTest(unittest.TestCase):
    def test_automatic_and_supplemental_sources_share_debug_package(self):
        with tempfile.TemporaryDirectory(prefix="qore-debug-source-order-") as name:
            root = Path(name)
            for directory in ("BUILD", "BUILDROOT", "RPMS", "SOURCES", "SPECS", "SRPMS"):
                (root / directory).mkdir()
            sources = root / "SOURCES"
            (sources / "Referenced.qm").write_text("%modern\nint answer = 42;\n")
            (sources / "Unreferenced.qm").write_text("%modern\nconst Answer = 42;\n")
            (sources / "probe.c").write_text(
                '#line 2 "qlib/Referenced.qm"\nint main(void) { return 0; }\n')
            core = (REPOSITORY / "qore.spec-multi").read_text()
            begin = core.index("%global qore_rpm_helper ")
            end = core.index("\n%if 0%{?suse_version}", begin)
            hook = core[begin:end]
            for helper in ("preserve-aot-metadata.py", "install-aot-sources.py"):
                hook = hook.replace("rpm/" + helper, str(REPOSITORY / "rpm" / helper))
            spec = root / "SPECS/source-order.spec"
            spec.write_text("""Name: qore-source-order-test
Version: 1
Release: 1
Summary: Debug-source installation order regression
License: MIT
%description
Verify automatic and supplemental Qore source installation.
""" + hook + """
%prep
%setup -q -c -T
mkdir qlib
cp %{_sourcedir}/Referenced.qm %{_sourcedir}/Unreferenced.qm qlib/
cp %{_sourcedir}/probe.c .
%build
cc -g -O0 -o probe probe.c
%install
install -D -m755 probe %{buildroot}%{_bindir}/qore-source-order-test
%check
cmp qlib/Referenced.qm %{buildroot}%{qore_debug_source_dir}/qlib/Referenced.qm
cmp qlib/Unreferenced.qm %{buildroot}%{qore_debug_source_dir}/qlib/Unreferenced.qm
%files
%{_bindir}/qore-source-order-test
%changelog
* Wed Oct 07 2026 Qore Technologies <info@qoretechnologies.com> - 1-1
- Exercise debug-source ordering.
""")
            env = os.environ.copy()
            for variable in ("LD_PRELOAD", "LD_LIBRARY_PATH"):
                env.pop(variable, None)
            result = subprocess.run([
                "rpmbuild", "-ba", "--define", "_topdir " + str(root),
                "--define", "_smp_build_ncpus 1",
                "--define", "_buildhost localhost", str(spec),
            ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=env)
            self.assertEqual(0, result.returncode, result.stdout)
            self.assertNotRegex(result.stdout, r"(?im)(warning:|error:|cpio:)")
            debug_packages = list((root / "RPMS").rglob("*-debugsource-*.rpm"))
            self.assertEqual(1, len(debug_packages), result.stdout)
            listing = subprocess.check_output(
                ["rpm", "-qlp", str(debug_packages[0])], text=True, env=env)
            for filename in ("Referenced.qm", "Unreferenced.qm"):
                self.assertRegex(listing, r"(?m)^/usr/src/debug/.+/qlib/" + re.escape(filename) + r"$")


if __name__ == "__main__":
    unittest.main()
