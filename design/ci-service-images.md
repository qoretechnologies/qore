# CI: the service containers come from the qore-test-base registry, by digest

Copyright (C) 2026 Qore Technologies, s.r.o.

## Problem

The CI jobs of `.gitlab-ci.yml` run service containers beside a job: MariaDB (alias `mysql`), PostgreSQL with pgvector
(`pgsql`), memcached, Redis and MongoDB (`mongodb`).  They were named as Docker Hub images (`mongo:6.0`,
`redis:latest`, ...), and an anonymous pull from Docker Hub is rate limited per IP address.  A job whose service image
could not be pulled failed in "prepare environment" before any test ran (pipeline 70167, job 248434:
`get image config descriptor: unexpected media type text/html`, ErrImagePull / ImagePullBackOff on `mongo:6.0`).  The
floating tags (`redis:latest`, `memcached:latest`) also let the test environment change without a commit.

## Design

- The default services name the copies in the registry of the qore-test-base project,
  `$CI_REGISTRY/infrastructure/qore-test-base/service-mirror/<name>@sha256:<digest>`, pinned by digest.
- The copies are made by the manual job `mirror-service-images` of infrastructure/qore-test-base
  (`service-images.txt` there lists the upstream references: `mariadb:11.4`, `pgvector/pgvector:pg18`,
  `memcached:latest`, `redis:latest`, `mongo:6.0`).  It copies each image with `docker buildx imagetools create`, which
  keeps the whole multi-arch index, so the digest of the copy is the digest of the upstream image; it reads the upstream
  digest first, copies by that digest and fails if the registry reports another one for the copy.  Each copy has the
  upstream tag and a `sha-<12 hex digits>` tag that nothing moves: the registry deletes images that no tag names, so a
  pin by digest needs a tag to stay pullable.  The job prints `<name> <repository>/<name>@sha256:<digest>` for each
  image: those are the references of `.gitlab-ci.yml`.
- The runner pulls them with the `DOCKER_AUTH_CONFIG` of the project, which holds the credentials of the registry
  (the job token of this project is not allowed to read the qore-test-base registry; see
  `ci-test-base-image-digests.md`).
- To move a service to another version, or to refresh a floating tag: change `service-images.txt` there if the version
  changes, run the job, and put the printed digests into the default `services:` here, in one commit.  Until then a
  refresh of the upstream tag changes nothing for a pipeline: the pull is by digest.

## Tests

`examples/test/runner/test_ci_service_images.py` checks that the default services are exactly the five aliases, each a
mirrored image by a SHA-256 digest, that no job names a service from outside the project's registry (a bare Docker Hub
name would be pulled anonymously again), that the digests differ, and that this note names the mirror and the way to
change a pin.  `test-cmake-*` runs it with the other runner tests.  What the registry holds is the mirror job's
(qore-test-base `test/test_service_images.py`).

## Rollback

Revert the commit: the services are the Docker Hub images again.
