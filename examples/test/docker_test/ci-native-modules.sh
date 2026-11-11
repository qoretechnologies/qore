# -*- mode: sh -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Sourced by the CI test scripts (test-ubuntu.sh, test-alpine.sh, test-macos.sh): the native modules of the image
# that the tests may load.
#
# The tests need native modules from the image (for example yaml, xml and the DBI drivers), but must not see the
# image's user modules, which were compiled by the image's older Qore; native modules carry their module API in their
# file name (<name>-api-<major>.<minor>.qmod), so the scripts expose only those, through a link farm of the run's own.
#
# A module built for a module API that the qore under test does not load is not exposed: the loader refuses it (it
# reports it as not found, naming the file and the module API it was built for), so a test that needs it fails
# instead of skipping, as it does when the module is not installed.  This happens whenever a job changes the module
# API (QORE_MODULE_API_MINOR), until the image is rebuilt with modules built for the new API; while it does, the
# tests of such a module are skipped, as on a host without it.

# Links the native modules of the given directories that the given qore loads into the given directory
# usage: qore_ci_native_module_farm QORE FARM DIR...
#   QORE: the qore under test, whose --module-apis option gives the module APIs that it loads
#   FARM: the directory to link the modules into
#   DIR...: the directories of the native modules; a module in more than one is linked from the first
# Prints the modules that are not linked and why; returns non-zero if the qore does not give its module APIs.
qore_ci_native_module_farm() {
    qore_ci_nmf_qore=$1
    qore_ci_nmf_farm=$2
    shift 2
    # ex: "2.1" or "2.1, 2.0"
    qore_ci_nmf_apis=$("${qore_ci_nmf_qore}" --module-apis) || {
        echo "qore_ci_native_module_farm: ${qore_ci_nmf_qore} --module-apis failed" >&2
        return 1
    }
    if [ -z "${qore_ci_nmf_apis}" ]; then
        echo "qore_ci_native_module_farm: ${qore_ci_nmf_qore} --module-apis gave no module API" >&2
        return 1
    fi
    echo "-- native modules: this qore loads module API ${qore_ci_nmf_apis} --"
    for qore_ci_nmf_dir in "$@"; do
        # "|| continue" and not "&& continue": under "set -e" a trailing test that fails would end the script rather
        # than the iteration
        [ -n "${qore_ci_nmf_dir}" ] || continue
        for qore_ci_nmf_qmod in "${qore_ci_nmf_dir}"/*-api-*.qmod; do
            [ -e "${qore_ci_nmf_qmod}" ] || continue
            qore_ci_nmf_base=$(basename "${qore_ci_nmf_qmod}")
            # the module API: what follows the last "-api-", without ".qmod"
            qore_ci_nmf_api=${qore_ci_nmf_base##*-api-}
            qore_ci_nmf_api=${qore_ci_nmf_api%.qmod}
            case ", ${qore_ci_nmf_apis}, " in
                *", ${qore_ci_nmf_api}, "*)
                    ;;
                *)
                    echo "-- ${qore_ci_nmf_qmod} is not used: it was built for module API ${qore_ci_nmf_api} --"
                    continue
                    ;;
            esac
            # the first directory wins, matching the search order of the directories
            if [ -e "${qore_ci_nmf_farm}/${qore_ci_nmf_base}" ] || [ -L "${qore_ci_nmf_farm}/${qore_ci_nmf_base}" ]; then
                continue
            fi
            ln -s "${qore_ci_nmf_qmod}" "${qore_ci_nmf_farm}/"
        done
    done
    return 0
}
