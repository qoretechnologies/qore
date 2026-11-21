#!/usr/bin/env python3
"""Resolves image tags to the digests that they name now and writes them as a dotenv report.

usage: resolve-image-digests.py <dotenv file> <NAME>=<registry>/<repository>:<tag>...

Run by the resolve-qore-test-base-images job at the start of every pipeline: each <NAME> is written to the dotenv file
as <registry>/<repository>@sha256:<digest>, the image that the tag names when the job runs, and every build and test
job of the pipeline runs on the image of its variable (image: $<NAME> in .gitlab-ci.yml).  A tag published again
while the pipeline runs then changes nothing in it: before, the build jobs could run on one image and the test jobs on
the next one (the batch/8 pipeline), and the test jobs tested AOT modules compiled against the binary modules of
another image.

The digest is read from the registry with the Docker Registry HTTP API V2: the manifest of the tag (an image manifest,
an image index or a manifest list, whichever the tag names) is read, and its digest is the SHA-256 of the bytes read,
which must match the Docker-Content-Digest that the registry sends, if it sends one.  The registry is authenticated
as the runner authenticates its image pulls: with the credentials of the registry in DOCKER_AUTH_CONFIG, or else, for
the GitLab registry of the pipeline (CI_REGISTRY), with CI_REGISTRY_USER and CI_REGISTRY_PASSWORD.

It fails closed: when any tag cannot be resolved, no dotenv file is written and it returns 1, which fails the job and
so the pipeline; no job falls back to the tag.  Bad arguments return 2.  No credential or token is ever printed.

QORE_IMAGE_DIGEST_REGISTRY_SCHEME (https by default) is the URL scheme of the registry, for the tests.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import base64
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request


# the media types of the manifests that a tag can name; the registry sends the manifest as stored, whose digest is the
# one a pull by tag gets, only when the request accepts its type
MANIFEST_TYPES = (
    "application/vnd.oci.image.index.v1+json",
    "application/vnd.docker.distribution.manifest.list.v2+json",
    "application/vnd.oci.image.manifest.v1+json",
    "application/vnd.docker.distribution.manifest.v2+json",
)

TIMEOUT = 30

NAME = re.compile(r"^[A-Z_][A-Z0-9_]*$")
# <registry host[:port]>/<repository>:<tag> (no digest)
REFERENCE = re.compile(r"^(?P<registry>[A-Za-z0-9.-]+(?::[0-9]+)?)/(?P<repository>[a-z0-9]+(?:[._/-][a-z0-9]+)*)"
                       r":(?P<tag>[A-Za-z0-9_][A-Za-z0-9_.-]{0,127})$")
DIGEST = re.compile(r"^sha256:[0-9a-f]{64}$")
# a parameter of a WWW-Authenticate challenge: key="value" or key=token
CHALLENGE_PARAM = re.compile(r'(\w+)=(?:"([^"]*)"|([^\s,]+))')


class ResolveError(Exception):
    pass


class UsageError(Exception):
    pass


class NoRedirect(urllib.request.HTTPRedirectHandler):
    """Credentials are never sent on to another URL: a redirect is an error"""
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


OPENER = urllib.request.build_opener(NoRedirect)


def parse_args(argv):
    """Returns the dotenv file and the (name, registry, repository, tag) of each image"""
    if len(argv) < 2:
        raise UsageError("usage: resolve-image-digests.py <dotenv file> <NAME>=<registry>/<repository>:<tag>...")
    images = []
    names = set()
    for arg in argv[1:]:
        name, sep, reference = arg.partition("=")
        if not sep:
            raise UsageError("'{}' is not <NAME>=<registry>/<repository>:<tag>".format(arg))
        if not NAME.match(name):
            raise UsageError("'{}' is not a variable name (upper case letters, digits and _)".format(name))
        if name in names:
            raise UsageError("the variable {} is given twice".format(name))
        names.add(name)
        match = REFERENCE.match(reference)
        if not match:
            raise UsageError("the image of {}, '{}', is not <registry>/<repository>:<tag>".format(name, reference))
        images.append((name, match.group("registry"), match.group("repository"), match.group("tag")))
    return argv[0], images


def registry_credentials(registry):
    """The user and password for the registry, or None"""
    config = os.environ.get("DOCKER_AUTH_CONFIG")
    if config:
        try:
            auths = json.loads(config).get("auths", {})
        except (ValueError, AttributeError):
            # the message must not quote the value, which holds credentials
            raise ResolveError("DOCKER_AUTH_CONFIG is not a JSON object with \"auths\"")
        if not isinstance(auths, dict):
            raise ResolveError("the \"auths\" of DOCKER_AUTH_CONFIG is not an object")
        for key, entry in auths.items():
            host = urllib.parse.urlsplit(key).netloc if "://" in key else key.split("/", 1)[0]
            if host != registry or not isinstance(entry, dict):
                continue
            if entry.get("auth"):
                try:
                    user, sep, password = base64.b64decode(entry["auth"], validate=True).decode().partition(":")
                except (ValueError, UnicodeDecodeError):
                    raise ResolveError("the auth of {} in DOCKER_AUTH_CONFIG is not base64 user:password"
                                       .format(registry))
                if not sep:
                    raise ResolveError("the auth of {} in DOCKER_AUTH_CONFIG is not base64 user:password"
                                       .format(registry))
                return user, password
            if entry.get("username") and entry.get("password"):
                return entry["username"], entry["password"]
    if registry == os.environ.get("CI_REGISTRY") and os.environ.get("CI_REGISTRY_USER") \
            and os.environ.get("CI_REGISTRY_PASSWORD"):
        return os.environ["CI_REGISTRY_USER"], os.environ["CI_REGISTRY_PASSWORD"]
    return None


def basic(credentials):
    return "Basic " + base64.b64encode("{}:{}".format(*credentials).encode()).decode()


def request(url, headers, what):
    """Returns (status, headers, body) of a GET; an HTTP error status is returned, not raised"""
    req = urllib.request.Request(url, headers=headers)
    try:
        with OPENER.open(req, timeout=TIMEOUT) as response:
            return response.status, response.headers, response.read()
    except urllib.error.HTTPError as err:
        with err:
            return err.code, err.headers, err.read()
    except (urllib.error.URLError, OSError) as err:
        # the URL has no credentials; the error has no headers
        raise ResolveError("{}: {} cannot be read: {}".format(what, url, getattr(err, "reason", err)))


def error_detail(body):
    """The registry's error codes in a response body, never the body itself"""
    try:
        codes = [e.get("code") for e in json.loads(body).get("errors", []) if isinstance(e, dict)]
    except (ValueError, AttributeError):
        return ""
    codes = [c for c in codes if isinstance(c, str) and re.match(r"^[A-Z_]+$", c)]
    return " ({})".format(", ".join(codes)) if codes else ""


