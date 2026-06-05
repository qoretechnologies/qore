#!/bin/sh

set -eu

usage() {
    cat <<EOF
Usage: $0 [options]

Runs plugin-type verification checks against a Qore build tree.

Options:
  --build-dir DIR        Build directory containing qore (default: build).
  --qore-bin FILE        Qore executable (default: <build-dir>/qore).
  --module NAME          Plugin module name to lint.
  --module-ref REF       Module load reference or qmod path (default: module name).
  --qtest FILE           Qore test/workload to run in all selected exec modes.
  --exec-modes LIST      Comma-separated modes (default: ast,ir,jit,tiered).
  --strict-lint          Fail lint on warnings and missing runtime helper symbols.
  --require-llvm         Require every linted operation to expose accepted LLVM codegen.
  --skip-internal-smoke  Do not build/run qore-ir-plugin-registry-smoke.
  -h, --help             Show this help.
EOF
}

BUILD_DIR="${QORE_BUILD_DIR:-build}"
QORE_BIN=""
MODULE_NAME=""
MODULE_REF=""
QTESTS=""
EXEC_MODES="${QORE_PLUGIN_VERIFY_MODES:-ast ir jit tiered}"
RUN_INTERNAL_SMOKE=1
STRICT_LINT=0
REQUIRE_LLVM=0

while [ "$#" -gt 0 ]; do
    case "$1" in
        --build-dir)
            [ "$#" -ge 2 ] || { echo "ERROR: --build-dir requires a value" >&2; exit 2; }
            BUILD_DIR="$2"
            shift 2
            ;;
        --build-dir=*)
            BUILD_DIR=${1#*=}
            shift
            ;;
        --qore-bin)
            [ "$#" -ge 2 ] || { echo "ERROR: --qore-bin requires a value" >&2; exit 2; }
            QORE_BIN="$2"
            shift 2
            ;;
        --qore-bin=*)
            QORE_BIN=${1#*=}
            shift
            ;;
        --module)
            [ "$#" -ge 2 ] || { echo "ERROR: --module requires a value" >&2; exit 2; }
            MODULE_NAME="$2"
            shift 2
            ;;
        --module=*)
            MODULE_NAME=${1#*=}
            shift
            ;;
        --module-ref)
            [ "$#" -ge 2 ] || { echo "ERROR: --module-ref requires a value" >&2; exit 2; }
            MODULE_REF="$2"
            shift 2
            ;;
        --module-ref=*)
            MODULE_REF=${1#*=}
            shift
            ;;
        --qtest)
            [ "$#" -ge 2 ] || { echo "ERROR: --qtest requires a value" >&2; exit 2; }
            QTESTS="${QTESTS}${QTESTS:+ }$2"
            shift 2
            ;;
        --qtest=*)
            QTESTS="${QTESTS}${QTESTS:+ }${1#*=}"
            shift
            ;;
        --exec-modes)
            [ "$#" -ge 2 ] || { echo "ERROR: --exec-modes requires a value" >&2; exit 2; }
            EXEC_MODES=$(printf '%s' "$2" | tr ',' ' ')
            shift 2
            ;;
        --exec-modes=*)
            EXEC_MODES=$(printf '%s' "${1#*=}" | tr ',' ' ')
            shift
            ;;
        --strict-lint)
            STRICT_LINT=1
            shift
            ;;
        --require-llvm)
            REQUIRE_LLVM=1
            shift
            ;;
        --skip-internal-smoke)
            RUN_INTERNAL_SMOKE=0
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "ERROR: unexpected argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [ -z "$QORE_BIN" ]; then
    QORE_BIN="$BUILD_DIR/qore"
fi
if [ ! -x "$QORE_BIN" ]; then
    echo "ERROR: qore executable not found or not executable: $QORE_BIN" >&2
    exit 2
fi

if [ -z "$MODULE_REF" ] && [ -n "$MODULE_NAME" ]; then
    MODULE_REF="$MODULE_NAME"
fi

MODULE_PATHS=""
append_module_path() {
    [ -d "$1" ] || return 0
    if [ -z "$MODULE_PATHS" ]; then
        MODULE_PATHS="$1"
    else
        MODULE_PATHS="$MODULE_PATHS:$1"
    fi
}

append_module_path "$BUILD_DIR/modules/reflection"
append_module_path "$BUILD_DIR/modules/dataframe"
for module_dir in "$BUILD_DIR"/modules/*; do
    [ -d "$module_dir" ] && append_module_path "$module_dir"
done
append_module_path "./qlib"

if [ -n "$MODULE_PATHS" ]; then
    export QORE_MODULE_DIR="$MODULE_PATHS${QORE_MODULE_DIR:+:$QORE_MODULE_DIR}"
fi
export LD_LIBRARY_PATH="$BUILD_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

if [ "$RUN_INTERNAL_SMOKE" -eq 1 ]; then
    if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
        echo "==> Building qore-ir-plugin-registry-smoke"
        cmake --build "$BUILD_DIR" --target qore-ir-plugin-registry-smoke
        echo "==> Running qore-ir-plugin-registry-smoke"
        "$BUILD_DIR/qore-ir-plugin-registry-smoke"
    else
        echo "WARN: skipping internal smoke target; no CMakeCache.txt in $BUILD_DIR" >&2
    fi
fi

if [ -n "$MODULE_REF" ]; then
    echo "==> Linting plugin module $MODULE_REF"
    set -- "examples/plugins/qore-plugin-lint"
    [ -n "$MODULE_NAME" ] && set -- "$@" --module-name "$MODULE_NAME"
    [ "$STRICT_LINT" -eq 1 ] && set -- "$@" --require-runtime-symbols --warnings-are-errors
    [ "$REQUIRE_LLVM" -eq 1 ] && set -- "$@" --require-llvm
    set -- "$@" "$MODULE_REF"
    "$QORE_BIN" "$@"
fi

for test_file in $QTESTS; do
    for mode in $EXEC_MODES; do
        echo "==> Running $test_file in $mode mode"
        if [ "$mode" = "ast" ]; then
            QORE_PLUGIN_VERIFY=1 "$QORE_BIN" "$test_file"
        else
            QORE_PLUGIN_VERIFY=1 "$QORE_BIN" "--exec-mode=$mode" --ir-fallback-warn "$test_file"
        fi
    done
done
