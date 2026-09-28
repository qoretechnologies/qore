#!/bin/bash
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
# Provision a fresh target builder, then build and test the .dsc offline.
set -euo pipefail
if [[ $# != 3 ]]; then
    echo "Usage: $0 SOURCE_DSC NEW_RESULTS_DIRECTORY BASE_IMAGE" >&2
    exit 2
fi
source_dsc=$(realpath "$1")
results=$(realpath -m "$2")
image=$3
[[ -f "$source_dsc" && ! -e "$results" ]] || {
    echo "Source must exist; results directory must be new" >&2
    exit 2
}
for path in "$source_dsc" "$results"; do
    [[ "$path" != *:* ]] || { echo "Bind-mount paths must not contain colons" >&2; exit 2; }
done
options=${QORE_DEB_BUILD_OPTIONS:-parallel=8 noautodbgsym}
profiles=${QORE_DEB_BUILD_PROFILES:-}
[[ ! "$options $profiles" =~ (^|[[:space:]])nocheck($|[[:space:]]) ]] || {
    echo "Qualification builds must run tests" >&2
    exit 2
}
mkdir -p "$(dirname "$results")"
mkdir "$results"
container="qore-package-build-$(date +%s)-$$"
trap 'podman rm -f "$container" >/dev/null 2>&1 || true' EXIT
volumes=(-v "$(dirname "$source_dsc"):/sources:ro")
if [[ -n ${QORE_TEST_APT_SOURCE:-} || -n ${QORE_TEST_APT_KEY:-} ]]; then
    : "${QORE_TEST_APT_SOURCE:?Set both QORE_TEST_APT_SOURCE and QORE_TEST_APT_KEY}"
    : "${QORE_TEST_APT_KEY:?Set both QORE_TEST_APT_SOURCE and QORE_TEST_APT_KEY}"
    volumes+=(-v "$(realpath "$QORE_TEST_APT_SOURCE"):/repository.sources:ro"
              -v "$(realpath "$QORE_TEST_APT_KEY"):/repository.gpg:ro")
fi
podman image inspect "$image" > "$results/base-image.json"
podman run --interactive --name "$container" "${volumes[@]}" \
    -e "SOURCE_DSC=$(basename "$source_dsc")" -e "DEB_BUILD_PROFILES=${QORE_DEB_BUILD_PROFILES:-}" \
    "$image" /bin/bash -se > "$results/provision.log" 2>&1 <<'PROVISION'
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get update -o APT::Update::Error-Mode=any
apt-get install -y --no-install-recommends ca-certificates build-essential debhelper fakeroot lintian
if [[ -f /repository.sources ]]; then
    cp /repository.gpg /usr/share/keyrings/qore-testing.gpg
    cp /repository.sources /etc/apt/sources.list.d/qore-testing.sources
    apt-get update -o APT::Update::Error-Mode=any
fi
mkdir /qualification
dpkg-source --no-check -x "/sources/$SOURCE_DSC" /qualification/source
. /etc/os-release
test "$(dpkg-parsechangelog -l /qualification/source/debian/changelog -S Distribution)" = "$VERSION_CODENAME"
apt-get build-dep -y --no-install-recommends /qualification/source
dpkg-query -W > /qualification/build-dependencies.txt
useradd --create-home --user-group qore-builder
chown -R qore-builder:qore-builder /qualification
PROVISION
build_image=$(podman commit --quiet "$container")
printf '%s\n' "$build_image" > "$results/build-image.txt"
printf '%s\n' "$options" > "$results/build-options.txt"
printf '%s\n' "${QORE_DEB_BUILD_PROFILES:-}" > "$results/build-profiles.txt"
podman run --rm --network=none -v "$results:/results" \
    -e "DEB_BUILD_OPTIONS=$options" -e "DEB_BUILD_PROFILES=${QORE_DEB_BUILD_PROFILES:-}" \
    "$build_image" /bin/bash -euc '
        cp /qualification/build-dependencies.txt /results/
        cd /qualification/source
        dpkg-checkbuilddeps
        runuser -u qore-builder -- env HOME=/sbuild-nonexistent dpkg-buildpackage -b -us -uc
        shopt -s nullglob
        packages=(/qualification/*.deb /qualification/*.ddeb)
        cp "${packages[@]}" /qualification/*.changes /qualification/*.buildinfo /results/
        lintian --allow-root --fail-on error "${packages[@]}"
        printf "%s\n" "PASS: offline build, tests and binary Lintian" > /results/PASS
    ' > "$results/build.log" 2>&1
test -s "$results/PASS"
echo "Offline build results: $results"
