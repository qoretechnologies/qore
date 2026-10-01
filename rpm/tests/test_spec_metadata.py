# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Check the runtime contract emitted by the target distribution's RPM parser."""
from pathlib import Path
import os
import shutil
import subprocess
import unittest


SPEC = Path(__file__).resolve().parents[2] / "qore.spec-multi"


@unittest.skipUnless(shutil.which("rpmspec"), "requires the target RPM parser")
class SpecMetadataTest(unittest.TestCase):
    def requirements(self):
        output = subprocess.check_output([
            "rpmspec", "-q", "--qf",
            "PACKAGE %{NAME}\n[%{REQUIRENAME} %{REQUIREFLAGS:depflags} %{REQUIREVERSION}\n]",
            str(SPEC)], text=True)
        packages = {}
        current = None
        for line in output.splitlines():
            if line.startswith("PACKAGE "):
                current = packages.setdefault(line.removeprefix("PACKAGE "), set())
            else:
                self.assertIsNotNone(current)
                current.add(line.strip())
        return packages

    def test_runtime_versions_are_enforced_without_development_packages(self):
        suse = subprocess.check_output(["rpm", "--eval", "%{?suse_version}"], text=True).strip()
        fedora = subprocess.check_output(["rpm", "--eval", "%{?fedora}"], text=True).strip()
        isa = subprocess.check_output(["rpm", "--eval", "%{?_isa}"], text=True).strip()
        self.assertTrue(isa.startswith("("), "native architecture capability is required")
        packages = self.requirements()
        runtime = packages["libqore20" if suse else "libqore"]
        expected = {
            "libnghttp2-14" if suse else "libnghttp2": "1.70.0",
            "libngtcp2-16" if suse else "ngtcp2": "1.25.0",
            "libngtcp2_crypto_ossl0" if suse else "ngtcp2-crypto-ossl": "1.25.0",
            "libnghttp3-9" if suse else "libnghttp3": "1.18.0",
        }
        for name, version in expected.items():
            self.assertIn(f"{name}{isa} >= {version}", runtime)
        self.assertIn(f"c-ares(qore-query-lifecycle-fixes){isa} = 1", runtime)
        converters = "glibc-gconv-modules-extra" if suse else "glibc-gconv-extra"
        self.assertIn(f"{converters}{isa}", runtime)
        parser = "libtree-sitter0_26" if suse else ("libtree-sitter" if fedora else "tree-sitter")
        self.assertIn(f"{parser}{isa} >= 0.26.13", packages["qore-stdlib"])
        for name in ("qore", "libqore20" if suse else "libqore", "qore-stdlib"):
            self.assertFalse(any("-devel" in value or "pkgconfig(" in value
                                 for value in packages[name]), name)
        self.assertIn("python3 >= 3.11", packages["qore-rpm-macros"])

    @unittest.skipUnless(os.environ.get("QORE_RPM_VERIFY_INSTALLED_DEPS") == "1",
                         "requires the target build dependency image")
    def test_parser_requirement_is_provided_by_installed_runtime(self):
        # Query the actual target RPM database, not a second spelling table.
        # Fedora's library package differs from the Enterprise Linux backport.
        requirements = [value for value in self.requirements()["qore-stdlib"]
                        if "tree-sitter" in value]
        self.assertEqual(1, len(requirements))
        capability, operator, minimum = requirements[0].split()
        self.assertEqual(">=", operator)
        result = subprocess.run(["rpm", "-q", "--whatprovides", capability,
                                 "--qf", "%{NAME}\n[%{PROVIDENAME} %{PROVIDEVERSION}\n]"],
                                capture_output=True, text=True)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertNotIn("-devel", result.stdout)
        versions = [line.split()[1] for line in result.stdout.splitlines()
                    if line.startswith(capability + " ")]
        self.assertEqual(1, len(versions), result.stdout)
        for value in (versions[0], minimum):
            self.assertRegex(value, r"^[A-Za-z0-9._+~:^\-]+$")
        # --whatprovides resolves names only; RPM's version expression performs
        # the EVR comparison using the same semantics as its dependency solver.
        expression = '%{expr:v"' + versions[0] + '" >= v"' + minimum + '"}'
        self.assertEqual("1", subprocess.check_output(
            ["rpm", "--eval", expression], text=True).strip())

    def test_fedora_postprocessor_fixes_are_build_dependencies_only(self):
        fedora = subprocess.check_output(["rpm", "--eval", "%{?fedora}"], text=True).strip()
        fixes = {"add-determinism(qore-tempfile-fix) = 1",
                 "linkdupes(qore-bounded-descriptors) = 1"}
        for options in ([], ["--without", "docs", "--without", "tests"]):
            requirements = set(subprocess.check_output(
                ["rpmspec", *options, "-q", "--buildrequires", str(SPEC)],
                text=True).splitlines())
            self.assertEqual(fixes if fedora else set(), fixes & requirements)
        for requirements in self.requirements().values():
            self.assertTrue(fixes.isdisjoint(requirements))
        if fedora and os.environ.get("QORE_RPM_VERIFY_INSTALLED_DEPS") == "1":
            for fix in fixes:
                # RPM's database lookup accepts a capability name, not a
                # version expression. Verify the provider's EVR separately.
                capability = fix.split()[0]
                result = subprocess.run([
                    "rpm", "-q", "--whatprovides", capability, "--qf",
                    "[%{PROVIDENAME} = %{PROVIDEVERSION}\n]"],
                    capture_output=True, text=True)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                self.assertIn(fix, result.stdout.splitlines())

    def test_disabling_docs_and_checks_does_not_disable_onnx(self):
        output = subprocess.check_output([
            "rpmspec", "--without", "docs", "--without", "tests", "-P", str(SPEC)], text=True)
        self.assertIn("-DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON", output)
        self.assertNotIn("%files doc", output)
        self.assertNotIn("./run_tests.sh", output)
        self.assertIn("-DQORE_WITH_ONNXRUNTIME=ON", output)
        self.assertIn("-DQORE_REQUIRE_ONNXRUNTIME=ON", output)

    def test_snapshot_compatibility_does_not_obsolete_itself(self):
        # Source preparation substitutes a snapshot version before rpmbuild.
        import tempfile
        with tempfile.TemporaryDirectory() as directory:
            snapshot = Path(directory) / "qore.spec"
            snapshot.write_text(SPEC.read_text().replace("Version: 3.0.0\n",
                                                        "Version: 3.0.0~git20261001.11\n"))
            output = subprocess.check_output([
                "rpmspec", "-q", "--qf", "[%{OBSOLETENAME} %{OBSOLETEFLAGS:depflags} %{OBSOLETEVERSION}\n]",
                str(snapshot)], text=True)
            for module in ("linenoise", "yaml"):
                self.assertIn(f"qore-{module}-module < 3.0.0~git20261001.11", output.splitlines())

    @unittest.skipUnless(os.environ.get("QORE_RPM_VERIFY_INSTALLED_DEPS") == "1",
                         "requires the target build dependency image")
    def test_mongodb_dependency_accepts_the_installed_driver_api(self):
        import tempfile
        requirements = subprocess.check_output([
            "rpmspec", "-q", "--buildrequires", str(SPEC)], text=True).splitlines()
        dependency, = [value for value in requirements if 'pkgconfig(' in value and 'mongoc' in value]
        # Exercise RPM's dependency solver against real SDKs carrying either
        # the 1.x or 2.x driver. Also prove the negative case cannot pass.
        with tempfile.TemporaryDirectory() as directory:
            spec = Path(directory) / 'probe.spec'
            def prepare(requirement):
                spec.write_text('Name: qore-mongodb-dependency-probe\nVersion: 1\nRelease: 1\n'
                                'Summary: Dependency solver probe\nLicense: MIT\n'
                                f'BuildRequires: {requirement}\n'
                                '%description\nDependency solver probe.\n%prep\n:\n%files\n'
                                '%changelog\n* Thu Oct 01 2026 Qore <info@qore.org> - 1-1\n'
                                '- Exercise native dependency resolution.\n')
                return subprocess.run(['rpmbuild', '-bp', '--define', f'_topdir {directory}',
                                       '--define', '_buildhost qore-rpm-builder', str(spec)],
                                      capture_output=True, text=True)
            result = prepare(dependency)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertNotIn('warning:', result.stderr.lower())
            result = prepare('(pkgconfig(qore-missing-mongoc2) or pkgconfig(qore-missing-mongoc1))')
            self.assertNotEqual(0, result.returncode)
            self.assertIn('Failed build dependencies', result.stderr)

    @unittest.skipUnless(os.environ.get("QORE_RPM_VERIFY_INSTALLED_DEPS") == "1",
                         "requires the target build dependency image")
    def test_documentation_deduplicator_has_a_resolvable_package_requirement(self):
        provider = subprocess.check_output([
            "rpm", "-qf", "/usr/bin/hardlink", "--qf", "%{NAME}"], text=True)
        requirements = subprocess.check_output([
            "rpmspec", "-q", "--buildrequires", str(SPEC)], text=True).splitlines()
        self.assertIn(provider, requirements)
        self.assertNotIn("/usr/bin/hardlink", requirements)
        without_docs = subprocess.check_output([
            "rpmspec", "--without", "docs", "-q", "--buildrequires", str(SPEC)],
            text=True).splitlines()
        self.assertNotIn(provider, without_docs)


if __name__ == "__main__":
    unittest.main()
