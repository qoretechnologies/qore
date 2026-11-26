#!/usr/bin/env python3
"""The service containers of the CI jobs come from the qore-test-base registry, by digest, not from Docker Hub (#7615)

An anonymous pull from Docker Hub is rate limited per IP address: a job whose service image could not be pulled failed
in "prepare environment" before any test ran (pipeline 70167, job 248434: `unexpected media type text/html`).  The
default services of .gitlab-ci.yml name the copies that infrastructure/qore-test-base's manual job
mirror-service-images makes in its registry (service-mirror/<name>), each pinned by the digest that the job prints
(design/ci-service-images.md).

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[3]
CI = ROOT / ".gitlab-ci.yml"
DESIGN = ROOT / "design/ci-service-images.md"

MIRROR = "$CI_REGISTRY/infrastructure/qore-test-base/service-mirror"
# the alias that the jobs and the tests reach a service by -> the mirrored image
SERVICES = {"mysql": "mariadb", "pgsql": "pgvector", "memcached": "memcached", "redis": "redis", "mongodb": "mongo"}


def default_services(text):
    """The (name, alias) pairs of the default services of the CI configuration"""
    default = re.search(r"(?ms)^default:\n(.*?)^\S", text)
    assert default, "the CI configuration has a default section"
    services = re.search(r"(?ms)^  services:\n((?:    .*\n|\s*\n)+)", default.group(1))
    assert services, "the default section names services"
    return re.findall(r"(?m)^    - name: (\S+)\n      alias: (\S+)$", services.group(1))


class CiServiceImagesTest(unittest.TestCase):
    def test_the_default_services_are_the_mirrored_images_by_digest(self):
        services = default_services(CI.read_text())
        self.assertEqual(sorted(SERVICES), sorted(alias for _, alias in services))
        for name, alias in services:
            with self.subTest(alias=alias):
                self.assertRegex(name, r"^%s/%s@sha256:[0-9a-f]{64}$" % (re.escape(MIRROR), SERVICES[alias]))

    def test_no_job_names_a_docker_hub_service(self):
        # a service of a job, or of the default, that is a bare Docker Hub name (mongo:6.0, redis:latest) would be
        # pulled anonymously again
        text = CI.read_text()
        for name in re.findall(r"(?m)^\s+- name: (\S+)$", text):
            with self.subTest(name=name):
                self.assertTrue(name.startswith("$CI_REGISTRY/"), "%s is not from the project's registry" % name)
        self.assertNotRegex(text, r"(?m)^\s+- (?:mariadb|redis|mongo|memcached|pgvector)\S*$")

    def test_the_pins_are_different_digests_of_different_images(self):
        digests = [re.search(r"@(sha256:[0-9a-f]{64})$", name).group(1) for name, _ in default_services(CI.read_text())]
        self.assertEqual(len(digests), len(set(digests)))

    def test_the_design_note_names_the_mirror_and_the_way_to_change_a_pin(self):
        design = DESIGN.read_text()
        for needle in ("service-mirror", "mirror-service-images", "service-images.txt", "sha256", "DOCKER_AUTH_CONFIG"):
            with self.subTest(needle=needle):
                self.assertIn(needle, design)


if __name__ == "__main__":
    unittest.main()
