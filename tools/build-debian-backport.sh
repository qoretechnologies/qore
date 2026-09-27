#!/bin/bash
# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
# Build one prepared source upload in a fresh Ubuntu 26.04 container. Only the
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
mkdir -p "$(dirname "$results")"
mkdir "$results"
container="qore-backport-$(date +%s)-$$"
trap 'podman rm -f "$container" >/dev/null 2>&1 || true' EXIT
image=${QORE_BACKPORT_IMAGE:-docker.io/library/ubuntu:26.04}
podman image inspect "$image" > "$results/base-image.json"

podman run --name "$container" --volume "$input:/input:ro" "$image" \
    /bin/bash -euc '
        export DEBIAN_FRONTEND=noninteractive
        . /etc/os-release
        test "$VERSION_CODENAME" = resolute
        apt-get update
        apt-get install -y --no-install-recommends build-essential debhelper fakeroot ca-certificates lintian
        mkdir /build
        dpkg-source -x /input/*.dsc /build/source
        test "$(dpkg-parsechangelog -l /build/source/debian/changelog -S Distribution)" = resolute
        apt-get build-dep -y --no-install-recommends /build/source
        dpkg-query -W > /build/installed-build-dependencies.txt
        useradd --create-home --user-group qore-builder
        chown -R qore-builder:qore-builder /build
    ' > "$results/provision.log" 2>&1
build_image=$(podman commit --quiet "$container")
printf '%s\n' "$build_image" > "$results/build-image.txt"
podman run --rm --network=none --volume "$input:/input:ro" \
    --volume "$results:/output" "$build_image" \
    /bin/bash -euc '
        cp /build/installed-build-dependencies.txt /output/
        cd /build/source
        dpkg-checkbuilddeps
        runuser -u qore-builder -- env DEB_BUILD_OPTIONS=parallel=4 dpkg-buildpackage -b -us -uc -j4
        shopt -s nullglob
        cp /build/*.deb /build/*.ddeb /build/*.changes /build/*.buildinfo /output/
        runuser -u qore-builder -- lintian --fail-on error /input/*_source.changes /output/*_*.changes
    ' > "$results/build.log" 2>&1
echo "Built and checked packages in $results"
