#!/bin/bash
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
# Install, upgrade and remove real package artifacts in a disposable testbed.
set -euo pipefail

if [[ $# -lt 4 || $# -gt 5 ]]; then
    echo "Usage: $0 SOURCE_DSC PACKAGES_DIRECTORY NEW_RESULTS_DIRECTORY BASE_IMAGE [PREVIOUS_PACKAGES_DIRECTORY]" >&2
    exit 2
fi
source_dsc=$(realpath "$1")
packages=$(realpath "$2")
results=$(realpath -m "$3")
image=$4
previous=${5:-}
[[ -f "$source_dsc" && -d "$packages" && ! -e "$results" ]] || {
    echo "Source/packages must exist; results directory must be new" >&2
    exit 2
}
[[ -z "$previous" ]] || previous=$(realpath "$previous")
for path in "$source_dsc" "$packages" "$results" "$previous"; do
    [[ "$path" != *:* ]] || { echo "Bind-mount paths must not contain colons" >&2; exit 2; }
done
mkdir -p "$(dirname "$results")"
mkdir "$results"
container="qore-package-test-$(date +%s)-$$"
trap 'podman rm -f "$container" >/dev/null 2>&1 || true' EXIT
podman image inspect "$image" > "$results/base-image.json"
volumes=(-v "$(dirname "$source_dsc"):/sources:ro" -v "$packages:/packages:ro" -v "$results:/results")
[[ -z "$previous" ]] || volumes+=(-v "$previous:/previous:ro")
# Optional signed repository configuration is explicit. Never import host APT
# configuration, private keys, or the host's development installation.
if [[ -n ${QORE_TEST_APT_SOURCE:-} || -n ${QORE_TEST_APT_KEY:-} ]]; then
    : "${QORE_TEST_APT_SOURCE:?Set both QORE_TEST_APT_SOURCE and QORE_TEST_APT_KEY}"
    : "${QORE_TEST_APT_KEY:?Set both QORE_TEST_APT_SOURCE and QORE_TEST_APT_KEY}"
    volumes+=(-v "$(realpath "$QORE_TEST_APT_SOURCE"):/repository.sources:ro"
              -v "$(realpath "$QORE_TEST_APT_KEY"):/repository.gpg:ro")
fi
podman run --interactive --name "$container" "${volumes[@]}" -e "SOURCE_DSC=$(basename "$source_dsc")" \
    "$image" /bin/bash -se > "$results/qualification.log" 2>&1 <<'TESTBED'
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
export LC_ALL=C.UTF-8
if [[ -f /repository.sources ]]; then
    apt-get update -o APT::Update::Error-Mode=any
    apt-get install -y --no-install-recommends ca-certificates
    cp /repository.gpg /usr/share/keyrings/qore-testing.gpg
    cp /repository.sources /etc/apt/sources.list.d/qore-testing.sources
fi
# Include docs and manpages in minimal distribution images so dpkg -V checks
# the actual package payload. This changes only the disposable container.
if [[ -f /etc/dpkg/dpkg.cfg.d/excludes ]]; then
    mv /etc/dpkg/dpkg.cfg.d/excludes /results/image-dpkg-excludes
fi
apt-get update -o APT::Update::Error-Mode=any
apt-get install -y --no-install-recommends dpkg-dev
dpkg-source --no-check -x "/sources/$SOURCE_DSC" /test-source
cat /etc/os-release > /results/os-release
. /etc/os-release
test "$(dpkg-parsechangelog -l /test-source/debian/changelog -S Distribution)" = "$VERSION_CODENAME"
dpkg --print-architecture > /results/architecture
shopt -s nullglob
debs=(/packages/*.deb)
[[ ${#debs[@]} -gt 0 ]]
names=()
version=
for deb in "${debs[@]}"; do
    package=$(dpkg-deb -f "$deb" Package)
    candidate=$(dpkg-deb -f "$deb" Version)
    [[ -z "$version" || "$candidate" = "$version" ]]
    version=$candidate
    names+=("$package")
    sha256sum "$deb" >> /results/package-sha256.txt
done
test "$version" = "$(dpkg-parsechangelog -l /test-source/debian/changelog -S Version)"
printf '%s\n' "$version" > /results/version
test "$(dpkg-parsechangelog -l /test-source/debian/changelog -S Source)" = qore

run_runtime() {
    mkdir -p "/results/$1"
    AUTOPKGTEST_TMP="/results/$1" /test-source/debian/tests/runtime > "/results/$1.log" 2>&1
}
if [[ -d /previous ]]; then
    old_debs=(/previous/*.deb)
    [[ ${#old_debs[@]} -gt 0 ]]
    for deb in "${old_debs[@]}"; do
        dpkg --compare-versions "$(dpkg-deb -f "$deb" Version)" lt "$version"
    done
    apt-get install -y --no-install-recommends "${old_debs[@]}"
    dpkg-query -W > /results/previous-inventory.txt
    qore -l json -l yaml -e 'if (parse_json("{\"answer\":42}").answer != 42 || parse_yaml("answer: 42\n").answer != 42) { exit(1); }'
    apt-get install -y --no-install-recommends "${debs[@]}"
    run_runtime upgraded-runtime
    printf '%s\n' PASS > /results/upgrade-result
    apt-get purge -y "${names[@]}"
else
    printf '%s\n' 'NOT RUN: no previous package artifacts supplied' > /results/upgrade-result
fi

# Minimal install precedes development/test dependencies, which could hide an
# undeclared runtime dependency. A separate fresh run is needed for each upgrade
# baseline; this path purges dependencies left by the previous package set.
apt-get autoremove --purge -y
apt-get install -y --no-install-recommends /packages/qore_*_*.deb \
    /packages/libqore20_*_*.deb /packages/qore-stdlib_*_*.deb
dpkg-query -W > /results/minimal-inventory.txt
run_runtime minimal-runtime
apt-get install -y --no-install-recommends autopkgtest
# The null backend is safe here because it is confined to this disposable
# container. It must never be used to install test dependencies on the host.
autopkgtest /test-source "${debs[@]}" --output-dir=/results/autopkgtest -- null
# Test dependencies are now installed. Use the supplied artifacts directly for
# the final coinstallation: APT can classify a different build of the same
# version as a downgrade when that version also exists in the configured PPA.
# dpkg still checks dependencies, and real version downgrades remain forbidden.
dpkg --refuse-downgrade --install "${debs[@]}"
for package in "${names[@]}"; do
    test "$(dpkg-query -W -f='${Version}' "$package")" = "$version"
done
dpkg-query -W > /results/coinstalled-inventory.txt
dpkg -V "${names[@]}" > /results/installed-file-verification.txt
test ! -s /results/installed-file-verification.txt
apt-get purge -y "${names[@]}"
test ! -e /usr/bin/qore
test ! -e /usr/bin/qcc
test ! -e /usr/bin/qdbg-server
apt-get autoremove --purge -y
dpkg-query -W > /results/purged-inventory.txt
apt-get install -y --no-install-recommends /packages/qore_*_*.deb \
    /packages/libqore20_*_*.deb /packages/qore-stdlib_*_*.deb
run_runtime reinstalled-runtime
printf '%s\n' 'PASS: installation, four autopkgtests, file verification, purge and reinstall' > /results/PASS
TESTBED
test -s "$results/PASS"
echo "Package qualification results: $results"
