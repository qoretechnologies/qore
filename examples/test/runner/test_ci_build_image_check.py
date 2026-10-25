#!/usr/bin/env python3
"""Exercise test/docker_test/ci-build-image-check.sh, which fails a CI test job that runs on another qore-test-base
image than the build job whose build artifact it tests.

The build job's image is described by the [qore-test-base-build-info] section of the build artifact info, and this
job's image by the file that QORE_TEST_BASE_BUILD_INFO names (/etc/qore-test-base-build-info in CI).  In pipeline
61639, the image tag was published again between the build jobs and the test jobs, and the test jobs tested the
build's AOT qlib modules with the binary modules of another image.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
DOCKER_TEST = ROOT / "test/docker_test"
SCRIPT = DOCKER_TEST / "ci-build-image-check.sh"

IMAGE_INFO = """qore_test_base_build_info_version=1
created_utc={created}
flavor=ubuntu
tag=develop
qore_commit={commit}

[modules]
module-xml=0123456789abcdef
"""

ARTIFACT_INFO = """qore_ci_build_artifact_info_version=1
created_utc=2026-10-25T11:52:33Z
ci_job_name=build-ubuntu-amd64
{image}
[built-qore-version]
created_utc=2026-10-25T11:24:18Z
qore_commit=7ecf4fcbabab93f3f70740816e29ae8b342790ba
"""

OLD = {"created": "2026-10-24T21:47:18Z", "commit": "a154e90942420be40b77b8ed1d41974c38b27bd9"}
NEW = {"created": "2026-10-25T11:24:18Z", "commit": "7ecf4fcbabab93f3f70740816e29ae8b342790ba"}

CHANGED = ("qore-test-base image changed during this pipeline (build: {build}, test: {test}); re-run the pipeline\n")


def image(info):
    return "{created} qore {commit}".format(**info)


class CiBuildImageCheckTest(unittest.TestCase):
    def check(self, build_image, this_image, *, artifact=True, section=True):
        with tempfile.TemporaryDirectory(prefix="qore-ci-image-check-") as directory:
            root = Path(directory)
            info = root / "qore-ci-build-artifact-info"
            if artifact:
                text = "\n[qore-test-base-build-info]\n" + build_image if section else ""
                info.write_text(ARTIFACT_INFO.format(image=text))
            this_info = root / "qore-test-base-build-info"
            if this_image is not None:
                this_info.write_text(this_image)
            env = dict(os.environ, QORE_TEST_BASE_BUILD_INFO=str(this_info))
            return subprocess.run(["sh", str(SCRIPT), str(info)], capture_output=True, text=True, env=env,
                                  timeout=60)

    def assertFails(self, result, reason):
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertEqual("", result.stdout)
        self.assertIn(reason, result.stderr)

    def test_same_image(self):
        result = self.check(IMAGE_INFO.format(**OLD), IMAGE_INFO.format(**OLD))
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertEqual("", result.stdout + result.stderr)

    def test_newer_test_image(self):
        result = self.check(IMAGE_INFO.format(**OLD), IMAGE_INFO.format(**NEW))
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertEqual("", result.stdout)
        self.assertEqual(CHANGED.format(build=image(OLD), test=image(NEW)), result.stderr)

    def test_only_one_identity_differs(self):
        # an image built again from the same Qore commit, and two images built at the same time from other commits
        for this in ({"created": NEW["created"], "commit": OLD["commit"]},
                     {"created": OLD["created"], "commit": NEW["commit"]}):
            with self.subTest(this=this):
                result = self.check(IMAGE_INFO.format(**OLD), IMAGE_INFO.format(**this))
                self.assertEqual(1, result.returncode, result.stdout + result.stderr)
                self.assertEqual(CHANGED.format(build=image(OLD), test=image(this)), result.stderr)

    def test_artifact_info_keys_are_not_the_image(self):
        # the artifact info's own created_utc, before the image section, and the keys of the section after it, are not
        # taken for the build job's image: both would be this job's image here
        result = self.check(IMAGE_INFO.format(**OLD), IMAGE_INFO.format(**NEW))
        self.assertEqual(CHANGED.format(build=image(OLD), test=image(NEW)), result.stderr)
        result = self.check(IMAGE_INFO.format(**NEW), IMAGE_INFO.format(**NEW))
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def test_unknown_images(self):
        self.assertFails(self.check(None, IMAGE_INFO.format(**NEW), artifact=False), "does not exist")
        self.assertFails(self.check(None, IMAGE_INFO.format(**NEW), section=False),
                         "has no [qore-test-base-build-info] section")
        self.assertFails(self.check(IMAGE_INFO.format(**OLD), None), "of this job's image does not exist")
        without_commit = re.sub(r"(?m)^qore_commit=.*\n", "", IMAGE_INFO.format(**OLD))
        self.assertFails(self.check(without_commit, IMAGE_INFO.format(**OLD)),
                         "the qore-test-base image of the build job is unknown")
        self.assertFails(self.check(IMAGE_INFO.format(**OLD), without_commit), "the image of this job is unknown")

    def test_test_scripts_check_before_testing(self):
        # each script that tests a build artifact runs the check before it installs or tests anything, and stops when
        # the check fails
        for name, first in (("test-ubuntu.sh", "setup_postgres_on_host"), ("test-alpine.sh", "setup_postgres_on_host"),
                            ("test-perf.sh", "cmake --install"), ("test-cmake.sh", "unittest discover")):
            with self.subTest(script=name):
                text = (DOCKER_TEST / name).read_text()
                call = re.search(r'ci-build-image-check\.sh"\s*\\?\s*"\$\{[A-Z_]+\}/(?:build/)?'
                                 r'qore-ci-build-artifact-info" \|\| exit 1\n', text)
                self.assertIsNotNone(call, name)
                self.assertLess(call.start(), text.index(first), name)


if __name__ == "__main__":
    unittest.main()
