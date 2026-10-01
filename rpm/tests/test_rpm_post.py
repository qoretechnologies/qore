# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Exercise the real distribution RPM post-processing pipeline on an ELF qmod."""
import importlib.util
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
loader = importlib.util.spec_from_file_location("aot", ROOT / "preserve-aot-metadata.py")
aot = importlib.util.module_from_spec(loader)
loader.loader.exec_module(aot)


class RpmPostTest(unittest.TestCase):
    def test_reproducible_rpm_retains_trailers_debug_info_and_sources(self):
        artifacts = []
        for prefix in ("qore-rpm-first-", "qore-rpm-second-longer-"):
            with tempfile.TemporaryDirectory(prefix=prefix) as temporary:
                artifacts.append(self.build_fixture(Path(temporary)))
        self.assertEqual(*artifacts, "Installed ELF differs between build roots")

    def build_fixture(self, temporary):
        top = Path(temporary)
        for directory in ["BUILD", "BUILDROOT", "RPMS", "SOURCES", "SPECS", "SRPMS"]:
            (top / directory).mkdir()
        # Exercise the real .attr path selection independently of an installed
        # SDK. The generator's SDK/EVR parsing is covered by test_requires.py.
        attributes = top / "attributes"
        attributes.mkdir()
        system_attributes = Path(subprocess.check_output(
            ["rpm", "--eval", "%{_fileattrsdir}"], text=True).strip())
        for attribute in system_attributes.glob("*.attr"):
            if attribute.name != "qore.attr":
                (attributes / attribute.name).symlink_to(attribute)
        generator = top / "attribute-requires.py"
        generator.write_text('#!/usr/bin/python3\nimport sys\nfrom pathlib import Path\n'
                             'for path in sys.stdin:\n'
                             '    print("qore-attribute-" + Path(path.strip()).name + " = 1")\n')
        generator.chmod(0o755)
        attribute = (ROOT / "qore.attr").read_text().splitlines()
        attribute[0] = "%__qore_requires " + str(generator)
        (attributes / "qore.attr").write_text("\n".join(attribute) + "\n")
        (top / "SOURCES/probe.c").write_text("int package_probe(void) { return 42; }\n"
                                               "const char* source_path(void) { return __FILE__; }\n")
        trailers = b"".join(payload + struct.pack("<Q4sI", len(payload), magic, 1)
                            for magic, payload in [(b"QAOM", b"optional"), (b"QPCM", b"locations"),
                                                   (b"QAMD", b"dependencies")])
        (top / "SOURCES/trailers").write_bytes(trailers)
        recipe = top / "SPECS/qore-rpm-fixture.spec"
        recipe.write_text(f"""%{{load:{ROOT}/macros.qore}}
%{{load:{attributes}/qore.attr}}
%global use_source_date_epoch_as_buildtime 1
%if v"%{{rpmversion}}" >= v"4.20"
%global build_mtime_policy clamp_to_source_date_epoch
%else
%global clamp_mtime_to_source_date_epoch 1
%endif
Name: qore-rpm-fixture
Version: 1.0
Release: 1%{{?dist}}
Summary: RPM post-processing test fixture
License: MIT
Source0: probe.c
Source1: trailers
%global qore_rpm_helper {ROOT}/preserve-aot-metadata.py
%qore_enable_aot_post
%description
Test fixture for real ELF, debug-info and metadata processing.
%prep
%setup -q -c -T
cp %{{SOURCE0}} probe.c
%build
. {ROOT}/build-env.sh
qore_set_source_prefix_maps "%qore_debug_source_dir"
gcc $CFLAGS -g -O2 -fPIC -shared -Wl,--build-id -o Probe.qmod "$PWD/probe.c"
cat %{{SOURCE1}} >> Probe.qmod
%install
install -Dm755 Probe.qmod %{{buildroot}}%{{_libdir}}/qore-modules/Probe.qmod
printf 'not a module\n' > %{{buildroot}}%{{_libdir}}/qore-modules/Probe.qmod.backup
%files
%{{_libdir}}/qore-modules/Probe.qmod
%{{_libdir}}/qore-modules/Probe.qmod.backup
%changelog
* Thu Oct 01 2026 David Nichols <david@qore.org> - 1.0-1
- Test real RPM post-processing.
""")
        result = subprocess.run(["rpmbuild", "-bb", "--define", f"_topdir {top}",
                                 "--define", f"_fileattrsdir {attributes}",
                                 "--define", "_smp_build_ncpus 2", "--define", "_buildhost qore-rpm-builder", str(recipe)],
                                env=dict(os.environ, SOURCE_DATE_EPOCH="1600000000"),
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertNotRegex(result.stdout.lower(), r"\bwarning:", result.stdout)
        packages = list((top / "RPMS").rglob("*.rpm"))
        runtime = [p for p in packages if p.name.startswith("qore-rpm-fixture-1.")]
        debug = [p for p in packages if "-debuginfo-" in p.name]
        self.assertEqual(len(runtime), 1, result.stdout)
        self.assertEqual(len(debug), 1, result.stdout)
        dependencies = subprocess.check_output(
            ["rpm", "-qp", "--requires", str(runtime[0])], text=True).splitlines()
        self.assertEqual(["qore-attribute-Probe.qmod = 1"],
                         [value for value in dependencies if value.startswith("qore-attribute-")], result.stdout)
        extract = top / "extracted"
        extract.mkdir()
        for package in packages:
            timestamps = subprocess.check_output(
                ["rpm", "-qp", "--qf", "%{BUILDTIME}\n[%{FILEMTIMES}\n]", str(package)], text=True)
            values = list(map(int, timestamps.splitlines()))
            self.assertEqual(1600000000, values[0])
            self.assertTrue(all(value <= 1600000000 for value in values[1:]), timestamps)
            payload = subprocess.check_output(["rpm2cpio", str(package)])
            subprocess.run(["cpio", "--extract", "--make-directories", "--quiet"], input=payload,
                           cwd=extract, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        source = next(extract.glob("usr/src/debug/**/probe.c"), None)
        self.assertIsNotNone(source, "Debug-source package lost the mapped source: " + result.stdout)
        self.assertEqual((top / "SOURCES/probe.c").read_bytes(), source.read_bytes())
        module = next(extract.rglob("Probe.qmod"))
        self.assertNotIn(str(top).encode(), module.read_bytes())
        self.assertEqual(aot.read_trailers(module), trailers)
        sections = subprocess.check_output(["readelf", "-S", str(module)], text=True)
        self.assertIn(".gnu_debuglink", sections)
        self.assertNotIn(".debug_info", sections)
        debugfiles = list(extract.rglob("*.debug"))
        self.assertTrue(debugfiles)
        self.assertTrue(any(".debug_info" in subprocess.check_output(
            ["readelf", "-S", str(path)], text=True) for path in debugfiles if path.is_file()))
        return module.read_bytes()


if __name__ == "__main__":
    unittest.main()
