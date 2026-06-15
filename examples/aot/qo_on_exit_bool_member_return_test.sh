#!/bin/bash
# Regression: aggregate AOT must preserve immediate return values across
# on_exit cleanup.  This mirrors Qorus JobManager::isShutdown(), which locks a
# Mutex, registers an on_exit unlock handler, then returns a bool member.

set -euo pipefail

QORE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${QORE_ROOT}"

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

QCC="${QCC:-./build/qcc}"
mkdir -p "${TMP}/src" "${TMP}/qo"

source_id() {
    realpath "$1" | sed 's/[^A-Za-z0-9_]/_/g'
}

LOCKED_SRC="${TMP}/src/locked-flag.qc"
MAIN_SRC="${TMP}/src/main.q"
AGG_QO="${TMP}/qo/on_exit_bool_member_return_agg.qo"

cat >"${LOCKED_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

public namespace OMQ {
    public class LockedFlag {
        private {
            Mutex m();
            bool flag = True;
        }

        bool check() {
            return isSet();
        }

        string backgroundCheck() {
            Queue q();
            Counter c(1);
            background runBackgroundCheck(q, c);
            c.waitForZero();
            return q.get(1s);
        }

        private runBackgroundCheck(Queue q, Counter c) {
            on_exit c.dec();
            try {
                q.push(isSet() ? "ok" : "false");
            } catch (hash<ExceptionInfo> ex) {
                q.push(sprintf("%s: %s", ex.err, ex.desc));
            }
        }

        private bool isSet() {
            m.lock();
            on_exit m.unlock();

            return flag;
        }
    }
}
QORE

cat >"${MAIN_SRC}" <<'QORE'
%new-style
%strict-args
%require-types

int sub on_exit_bool_member_return_test() {
    OMQ::LockedFlag lf();
    if (!lf.check()) {
        throw "AOT-ON-EXIT-BOOL-RETURN-ERROR",
            sprintf("expected True from locked bool member return: %y", lf.check());
    }
    string bg = lf.backgroundCheck();
    if (bg != "ok") {
        throw "AOT-ON-EXIT-BOOL-RETURN-ERROR",
            sprintf("expected background check to return ok: %y", bg);
    }
    return 0;
}
QORE

LOCKED_ID="$(source_id "${LOCKED_SRC}")"
MAIN_ID="$(source_id "${MAIN_SRC}")"
LOCKED_QO="${TMP}/qo/${LOCKED_ID}.qo"
MAIN_QO="${TMP}/qo/${MAIN_ID}.qo"

echo "=== Step 1: batch-compile per-file script objects ==="
"${QCC}" -c --output-dir="${TMP}/qo" \
    "${LOCKED_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 2: compile native-registering script aggregate ==="
"${QCC}" -c \
    -o "${AGG_QO}" \
    --script-aggregate=on_exit_bool_member_return_agg \
    --script-aggregate-native-registers \
    "${LOCKED_SRC}" \
    "${MAIN_SRC}" | tail -6

echo ""
echo "=== Step 3: build and run aggregate registration harness ==="
cat >"${TMP}/harness.cpp" <<'EOF'
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <stdio.h>

extern "C" void init_on_exit_bool_member_return_agg_qo(QoreProgram*);

int main() {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
    qore_aot_script_begin_batch(pgm);
    init_on_exit_bool_member_return_agg_qo(pgm);
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        fprintf(stderr, "qore_aot_script_end_batch rc=%d: %s\n", flush_rc, qore_last_error(pgm));
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }
    int rc = qore_run_callable(pgm, "on_exit_bool_member_return_test", nullptr);
    qore_destroy_program(pgm);
    qore_cleanup();
    return rc;
}
EOF

g++ -std=c++20 -Iinclude -Lbuild \
    "${TMP}/harness.cpp" \
    "${AGG_QO}" \
    "${LOCKED_QO}" \
    "${MAIN_QO}" \
    -lqore -Wl,-rpath,build \
    -o "${TMP}/on_exit_bool_member_return_test"

LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    "${TMP}/on_exit_bool_member_return_test"

echo ""
echo "OK: aggregate AOT preserves bool member returns across on_exit cleanup."
