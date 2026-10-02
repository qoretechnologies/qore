#!/bin/bash
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
# Check existing binary artifacts with an explicitly selected Lintian image.
set -euo pipefail

if [[ $# -lt 3 || $# -gt 4 ]]; then
    echo "Usage: $0 PACKAGES_DIRECTORY NEW_RESULTS_DIRECTORY CHECKER_IMAGE [PROFILE]" >&2
    exit 2
fi
packages=$(realpath "$1")
results=$(realpath -m "$2")
image=$3
profile=${4:-debian}
[[ -d "$packages" && ! -e "$results" ]] || {
    echo "Packages must exist; results directory must be new" >&2
    exit 2
}
for path in "$packages" "$results"; do
    [[ "$path" != *:* ]] || { echo "Bind-mount paths must not contain colons" >&2; exit 2; }
done
shopt -s nullglob
artifacts=("$packages"/*.deb "$packages"/*.ddeb)
[[ ${#artifacts[@]} -gt 0 ]] || { echo "No binary package artifacts found" >&2; exit 2; }
mkdir -p "$(dirname "$results")"
mkdir "$results"
checker_image=$(podman image inspect --format '{{.Id}}' "$image")
podman image inspect "$checker_image" > "$results/checker-image.json"
printf '%s\n' "$profile" > "$results/profile.txt"
status=0
podman run --rm --network=none -v "$packages:/packages:ro" -v "$results:/results" \
    -e "LINTIAN_PROFILE=$profile" "$checker_image" /bin/bash -euc '
        lintian --version > /results/version.txt
        shopt -s nullglob
        packages=(/packages/*.deb /packages/*.ddeb)
        lintian --allow-root --fail-on error --profile="$LINTIAN_PROFILE" "${packages[@]}"
    ' > "$results/lintian.log" 2>&1 || status=$?
printf '%s\n' "$status" > "$results/exit-code.txt"
[[ "$status" = 0 ]] || exit "$status"
printf '%s\n' 'PASS: binary Lintian' > "$results/PASS"
echo "Binary Lintian results: $results"
