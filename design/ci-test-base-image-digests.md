# CI: the qore-test-base images of a pipeline are resolved to digests at its start

Copyright (C) 2026 Qore Technologies, s.r.o.

## Problem

The Linux build and test jobs of `.gitlab-ci.yml` run on the qore-test-base images
(`registry.qoretechnologies.com/infrastructure/qore-test-base/qore-test-base`), whose tags (`develop`,
`develop-arm64`, `develop-alpine`, `develop-alpine-arm64`) are published again whenever the images are rebuilt.  A
build job compiles Qore and the qlib AOT modules against the binary modules of its image, and the test jobs test that
build artifact.  When a tag was published while a pipeline ran (pipeline 61639, the batch/8 pipeline), the build jobs
had run on the old image and the test jobs ran on the new one: the AOT modules were stale against the new image's
binary modules, and no test result was valid.  `examples/test/docker_test/ci-build-image-check.sh` fails such a test
job before any test runs, but the pipeline then has to be run again.

## Design

- The job `resolve-qore-test-base-images` (stage `.pre`) runs `test/docker_test/resolve-image-digests.py`, which
  reads the manifest of each tag from the registry (Docker Registry HTTP API V2) and writes
  `QORE_TEST_BASE_<UBUNTU|ALPINE>_<AMD64|ARM64>_IMAGE=<registry>/<repository>@sha256:<digest>` to the dotenv report
  `qore-test-base-images.env`.  The digest is the SHA-256 of the manifest bytes as stored (the request accepts the
  OCI image index and Docker manifest list types as well as image manifests, so an index is not converted), and must
  equal the registry's `Docker-Content-Digest`.
- Every build and test job on a qore-test-base image has `image: $QORE_TEST_BASE_<flavor>_<arch>_IMAGE`.  GitLab
  passes a dotenv report's variables to the jobs of later stages (the build-stage jobs, which have no `needs`) and to
  the jobs that name the job in `needs` (the test jobs name it), and the runner expands `image:` with the job's
  variables, so each job pulls by digest.  The tags are named once, in the resolver's arguments.
- Fail closed: the script writes the report only when every tag resolves (atomically: a temporary file renamed onto
  it), and otherwise exits 1; the build jobs then do not run (a failed `.pre` job stops the later stages) and the test
  jobs do not run (their `needs`).  As a second guard, the global `variables:` give each image variable a value in no
  registry (`invalid.invalid/unresolved/...`); a dotenv report variable takes precedence over a global variable, so a
  job that did not get the report fails to pull its image instead of running on a tag or on the runner's default
  image.  Nothing falls back to a tag.
- The resolver is run once more when it fails (`retry: max: 1` on `script_failure` and `runner_system_failure`), so
  a registry that is briefly unavailable does not fail the pipeline; a second failure does.
- Credentials: the image pulls of mirror/qore authenticate with the project's `DOCKER_AUTH_CONFIG` (the job token of
  mirror/qore is not in the inbound job token allowlist of infrastructure/qore-test-base), so the script uses the
  credentials of the registry in `DOCKER_AUTH_CONFIG` for the registry's token service, or else, for `CI_REGISTRY`,
  `CI_REGISTRY_USER` and `CI_REGISTRY_PASSWORD`.  It never prints a credential, a token or a response body (only the
  registry's error codes), and it follows no redirect, so credentials go only to the registry and its token realm.
- The resolver and `tree-already-tested` (both `.pre`) run on the `develop` tag itself: neither uses the image's Qore
  modules.  So do the GitHub status jobs `pipeline-passed` and `github-status` (`.post`, qore#7203), which must run
  even when the resolver failed, to post the failure.
- `ci-build-image-check.sh` remains as the backstop: a test job whose image is not its build job's still fails before
  any test runs.
- The macOS job (`test-macos`, tart executor) is deliberately not pinned (decided in the review of this change): it
  names no image (the runner's default,
  `qore-test-base-macos:develop`, is cloned once per job) and builds and tests in the same VM, so no other job can run
  on another macOS image.

## Tests

`examples/test/runner/test_resolve_image_digests.py` runs the script against a stand-in registry (bearer token,
basic and anonymous authentication; image index and manifest types; the credential forms; a missing tag, a digest that
does not match the manifest, an unexpected content type, refused or missing credentials, a malformed
`DOCKER_AUTH_CONFIG`, an unreachable registry and bad arguments, none of which writes a report or prints a credential)
and checks `.gitlab-ci.yml`: every job on a qore-test-base image other than the two `.pre` jobs runs on a resolved
variable of its architecture, every test job names the resolver in `needs`, and each variable has its placeholder.
`test-cmake-*` runs it with the other runner tests.

## Verifying a pipeline

Each job log names the image it ran on ("Using Kubernetes executor with image ...@sha256:..."); every job of a
flavor and architecture shows the digest that the resolver's log prints for its tag.

## Rollback

Revert the commit: the jobs then run on the tags again, with `ci-build-image-check.sh` as the only guard.
