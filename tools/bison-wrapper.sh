#!/bin/sh
set -e

bison_real="${BISON_REAL:-bison}"

if [ -z "$bison_real" ]; then
    bison_real="bison"
fi

for arg in "$@"; do
    case "$arg" in
        --version|-V|--help|-h)
            exec "$bison_real" "$@"
            ;;
    esac
done

input=""
for arg in "$@"; do
    case "$arg" in
        -*)
            ;;
        *)
            if [ -f "$arg" ]; then
                input="$arg"
            fi
            ;;
    esac
done

if [ -z "$input" ]; then
    exec "$bison_real" "$@"
fi

version=$("$bison_real" --version | head -n 1 | awk '{print $NF}')
major=$(printf '%s' "$version" | awk -F. '{print $1}')
minor=$(printf '%s' "$version" | awk -F. '{print $2}')

if [ -z "$major" ] || [ -z "$minor" ]; then
    exec "$bison_real" "$@"
fi

use_new_directives=0
if [ "$major" -gt 3 ] || { [ "$major" -eq 3 ] && [ "$minor" -ge 0 ]; }; then
    use_new_directives=1
fi

tmp="$(mktemp "${TMPDIR:-/tmp}/qore-bison.XXXXXX")"
trap 'rm -f "$tmp"' EXIT

if [ "$use_new_directives" -eq 0 ]; then
    # Older bison does not support %code blocks; strip them.
    awk '
    BEGIN { skip = 0 }
    /^[[:space:]]*%code[[:space:]]/ { skip = 1; next }
    skip {
        if ($0 ~ /^[[:space:]]*}[[:space:]]*$/) { skip = 0 }
        next
    }
    { print }
    ' "$input" > "$tmp"
else
    sed -e 's/^[[:space:]]*%pure-parser[[:space:]]*$/%define api.pure full/' \
        -e 's/^[[:space:]]*%error-verbose[[:space:]]*$/%define parse.error verbose/' \
        "$input" > "$tmp"
fi

set -- "$@"
new_args=""
for arg in "$@"; do
    if [ "$arg" = "$input" ]; then
        new_args="$new_args \"$tmp\""
    else
        new_args="$new_args \"$arg\""
    fi
done

eval exec "\"$bison_real\"" "$new_args"