class Registry:
    def __init__(self, scheme, registry):
        self.scheme = scheme
        self.registry = registry
        self.base = "{}://{}/v2/".format(scheme, registry)
        self.challenge = None
        self.tokens = {}

    def authorization(self, repository):
        """The Authorization header for pulling from the repository, or None when the registry needs none"""
        if self.challenge is None:
            status, headers, body = request(self.base, {}, self.registry)
            if status == 200:
                self.challenge = ("", {})
            elif status == 401:
                header = headers.get("WWW-Authenticate", "")
                scheme, _, params = header.partition(" ")
                self.challenge = (scheme.lower(), {m.group(1).lower(): m.group(2) if m.group(2) is not None
                                                   else m.group(3) for m in CHALLENGE_PARAM.finditer(params)})
                if self.challenge[0] not in ("bearer", "basic"):
                    raise ResolveError("{}: the registry asks for authentication that is not supported: '{}'"
                                       .format(self.registry, scheme))
            else:
                raise ResolveError("{}: {} returned HTTP {}{}".format(self.registry, self.base, status,
                                                                     error_detail(body)))
        scheme, params = self.challenge
        if not scheme:
            return None
        credentials = registry_credentials(self.registry)
        if credentials is None:
            raise ResolveError("{}: the registry needs credentials, and neither DOCKER_AUTH_CONFIG nor CI_REGISTRY_USER "
                               "and CI_REGISTRY_PASSWORD (for CI_REGISTRY) have any for it".format(self.registry))
        if scheme == "basic":
            return basic(credentials)
        if repository not in self.tokens:
            realm = params.get("realm")
            if not realm or urllib.parse.urlsplit(realm).scheme != self.scheme:
                raise ResolveError("{}: the registry's token realm '{}' is not a {} URL".format(self.registry, realm,
                                                                                               self.scheme))
            query = {"scope": "repository:{}:pull".format(repository)}
            if params.get("service"):
                query["service"] = params["service"]
            url = realm + ("&" if "?" in realm else "?") + urllib.parse.urlencode(query)
            status, _, body = request(url, {"Authorization": basic(credentials)}, self.registry)
            if status != 200:
                raise ResolveError("{}: the token for pulling {} was refused: HTTP {}{}".format(
                    self.registry, repository, status, error_detail(body)))
            try:
                answer = json.loads(body)
                token = answer.get("token") or answer.get("access_token")
            except (ValueError, AttributeError):
                token = None
            if not isinstance(token, str) or not token:
                raise ResolveError("{}: the token service sent no token for pulling {}".format(self.registry,
                                                                                               repository))
            self.tokens[repository] = token
        return "Bearer " + self.tokens[repository]

    def digest(self, repository, tag):
        """The digest of the manifest that the tag names"""
        headers = {"Accept": ", ".join(MANIFEST_TYPES)}
        authorization = self.authorization(repository)
        if authorization:
            headers["Authorization"] = authorization
        image = "{}/{}:{}".format(self.registry, repository, tag)
        url = "{}{}/manifests/{}".format(self.base, repository, tag)
        status, response_headers, body = request(url, headers, image)
        if status != 200:
            raise ResolveError("{}: the manifest cannot be read: HTTP {}{}".format(image, status, error_detail(body)))
        media_type = response_headers.get("Content-Type", "").split(";", 1)[0].strip()
        if media_type not in MANIFEST_TYPES:
            raise ResolveError("{}: the registry sent a manifest of type '{}', which is none of {}".format(
                image, media_type, ", ".join(MANIFEST_TYPES)))
        digest = "sha256:" + hashlib.sha256(body).hexdigest()
        sent = response_headers.get("Docker-Content-Digest")
        if sent is not None and sent.strip() != digest:
            raise ResolveError("{}: the registry names the manifest {}, but the manifest it sent is {}".format(
                image, sent.strip(), digest))
        assert DIGEST.match(digest)
        return digest


