#!/bin/bash
# Regression: qmod native-body registration must bind each overloaded variant
# by its exact signature.  In particular, an `auto` overload must not steal the
# native body for a more specific object or hashdecl overload.

set -euo pipefail

QORE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${QORE_ROOT}"

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

QCC="${QCC:-./build/qcc}"
QORE="${QORE:-./build/qore}"
mkdir -p "${TMP}/src" "${TMP}/mod"

cat >"${TMP}/src/OverloadQmod.qm" <<'QORE'
%modern

module OverloadQmod {
    version = "1.0";
    desc = "qmod overload native binding regression";
    author = "Qore Technologies";
    url = "https://qoretechnologies.com";
    license = "MIT";
}

public namespace OverloadQmod {
public hashdecl Expr {
    string exp;
}

public class Base {
}

public class Receiver {
    private {
        string hit = "";
    }

    submit(auto value) {
        hit = "auto";
    }

    submit(Base value) {
        hit = "base";
    }

    string getHit() {
        return hit;
    }

    static string staticPick(auto value) {
        return "auto";
    }

    static string staticPick(hash<Expr> value) {
        return "expr";
    }
}
}
QORE

cat >"${TMP}/consumer.q" <<QORE
%modern
%prepend-module-path "${TMP}/mod"
%requires OverloadQmod

class Child inherits OverloadQmod::Base {
}

OverloadQmod::Receiver r();
Child c();
r.submit(c);
printf("submit=%s\n", r.getHit());
printf("static=%s\n", OverloadQmod::Receiver::staticPick(<OverloadQmod::Expr>{"exp": "x"}));
QORE

QORE_LIBDIR="${QORE_ROOT}/build" \
    "${QCC}" -m --output "${TMP}/mod/OverloadQmod.qmod" \
    "${TMP}/src/OverloadQmod.qm" | tail -2

source_out="$(QORE_BUILD_DIR="${QORE_ROOT}/build" \
    "${QORE}" -l "${TMP}/src/OverloadQmod.qm" "${TMP}/consumer.q")"
qmod_out="$(QORE_BUILD_DIR="${QORE_ROOT}/build" \
    "${QORE}" "${TMP}/consumer.q")"

expected=$'submit=base\nstatic=expr'
test "${source_out}" = "${expected}"
test "${qmod_out}" = "${expected}"
printf '%s\n' "${qmod_out}"

echo ""
echo "OK: qmod overloaded variants bind native bodies by exact signature."
