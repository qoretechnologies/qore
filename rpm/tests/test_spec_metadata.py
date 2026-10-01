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

    def test_disabling_docs_and_checks_does_not_disable_onnx(self):
        output = subprocess.check_output([
            "rpmspec", "--without", "docs", "--without", "tests", "-P", str(SPEC)], text=True)
        self.assertIn("-DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON", output)
        self.assertNotIn("%files doc", output)
        self.assertNotIn("./run_tests.sh", output)
        self.assertIn("-DQORE_WITH_ONNXRUNTIME=ON", output)
        self.assertIn("-DQORE_REQUIRE_ONNXRUNTIME=ON", output)

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
