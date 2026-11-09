# The runners of the qmod build cache

The CI configuration that the qmod build cache (`design/qmod-build-cache.md`, "Trust levels" and "CI") needs outside
this repository: in GitLab and in the runners.  The runner files are in the infrastructure/k8s repository,
`gitlab-runner/` (README.md, "Qore qmod build cache").

Copyright (C) 2026 Qore Technologies, s.r.o.

## GitLab (mirror/qore): done

- Environment `qmod-cache-build`: only the build jobs that run on the writer's runner name it (`.qmod-cache` in
  `.gitlab-ci.yml`, `QORE_QMOD_CACHE_ENVIRONMENT`), with action `prepare`
  (no deployment).
- `QORE_QMOD_CACHE_SIGNING_KEY`: the Ed25519 private key, a variable of type File, protected, scoped to the
  environment `qmod-cache-build`, so that only the build jobs of protected refs get it.
- `QORE_QMOD_CACHE_VERIFY_KEY`: the public key as PEM text, a variable of type env_var, not protected, all
  environments.
- The refs that write the cache must be protected (`CI_COMMIT_REF_PROTECTED=true`): a job of an unprotected ref
  never gets the private key and only reads.  `develop` must therefore be a protected branch of mirror/qore, with
  the mirroring user (a Maintainer) allowed to push to it.

## What every runner must provide

- `QORE_QMOD_CACHE_DIR` in the job's environment, naming the directory whose `protected/` the runner mounts;
  `test/docker_test/qmod-cache.sh` uses `protected/<job name>` in it and creates that directory when it can write.
  Without the variable, or without `protected/`, the job builds without the cache.
- The same checkout path in every job (the key has absolute paths): `/builds/mirror/qore` on Kubernetes and
  `/private/var/tmp/builds/mirror/qore` on the macOS VMs today, neither of which has a concurrency number in it.  A
  runner whose default path has one must allow `GIT_CLONE_PATH` (`[runners.custom_build_dir] enabled = true`), and
  the jobs must set it.
- An OpenSSL 3 command in the job (Ed25519 signatures): the openssl of the `PATH`, `/opt/local/bin/openssl`,
  `/opt/homebrew/opt/openssl@3/bin/openssl`, `/usr/local/opt/openssl@3/bin/openssl` or `/usr/bin/openssl` are tried,
  or `QORE_QMOD_CACHE_OPENSSL` names it.  The macOS image installs Homebrew's `openssl@3` (keg-only, found at
  `/opt/homebrew/opt/openssl@3/bin/openssl`; macOS's own openssl is LibreSSL, which has no Ed25519); the Linux images
  have OpenSSL 3.  A job without it builds without the cache.
- A runner of protected refs mounts `protected/` read-write; a runner of any other ref mounts it read-only or not at
  all, and runs no privileged pods and adds no capabilities (either could mount it again, read-write).  A job of a
  protected ref on a read-only mount only reads.  So the jobs of protected refs and of the other refs need separate
  runners, and the jobs choose them by tag according to `CI_COMMIT_REF_PROTECTED`.
- No other project's runner mounts the directory: the cache runners are registered for mirror/qore only.

## macOS (professor, tart executor)

- Host directory: `/Users/david/ci/qore-qmod-cache/protected` (the runner runs as david), owned by the runner's
  user, `/Users/david/ci/qore-qmod-cache` mode 0700.
- The runner of protected refs (a new registration for mirror/qore with access level `ref_protected`, its own tag)
  adds `"--dir", "qmod-cache:/Users/david/ci/qore-qmod-cache"` to `prepare_args` and
  `QORE_QMOD_CACHE_DIR=/Volumes/My Shared Files/qmod-cache` to `environment`.
- The existing runner (`macOS Runner (prof)`, every ref) adds the same with `:ro`
  (`"qmod-cache:/Users/david/ci/qore-qmod-cache:ro"`): a VM cannot remount the host's directory read-write.
