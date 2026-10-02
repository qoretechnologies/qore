#!/bin/bash
# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
# Build one prepared source upload in a fresh Debian 13 or Ubuntu 26.04 container. Only the
# APT provisioning phase has network access; compilation and tests run offline.
set -euo pipefail

if [[ $# != 2 ]]; then
    echo "Usage: $0 PREPARED_PACKAGE_DIRECTORY NEW_RESULTS_DIRECTORY" >&2
    exit 2
fi
input=$(realpath "$1")
results=$(realpath -m "$2")
[[ -d "$input" && ! -e "$results" ]] || {
    echo "Input must exist; results directory must be new" >&2
    exit 2
}
[[ "$input" != *:* && "$results" != *:* ]] || {
    echo "Podman bind-mount paths must not contain colons" >&2
    exit 2
}
shopt -s nullglob
sources=("$input"/*.dsc)
[[ ${#sources[@]} == 1 ]] || { echo "Expected exactly one .dsc" >&2; exit 2; }
dependency_mount=()
if [[ -n ${QORE_BACKPORT_DEPENDENCIES:-} ]]; then
    dependencies=$(realpath "$QORE_BACKPORT_DEPENDENCIES")
    [[ -d "$dependencies" && "$dependencies" != *:* ]] || {
        echo "Dependency directory must exist and contain no colon" >&2
        exit 2
    }
    debs=("$dependencies"/*.deb)
    [[ ${#debs[@]} -gt 0 ]] || { echo "Dependency directory contains no .deb files" >&2; exit 2; }
    dependency_mount=(--volume "$dependencies:/dependencies:ro")
fi
mkdir -p "$(dirname "$results")"
mkdir "$results"
container="qore-backport-$(date +%s)-$$"
trap 'podman rm -f "$container" >/dev/null 2>&1 || true' EXIT
image=${QORE_BACKPORT_IMAGE:-docker.io/library/ubuntu:26.04}
podman image inspect "$image" > "$results/base-image.json"

podman run --name "$container" --volume "$input:/input:ro" "${dependency_mount[@]}" "$image" \
    /bin/bash -euc '
        export DEBIAN_FRONTEND=noninteractive
        . /etc/os-release
        case "$VERSION_CODENAME" in
            resolute|trixie) ;;
            *) echo "Unsupported backport build suite: $VERSION_CODENAME" >&2; exit 1 ;;
        esac
        apt-get update -o APT::Update::Error-Mode=any
        apt-get install -y --no-install-recommends build-essential debhelper fakeroot ca-certificates lintian
        mkdir /build
        dpkg-source -x /input/*.dsc /build/source
        source_suite=$(dpkg-parsechangelog -l /build/source/debian/changelog -S Distribution)
        if [[ "$source_suite" != "$VERSION_CODENAME" ]]; then
            echo "Source suite $source_suite does not match builder suite $VERSION_CODENAME" >&2
            exit 1
        fi
        if [[ -d /dependencies ]]; then
            sha256sum /dependencies/*.deb > /build/local-build-dependencies.sha256
            apt-get install -y --no-install-recommends /dependencies/*.deb
        fi
        apt-get build-dep -y --no-install-recommends /build/source
        dpkg-query -W > /build/installed-build-dependencies.txt
        useradd --create-home --user-group qore-builder
        chown -R qore-builder:qore-builder /build
    ' > "$results/provision.log" 2>&1
build_image=$(podman commit --quiet "$container")
printf '%s\n' "$build_image" > "$results/build-image.txt"
podman run --rm --network=none \
    --add-host=localhost:127.0.0.1 --add-host=localhost:::1 --volume "$input:/input:ro" \
    --volume "$results:/output" "$build_image" \
    /bin/bash -euc '
        cp /build/installed-build-dependencies.txt /output/
        if [[ -f /build/local-build-dependencies.sha256 ]]; then
            cp /build/local-build-dependencies.sha256 /output/
        fi
        cd /build/source
        dpkg-checkbuilddeps
        runuser -u qore-builder -- env DEB_BUILD_OPTIONS=parallel=4 dpkg-buildpackage -b -us -uc -j4
        shopt -s nullglob
        cp /build/*.deb /build/*.ddeb /build/*.changes /build/*.buildinfo /output/
        runuser -u qore-builder -- lintian --fail-on error /input/*_source.changes /output/*_*.changes
    ' > "$results/build.log" 2>&1
echo "Built and checked packages in $results"
