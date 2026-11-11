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
# A module built for a module API that this qore does not load is left out, as the loader would refuse it: after a
# module API change, the tests that need such a module are skipped until the image is rebuilt, and the summary line in
# the job log names the modules left out.

# Links the native modules of the given directories that the given qore loads into the given directory
# usage: qore_ci_native_module_farm QORE FARM DIR...
#   QORE: the qore under test, whose --module-apis option gives the module APIs that it loads
#   FARM: the directory to link the modules into
#   DIR...: the directories of the native modules; a module in more than one is linked from the first
# Prints the modules that are not linked and why, then a summary; returns non-zero if the qore does not give a valid
# list of module APIs.
qore_ci_native_module_farm() {
    qore_ci_nmf_qore=$1
    qore_ci_nmf_farm=$2
    shift 2
    # ex: "2.1" or "2.1, 2.0"
    qore_ci_nmf_apis=$("${qore_ci_nmf_qore}" --module-apis) || {
        echo "qore_ci_native_module_farm: ${qore_ci_nmf_qore} --module-apis failed" >&2
        return 1
    }
    # every comma-separated item is <major>.<minor>; anything else (an empty output, a warning or a banner before or
    # instead of the list) is an error, as the modules could not be selected reliably
    qore_ci_nmf_bad=
    case "${qore_ci_nmf_apis}" in
        ""|*"
"*|,*|*,|*", ")
            qore_ci_nmf_bad=1
            ;;
    esac
    if [ -z "${qore_ci_nmf_bad}" ]; then
        qore_ci_nmf_ifs=$IFS
        IFS=,
        for qore_ci_nmf_item in ${qore_ci_nmf_apis}; do
            # without the space that follows each comma
            qore_ci_nmf_item=${qore_ci_nmf_item# }
            case "${qore_ci_nmf_item}" in
                *[!0-9.]*|.*|*.|*.*.*|*..*)
                    qore_ci_nmf_bad=1
                    ;;
                *.*)
                    ;;
                *)
                    qore_ci_nmf_bad=1
                    ;;
            esac
        done
        IFS=$qore_ci_nmf_ifs
    fi
    if [ -n "${qore_ci_nmf_bad}" ]; then
        echo "qore_ci_native_module_farm: ${qore_ci_nmf_qore} --module-apis did not give a list of module APIs" \
            "(<major>.<minor>, ...): '${qore_ci_nmf_apis}'" >&2
        return 1
    fi
    echo "-- native modules: this qore loads module API ${qore_ci_nmf_apis} --"
    qore_ci_nmf_linked=0
    qore_ci_nmf_skipped=0
    qore_ci_nmf_skipped_names=
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
                    qore_ci_nmf_skipped=$((qore_ci_nmf_skipped + 1))
                    qore_ci_nmf_skipped_names="${qore_ci_nmf_skipped_names} ${qore_ci_nmf_base%.qmod}"
                    continue
                    ;;
            esac
            # the first directory wins, matching the search order of the directories
            if [ -e "${qore_ci_nmf_farm}/${qore_ci_nmf_base}" ] || [ -L "${qore_ci_nmf_farm}/${qore_ci_nmf_base}" ]; then
                continue
            fi
            ln -s "${qore_ci_nmf_qmod}" "${qore_ci_nmf_farm}/"
            qore_ci_nmf_linked=$((qore_ci_nmf_linked + 1))
        done
    done
    # the coverage of the run: the tests of the modules left out are skipped
    echo "-- native modules: ${qore_ci_nmf_linked} exposed, ${qore_ci_nmf_skipped} left out (built for another" \
        "module API)${qore_ci_nmf_skipped_names:+:${qore_ci_nmf_skipped_names}} --"
    return 0
}
