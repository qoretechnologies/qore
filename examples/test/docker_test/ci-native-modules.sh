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
# A module built for a module API that this qore does not load is left out, as the loader would refuse it.  A module of
# the image that is left out without a build for a module API that this qore loads fails the job, naming the module
# APIs and the modules: the tests that need such a module would be skipped, and the job would pass without them (as
# in pipeline 64250, whose qore loaded module API 2.1 while all of the image's 34 native modules were built for 2.0).
#
# QORE_CI_NATIVE_MODULE_API_TRANSITION=1 lets such a job run: only for a batch that changes the module API, whose
# images cannot have the modules of the new API yet.  The job then names the modules whose tests are skipped, at the
# start and again after the tests (qore_ci_native_module_notice); the pipelines after the images are rebuilt must not
# set it.

# Restores the pathname expansion setting of the caller of qore_ci_native_module_farm
qore_ci_nmf_restore_glob() {
    if [ -n "${qore_ci_nmf_noglob}" ]; then
        set -f
    else
        set +f
    fi
}

# Links the native modules of the given directories that the given qore loads into the given directory
# usage: qore_ci_native_module_farm QORE FARM DIR...
#   QORE: the qore under test, whose --module-apis option gives the module APIs that it loads
#   FARM: the directory to link the modules into
#   DIR...: the directories of the native modules; a module in more than one is linked from the first
# Keeps the caller's IFS and pathname expansion setting.  Prints the modules that are not linked and why, then a
# summary; returns non-zero if the qore does not give a valid list of module APIs.
qore_ci_native_module_farm() {
    qore_ci_nmf_qore=$1
    qore_ci_nmf_farm=$2
    shift 2
    # the caller's pathname expansion setting, restored before returning: the modules are found with pathname
    # expansion, and the list of module APIs is split without it
    case $- in
        *f*) qore_ci_nmf_noglob=1 ;;
        *) qore_ci_nmf_noglob= ;;
    esac
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
        # the list is split at the commas, without pathname expansion; the caller's IFS (also when it is unset) and
        # pathname expansion setting are restored
        qore_ci_nmf_ifs_set=${IFS+set}
        qore_ci_nmf_ifs=$IFS
        set -f
        IFS=,
        # the list with the items separated by ", ", as it is matched below
        qore_ci_nmf_norm=
        for qore_ci_nmf_item in ${qore_ci_nmf_apis}; do
            # without the space that may follow each comma
            qore_ci_nmf_item=${qore_ci_nmf_item# }
            case "${qore_ci_nmf_item}" in
                *[!0-9.]*|.*|*.|*.*.*|*..*)
                    qore_ci_nmf_bad=1
                    ;;
                *.*)
                    qore_ci_nmf_norm="${qore_ci_nmf_norm:+${qore_ci_nmf_norm}, }${qore_ci_nmf_item}"
                    ;;
                *)
                    qore_ci_nmf_bad=1
                    ;;
            esac
        done
        if [ -n "${qore_ci_nmf_ifs_set}" ]; then
            IFS=$qore_ci_nmf_ifs
        else
            unset IFS
        fi
        qore_ci_nmf_restore_glob
    fi
    if [ -n "${qore_ci_nmf_bad}" ]; then
        echo "qore_ci_native_module_farm: ${qore_ci_nmf_qore} --module-apis did not give a list of module APIs" \
            "(<major>.<minor>, ...): '${qore_ci_nmf_apis}'" >&2
        return 1
    fi
    qore_ci_nmf_apis=${qore_ci_nmf_norm}
    echo "-- native modules: this qore loads module API ${qore_ci_nmf_apis} --"
    qore_ci_nmf_linked=0
    set +f
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
    # the modules left out that have no build for a module API that this qore loads, by name (ex: "xml" for
    # xml-api-2.0); a module of another API whose build for this one is exposed is not missing
    qore_ci_nmf_missing=
    # with their files left out (ex: "xml-api-2.0")
    qore_ci_nmf_missing_files=
    # the names are split at the spaces whatever the caller's IFS is, which is restored after the loop
    qore_ci_nmf_ifs_set=${IFS+set}
    qore_ci_nmf_ifs=$IFS
    IFS=' '
    for qore_ci_nmf_base in ${qore_ci_nmf_skipped_names}; do
        qore_ci_nmf_name=${qore_ci_nmf_base%-api-*}
        qore_ci_nmf_exposed=
        for qore_ci_nmf_qmod in "${qore_ci_nmf_farm}/${qore_ci_nmf_name}"-api-*.qmod; do
            # the name of a match is checked, as the pattern also matches the modules of a name with "-api-" in it
            # (ex: my-api-tool-api-2.1 for "my")
            qore_ci_nmf_qmod=$(basename "${qore_ci_nmf_qmod}")
            if [ "${qore_ci_nmf_qmod%-api-*}" = "${qore_ci_nmf_name}" ] \
                && { [ -e "${qore_ci_nmf_farm}/${qore_ci_nmf_qmod}" ] || [ -L "${qore_ci_nmf_farm}/${qore_ci_nmf_qmod}" ]; }; then
                qore_ci_nmf_exposed=1
            fi
        done
        if [ -z "${qore_ci_nmf_exposed}" ]; then
            qore_ci_nmf_missing_files="${qore_ci_nmf_missing_files:+${qore_ci_nmf_missing_files} }${qore_ci_nmf_base}"
            case " ${qore_ci_nmf_missing} " in
                *" ${qore_ci_nmf_name} "*)
                    ;;
                *)
                    qore_ci_nmf_missing="${qore_ci_nmf_missing:+${qore_ci_nmf_missing} }${qore_ci_nmf_name}"
                    ;;
            esac
        fi
    done
    if [ -n "${qore_ci_nmf_ifs_set}" ]; then
        IFS=$qore_ci_nmf_ifs
    else
        unset IFS
    fi
    qore_ci_nmf_restore_glob
    # the coverage of the run: the tests of the modules left out are skipped
    echo "-- native modules: ${qore_ci_nmf_linked} exposed, ${qore_ci_nmf_skipped} left out (built for another" \
        "module API)${qore_ci_nmf_skipped_names:+:${qore_ci_nmf_skipped_names}} --"
    if [ -n "${qore_ci_nmf_missing}" ]; then
        qore_ci_nmf_msg="the image's native modules ${qore_ci_nmf_missing} were built only for another module API"
        qore_ci_nmf_msg="${qore_ci_nmf_msg} (${qore_ci_nmf_missing_files}) than the ${qore_ci_nmf_apis} that this"
        qore_ci_nmf_msg="${qore_ci_nmf_msg} qore loads, so their tests would be skipped"
        if [ "${QORE_CI_NATIVE_MODULE_API_TRANSITION:-}" = "1" ]; then
            qore_ci_native_module_notice_text="QORE_CI_NATIVE_MODULE_API_TRANSITION=1: ${qore_ci_nmf_msg}; this job"
            qore_ci_native_module_notice_text="${qore_ci_native_module_notice_text} does not test them"
            qore_ci_native_module_notice
            return 0
        fi
        echo "qore_ci_native_module_farm: ${qore_ci_nmf_msg}: rebuild the image's native modules for module API" \
            "${qore_ci_nmf_apis}, or set QORE_CI_NATIVE_MODULE_API_TRANSITION=1 in a batch that changes the module" \
            "API (see ci-native-modules.sh)" >&2
        return 1
    fi
    return 0
}

# Prints the notice of a job whose native modules were left out under QORE_CI_NATIVE_MODULE_API_TRANSITION=1, if any:
# the test scripts call it after the tests, so that the job's summary names them
qore_ci_native_module_notice() {
    if [ -n "${qore_ci_native_module_notice_text:-}" ]; then
        echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
        echo "!!! native modules: ${qore_ci_native_module_notice_text}"
        echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
    fi
    return 0
}
