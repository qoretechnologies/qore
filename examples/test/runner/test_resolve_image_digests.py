#!/usr/bin/env python3
"""Exercise test/docker_test/resolve-image-digests.py, which resolves the qore-test-base image tags of a pipeline to
digests at its start, and the jobs of .gitlab-ci.yml that run on them.

In the batch/8 pipeline, the qore-test-base tags were published again between the build jobs and the test jobs: the
build jobs ran on the old images and the test jobs on the new ones.  The resolve-qore-test-base-images job now writes
the digest of each tag to a dotenv report when the pipeline starts, and every build and test job runs on its digest.
The cases run the script against a stand-in registry (Docker Registry HTTP API V2, with the token authentication of
the GitLab registry) and check that every job of .gitlab-ci.yml on a qore-test-base image takes it from the report.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import base64
import hashlib
import http.server
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import threading
import unittest
import urllib.parse


ROOT = Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "test/docker_test/resolve-image-digests.py"
CI = ROOT / ".gitlab-ci.yml"

USER = "ci-pull-user"
PASSWORD = "s3cret-pull-password-value"
TOKEN = "registry-bearer-token-value"
SERVICE = "container_registry"

INDEX = "application/vnd.oci.image.index.v1+json"
MANIFEST = "application/vnd.docker.distribution.manifest.v2+json"


class Registry:
    """A stand-in registry: tags of repositories, each with a manifest and its media type"""
    def __init__(self, auth="bearer"):
        self.auth = auth
        self.tags = {}
        self.requests = []
        self.digest_override = {}
        self.token_status = 200
        self.content_type_override = {}
        registry = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def send(self, status, body=b"", headers=()):
                self.send_response(status)
                for key, value in headers:
                    self.send_header(key, value)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def authorized(self):
                header = self.headers.get("Authorization")
                if registry.auth == "none":
                    return True
                if registry.auth == "basic":
                    return header == basic(USER, PASSWORD)
                return header == "Bearer " + TOKEN

            def challenge(self):
                if registry.auth == "basic":
                    return 'Basic realm="registry"'
                return 'Bearer realm="http://{}/jwt/auth",service="{}"'.format(self.headers["Host"], SERVICE)

            def do_GET(self):
                url = urllib.parse.urlsplit(self.path)
                registry.requests.append((url.path, dict(urllib.parse.parse_qsl(url.query)), dict(self.headers)))
                if url.path == "/jwt/auth":
                    if registry.token_status != 200:
                        self.send(registry.token_status, b'{"errors":[{"code":"DENIED","message":"no"}]}')
                    elif self.headers.get("Authorization") != basic(USER, PASSWORD):
                        self.send(401, b'{"errors":[{"code":"UNAUTHORIZED","message":"bad"}]}')
                    else:
                        self.send(200, json.dumps({"token": TOKEN}).encode(), [("Content-Type", "application/json")])
                    return
                if not self.authorized():
                    self.send(401, b'{"errors":[{"code":"UNAUTHORIZED","message":"auth"}]}',
                              [("WWW-Authenticate", self.challenge())])
                    return
                if url.path == "/v2/":
                    self.send(200, b"{}")
                    return
                match = re.match(r"^/v2/(.+)/manifests/([^/]+)$", url.path)
                key = match and (match.group(1), match.group(2))
                if not key or key not in registry.tags:
                    self.send(404, b'{"errors":[{"code":"MANIFEST_UNKNOWN","message":"secret detail"}]}')
                    return
                media_type, body = registry.tags[key]
                if media_type not in self.headers.get("Accept", ""):
                    # as a registry does for a type the client does not accept
                    self.send(404, b'{"errors":[{"code":"MANIFEST_UNKNOWN","message":"type"}]}')
                    return
                digest = registry.digest_override.get(key, "sha256:" + hashlib.sha256(body).hexdigest())
                headers = [("Content-Type", registry.content_type_override.get(key, media_type))]
                if digest:
                    headers.append(("Docker-Content-Digest", digest))
                self.send(200, body, headers)

        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.host = "127.0.0.1:{}".format(self.server.server_address[1])
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, *args):
        self.server.shutdown()
        self.thread.join()
        self.server.server_close()

    def add(self, repository, tag, body, media_type=MANIFEST):
        self.tags[(repository, tag)] = (media_type, body)
        return "sha256:" + hashlib.sha256(body).hexdigest()


def basic(user, password):
    return "Basic " + base64.b64encode("{}:{}".format(user, password).encode()).decode()


def auth_config(host, form="auth"):
    if form == "auth":
        entry = {"auth": base64.b64encode("{}:{}".format(USER, PASSWORD).encode()).decode()}
    else:
        entry = {"username": USER, "password": PASSWORD}
    return json.dumps({"auths": {host: entry}})


class ResolveImageDigestsTest(unittest.TestCase):
    def run_script(self, args, env, output_name="images.env"):
        with tempfile.TemporaryDirectory(prefix="qore-resolve-digests-") as directory:
            output = Path(directory) / output_name
            full_env = {"PATH": os.environ.get("PATH", "/usr/bin:/bin"), "QORE_IMAGE_DIGEST_REGISTRY_SCHEME": "http"}
            full_env.update(env)
            result = subprocess.run([sys.executable, "-I", "-B", "-W", "error", str(SCRIPT), str(output)] + args,
                                    env=full_env, capture_output=True, text=True, timeout=120)
            text = output.read_text() if output.exists() else None
            leftovers = sorted(p.name for p in Path(directory).iterdir() if p.name != output_name)
            self.assertEqual([], leftovers, "no temporary file is left behind")
            for secret in (PASSWORD, TOKEN, base64.b64encode("{}:{}".format(USER, PASSWORD).encode()).decode()):
                self.assertNotIn(secret, result.stdout + result.stderr, "no credential or token is printed")
            return result, text

    def test_resolves_every_tag_to_its_digest(self):
        with Registry() as registry:
            repo = "infrastructure/qore-test-base/qore-test-base"
            amd64 = registry.add(repo, "develop", b'{"schemaVersion":2,"manifests":[]}', INDEX)
            arm64 = registry.add(repo, "develop-arm64", b'{"schemaVersion":2,"layers":["arm64"]}')
            macos = registry.add(repo + "-macos", "develop", b'{"schemaVersion":2,"layers":["macos"]}',
                                 "application/vnd.oci.image.manifest.v1+json")
            result, text = self.run_script([
                "UBUNTU_AMD64={}/{}:develop".format(registry.host, repo),
                "UBUNTU_ARM64={}/{}:develop-arm64".format(registry.host, repo),
                "MACOS={}/{}-macos:develop".format(registry.host, repo),
            ], {"DOCKER_AUTH_CONFIG": auth_config(registry.host)})
            self.assertEqual(0, result.returncode, result.stderr)
            self.assertEqual("", result.stderr)
            self.assertEqual("UBUNTU_AMD64={h}/{r}@{a}\nUBUNTU_ARM64={h}/{r}@{b}\nMACOS={h}/{r}-macos@{m}\n".format(
                h=registry.host, r=repo, a=amd64, b=arm64, m=macos), text)
            self.assertIn("UBUNTU_AMD64: {}/{}:develop is {}".format(registry.host, repo, amd64), result.stdout)
            # one token per repository, asked for with the challenge's service and a pull scope
            tokens = [query for path, query, _ in registry.requests if path == "/jwt/auth"]
            self.assertEqual([{"scope": "repository:{}:pull".format(repo), "service": SERVICE},
                              {"scope": "repository:{}-macos:pull".format(repo), "service": SERVICE}], tokens)
            # every manifest request accepts the index and manifest list types, so the registry sends what it stores
            for path, _, headers in registry.requests:
                if "/manifests/" in path:
                    for media_type in (INDEX, "application/vnd.docker.distribution.manifest.list.v2+json",
                                       "application/vnd.oci.image.manifest.v1+json", MANIFEST):
                        self.assertIn(media_type, headers["Accept"])

    def test_credentials_forms(self):
        for name, env in (
                ("username and password", lambda h: {"DOCKER_AUTH_CONFIG": auth_config(h, "username")}),
                ("key with a scheme", lambda h: {"DOCKER_AUTH_CONFIG": json.dumps(
                    {"auths": {"https://" + h: json.loads(auth_config(h))["auths"][h]}})}),
                ("CI registry", lambda h: {"CI_REGISTRY": h, "CI_REGISTRY_USER": USER,
                                           "CI_REGISTRY_PASSWORD": PASSWORD}),
                ("other registry in DOCKER_AUTH_CONFIG, CI registry", lambda h: {
                    "DOCKER_AUTH_CONFIG": auth_config("other.example:5000"), "CI_REGISTRY": h,
                    "CI_REGISTRY_USER": USER, "CI_REGISTRY_PASSWORD": PASSWORD})):
            with self.subTest(credentials=name), Registry() as registry:
                digest = registry.add("group/image", "v1", b'{"v":1}')
                result, text = self.run_script(["IMAGE={}/group/image:v1".format(registry.host)],
                                               env(registry.host))
                self.assertEqual(0, result.returncode, result.stderr)
                self.assertEqual("IMAGE={}/group/image@{}\n".format(registry.host, digest), text)

    def test_basic_and_anonymous_registries(self):
        for auth, env in (("basic", lambda h: {"DOCKER_AUTH_CONFIG": auth_config(h)}), ("none", lambda h: {})):
            with self.subTest(auth=auth), Registry(auth) as registry:
                digest = registry.add("image", "latest", b'{"v":2}')
                result, text = self.run_script(["IMAGE={}/image:latest".format(registry.host)], env(registry.host))
                self.assertEqual(0, result.returncode, result.stderr)
                self.assertEqual("IMAGE={}/image@{}\n".format(registry.host, digest), text)
                self.assertFalse(any(path == "/jwt/auth" for path, _, _ in registry.requests))

    def test_digest_without_header_is_the_manifests_hash(self):
        with Registry() as registry:
            digest = registry.add("image", "t", b'{"v":3}')
            registry.digest_override[("image", "t")] = ""
            result, text = self.run_script(["IMAGE={}/image:t".format(registry.host)],
                                           {"DOCKER_AUTH_CONFIG": auth_config(registry.host)})
            self.assertEqual(0, result.returncode, result.stderr)
            self.assertEqual("IMAGE={}/image@{}\n".format(registry.host, digest), text)

    def failure(self, registry, args, env, message):
        result, text = self.run_script(args, env)
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertIsNone(text, "no dotenv report is written when a tag is not resolved")
        self.assertIn(message, result.stderr)
        self.assertIn("the pipeline must not run on the tags", result.stderr)
        return result

    def test_any_unresolved_tag_fails_and_writes_nothing(self):
        with Registry() as registry:
            registry.add("image", "present", b'{"v":4}')
            env = {"DOCKER_AUTH_CONFIG": auth_config(registry.host)}
            result = self.failure(registry, ["A={}/image:present".format(registry.host),
                                             "B={}/image:missing".format(registry.host)], env,
                                  "{}/image:missing: the manifest cannot be read: HTTP 404 (MANIFEST_UNKNOWN)"
                                  .format(registry.host))
            # only the registry's error code is shown, never the body
            self.assertNotIn("secret detail", result.stderr)

    def test_digest_header_mismatch_fails(self):
        with Registry() as registry:
            digest = registry.add("image", "t", b'{"v":5}')
            other = "sha256:" + "0" * 64
            registry.digest_override[("image", "t")] = other
            self.failure(registry, ["IMAGE={}/image:t".format(registry.host)],
                         {"DOCKER_AUTH_CONFIG": auth_config(registry.host)},
                         "the registry names the manifest {}, but the manifest it sent is {}".format(other, digest))

    def test_unexpected_content_type_fails(self):
        with Registry() as registry:
            registry.add("image", "t", b'{"v":6}')
            registry.content_type_override[("image", "t")] = "text/html"
            self.failure(registry, ["IMAGE={}/image:t".format(registry.host)],
                         {"DOCKER_AUTH_CONFIG": auth_config(registry.host)},
                         "the registry sent a manifest of type 'text/html'")

    def test_missing_or_refused_credentials_fail(self):
        with Registry() as registry:
            registry.add("image", "t", b'{"v":7}')
            arg = ["IMAGE={}/image:t".format(registry.host)]
            self.failure(registry, arg, {"DOCKER_AUTH_CONFIG": auth_config("other.example")},
                         "the registry needs credentials")
            self.failure(registry, arg, {"CI_REGISTRY": "other.example", "CI_REGISTRY_USER": USER,
                                         "CI_REGISTRY_PASSWORD": PASSWORD}, "the registry needs credentials")
            self.failure(registry, arg, {"DOCKER_AUTH_CONFIG": json.dumps(
                {"auths": {registry.host: {"auth": base64.b64encode(b"user:wrong").decode()}}})},
                "the token for pulling image was refused: HTTP 401 (UNAUTHORIZED)")
            registry.token_status = 403
            self.failure(registry, arg, {"DOCKER_AUTH_CONFIG": auth_config(registry.host)},
                         "the token for pulling image was refused: HTTP 403 (DENIED)")

    def test_malformed_auth_config_fails_without_showing_it(self):
        with Registry() as registry:
            registry.add("image", "t", b'{"v":8}')
            arg = ["IMAGE={}/image:t".format(registry.host)]
            result = self.failure(registry, arg, {"DOCKER_AUTH_CONFIG": "{not json " + PASSWORD},
                                  "DOCKER_AUTH_CONFIG is not a JSON object")
            self.assertNotIn(PASSWORD, result.stderr)
            self.failure(registry, arg, {"DOCKER_AUTH_CONFIG": json.dumps({"auths": {registry.host: {
                "auth": "!!!not-base64"}}})}, "is not base64 user:password")
            self.failure(registry, arg, {"DOCKER_AUTH_CONFIG": json.dumps({"auths": {registry.host: {
                "auth": base64.b64encode(b"no-colon").decode()}}})}, "is not base64 user:password")

    def test_unreachable_registry_fails(self):
        with Registry() as registry:
            host = registry.host
        # the stand-in has stopped: nothing listens on its port
        result, text = self.run_script(["IMAGE={}/image:t".format(host)], {})
        self.assertEqual(1, result.returncode)
        self.assertIsNone(text)
        self.assertIn("cannot be read", result.stderr)

    def test_bad_arguments(self):
        for args, message in (
                ([], "usage:"),
                (["IMAGE"], "is not <NAME>="),
                (["image=r.example/image:t"], "is not a variable name"),
                (["A=r.example/image:t", "A=r.example/image:u"], "the variable A is given twice"),
                (["A=r.example/image"], "is not <registry>/<repository>:<tag>"),
                (["A=r.example/image@sha256:" + "0" * 64], "is not <registry>/<repository>:<tag>"),
                (["A=image:t"], "is not <registry>/<repository>:<tag>")):
            with self.subTest(args=args):
                if not args:
                    result = subprocess.run([sys.executable, "-I", "-B", "-W", "error", str(SCRIPT)],
                                            capture_output=True, text=True, timeout=60)
                    self.assertEqual(2, result.returncode)
                    self.assertIn(message, result.stderr)
                    continue
                result, text = self.run_script(args, {})
                self.assertEqual(2, result.returncode, result.stderr)
                self.assertIsNone(text)
                self.assertIn(message, result.stderr)
        result, text = self.run_script(["A=r.example/image:t"], {"QORE_IMAGE_DIGEST_REGISTRY_SCHEME": "ftp"})
        self.assertEqual(2, result.returncode)
        self.assertIn("must be https or http", result.stderr)


def ci_jobs():
    """The top-level keys of .gitlab-ci.yml with the text of each"""
    text = CI.read_text()
    blocks = {}
    for match in re.finditer(r"(?ms)^([.\w-]+):\n(.*?)(?=^[.\w-]+:\n|\Z)", text):
        blocks[match.group(1)] = match.group(2)
    return text, blocks


class CiImageDigestsTest(unittest.TestCase):
    RESOLVER = "resolve-qore-test-base-images"

    def test_jobs_run_on_resolved_digests(self):
        text, blocks = ci_jobs()
        self.assertIn(self.RESOLVER, blocks)
        resolver = blocks[self.RESOLVER]
        self.assertRegex(resolver, r"(?m)^  stage: \.pre$")
        self.assertRegex(resolver, r"(?m)^      dotenv: qore-test-base-images\.env$")
        self.assertIn("test/docker_test/resolve-image-digests.py qore-test-base-images.env", resolver)
        # the variables the resolver writes, and the tags it resolves
        resolved = dict(re.findall(r'"(QORE_TEST_BASE_\w+_IMAGE)=\$\{QORE_TEST_BASE_REPOSITORY\}:([\w.-]+)"',
                                   resolver))
        self.assertEqual({"QORE_TEST_BASE_UBUNTU_AMD64_IMAGE": "develop",
                          "QORE_TEST_BASE_UBUNTU_ARM64_IMAGE": "develop-arm64",
                          "QORE_TEST_BASE_ALPINE_AMD64_IMAGE": "develop-alpine",
                          "QORE_TEST_BASE_ALPINE_ARM64_IMAGE": "develop-alpine-arm64"}, resolved)
        # each has a global value that no registry has, so a job that misses the report fails to pull its image
        # instead of running on the runner's default image (a report variable takes precedence over it)
        top = re.search(r"(?m)^variables:\n((?:[ \t]+.*\n|[ \t]*\n)+)", text).group(1)
        for name in resolved:
            placeholder = re.search(r'(?m)^  {}: "([^"]+)"$'.format(name), top)
            self.assertIsNotNone(placeholder, "{} has a global placeholder".format(name))
            self.assertTrue(placeholder.group(1).startswith("invalid.invalid/"), placeholder.group(1))

        used = set()
        checked = 0
        for job, block in blocks.items():
            image = re.search(r"(?m)^  image: (.+)$", block)
            if job in (self.RESOLVER, "tree-already-tested"):
                # the .pre jobs run on the tag: they do not use the image's Qore modules
                self.assertRegex(image.group(1), r":develop$")
                continue
            if not image:
                continue
            checked += 1
            with self.subTest(job=job):
                value = image.group(1)
                self.assertRegex(value, r"^\$QORE_TEST_BASE_\w+_IMAGE$", "the job runs on a resolved digest")
                name = value[1:]
                self.assertIn(name, resolved)
                used.add(name)
                # the architecture of the image is the runner's
                arch = "arm64" if name.endswith("_ARM64_IMAGE") else "amd64"
                self.assertRegex(block, r"(?m)^    - {}$".format(arch))
                # a job with needs gets the report only from a job it names
                needs = re.search(r"(?m)^  needs: (.+)$", block)
                if needs:
                    self.assertIn(self.RESOLVER, needs.group(1))
        self.assertGreater(checked, 10)
        self.assertEqual(set(resolved), used, "every resolved image is used")
        # no job image names a tag of qore-test-base besides the .pre jobs
        self.assertEqual(2, len(re.findall(r"(?m)^  image: .*qore-test-base.*:develop", text)))


if __name__ == "__main__":
    unittest.main()