- `test-macos` takes the writer's tag on protected refs.
- The same mount holds the C and C++ compiler cache of `test-macos` (`examples/test/docker_test/ccache-ci.sh`):
  `/Users/david/ci/qore-qmod-cache/ccache`, beside `protected/` (create it, owned by the runner's user); the job uses
  it when the directory exists and the image has ccache (Homebrew `ccache`, installed by qore-test-base's
  `prep-macos.sh`), writes it only on a protected ref with the read-write mount, and keeps a cache per toolchain
  under it.  It is not in `protected/`, whose directories the qmod cache prunes as its own.

## Kubernetes (amd64, arm64)

- Node directory: `/var/cache/qore-qmod-cache/protected` on every build node, root:root 0755
  (`gitlab-runner/qmod-cache-node-prepare.yaml`).  The jobs keep each job name's cache below
  `QORE_QMOD_CACHE_MAX_SIZE_MB` (3072, `.gitlab-ci.yml`): three amd64 job names, two arm64.
- The runner of protected refs (per architecture: mirror/qore, access level `ref_protected`, tags
  `qmod-cache-writer` and the architecture): `privileged = false`, no `cap_add`, a `host_path` volume of the node
  directory mounted read-write at `/qmod-cache/protected`, `QORE_QMOD_CACHE_DIR=/qmod-cache`; the build container
  runs as root (the directory is root's).
- The runner of the other refs (mirror/qore, `not_protected`, tags `qmod-cache-reader` and the architecture): the
  same with `read_only = true`.
- The existing shared runners (`k8s amd64 runner`, `k8s arm64 runner`) serve every project and ref and run
  privileged pods (docker-in-docker): they mount nothing.  Their privileged job pods on the same nodes can still
  reach the host directory; they cannot sign an entry, so what they write is a miss, but they could delete entries
  or fill the disk.
- `gitlab-runner/values-qmod-cache-<arch>.yaml` with `values-qmod-cache-<writer|reader>.yaml`, installed by
  `install-runner-qmod-cache.sh <arch> <role>`; the runner token is in the secret
  `gitlab-runner-<arch>-qmod-cache-<role>`.

## The jobs

The build jobs (`build-ubuntu-*`, `build-alpine-*`, `build-perf`) and `test-macos` choose their runner by tag (the
`.qmod-cache` template in `.gitlab-ci.yml`): `QORE_QMOD_CACHE_RUNNER_TAG` replaces `k8s` in the build jobs' tags
(the architecture tag stays), and `QORE_QMOD_CACHE_MACOS_RUNNER_TAG` is `test-macos`'s tag.  The template's rules set
them:

| `QORE_QMOD_CACHE_RUNNERS` | `CI_COMMIT_REF_PROTECTED` | Kubernetes tags | macOS tag |
|---|---|---|---|
| unset (default) | any | `k8s`, architecture (shared runners, no cache) | `macos` (existing runner) |
| `1` | `true` | `qmod-cache-writer`, architecture | `macos-qmod-cache-writer` |
| `1` | otherwise | `qmod-cache-reader`, architecture | `macos` (existing runner, mounted `:ro`) |

`QORE_QMOD_CACHE_RUNNERS` is a CI/CD variable of mirror/qore, set to `1` only after the writer and reader runners are
registered (infrastructure/k8s!85 and the macOS registration above): until then every job keeps the shared runners'
tags and builds without the cache, so this configuration can be merged before the runners exist.  Setting the variable
before they exist leaves the jobs pending, as no runner has the tags.  The rules also keep the jobs out of merge request
pipelines, as for jobs without rules.

The environment that the signing key's variable is scoped to follows the runner: the template's
`QORE_QMOD_CACHE_ENVIRONMENT` is `qmod-cache-read` (an environment no secret is scoped to), and only the writer's rule
(`QORE_QMOD_CACHE_RUNNERS=1` on a protected ref) sets it to `qmod-cache-build`.  GitLab writes a File variable into
every job of its environment, so a protected job on the shared runners, which run privileged pods (or on the macOS
runner every ref uses), must never name `qmod-cache-build`: while `QORE_QMOD_CACHE_RUNNERS` is unset, no job does.

A pipeline variable (a variable given when a pipeline is run or by a trigger) overrides these variables, so it could
name `qmod-cache-build` on any runner: in mirror/qore, pipeline variables must be restricted to Maintainers (project
setting "Minimum role to use pipeline variables": Maintainer, or no one), the same users who can change the CI
configuration of a protected branch anyway.

`develop` is a protected branch of mirror/qore: its jobs get the writer's runner and the signing key once
`QORE_QMOD_CACHE_RUNNERS` is `1` and the writer runners are registered; until then no job writes the cache.