def resolve(images, scheme):
    """Returns the lines of the dotenv file: <NAME>=<registry>/<repository>@<digest>"""
    registries = {}
    lines = []
    for name, registry, repository, tag in images:
        if registry not in registries:
            registries[registry] = Registry(scheme, registry)
        digest = registries[registry].digest(repository, tag)
        print("{}: {}/{}:{} is {}".format(name, registry, repository, tag, digest))
        lines.append("{}={}/{}@{}".format(name, registry, repository, digest))
    return lines


def write_atomically(path, lines):
    """Writes the file whole or not at all"""
    path = Path(path)
    fd, temp = tempfile.mkstemp(dir=str(path.parent if str(path.parent) else "."), prefix=path.name + ".")
    try:
        with os.fdopen(fd, "w") as out:
            out.write("".join(line + "\n" for line in lines))
        os.replace(temp, str(path))
    except BaseException:
        try:
            os.unlink(temp)
        except OSError:
            pass
        raise


def main(argv):
    try:
        output, images = parse_args(argv)
    except UsageError as err:
        print(err, file=sys.stderr)
        return 2
    scheme = os.environ.get("QORE_IMAGE_DIGEST_REGISTRY_SCHEME", "https")
    if scheme not in ("https", "http"):
        print("QORE_IMAGE_DIGEST_REGISTRY_SCHEME must be https or http, not '{}'".format(scheme), file=sys.stderr)
        return 2
    try:
        lines = resolve(images, scheme)
        write_atomically(output, lines)
    except ResolveError as err:
        print("ERROR: {}; no image is resolved, and the pipeline must not run on the tags".format(err),
              file=sys.stderr)
        return 1
    except OSError as err:
        print("ERROR: {} cannot be written: {}".format(output, err), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
