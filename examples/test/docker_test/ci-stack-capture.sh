# -*- mode: sh -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# The root helper through which run_tests.sh captures the native call stacks of a timed-out test's processes in a CI
# job (qore#6888; see capture_descendant_stacks() in run_tests.sh).  The tests run as the user qore (gosu), and a
# debugger that the runner starts as that user cannot attach to them where yama's ptrace_scope is 1 (only to its own
# descendants without CAP_SYS_PTRACE; the runner's debugger is not an ancestor of the test's processes).  The images
# have no sudo, and gosu cannot raise privileges, so the job's script, which runs as root in the privileged pod,
# starts this helper before it runs the tests as qore (qore_ci_start_stack_capture), and ends it after them
# (qore_ci_stop_stack_capture).  Off CI there is no helper, and run_tests.sh runs the debugger as itself.
#
# Trust boundary: the helper runs as root and reads requests from a FIFO that only root and the runner's group can
# write (mode 0620 in a directory of root's own, mode 0711).  A request is one line, "<pid> <seconds> <mark> <tool>
# <reply>", and nothing in it is ever run or used as a path by root:
# - <pid> must be a process of the runner's user that carries QORE_TEST_PROCESS_MARK=<mark> in its environment, or
#   that is in the process group of such a process (a test leads its group): a process of a test, never root's or
#   another user's, nor any process of the runner's user outside the tests;
# - <seconds> is the capture's time limit (1 to 300), <mark> has the runner's form (<runner pid>.<test number>), and
#   <tool> names one of the fixed commands below (gdb or eu-stack, with fixed options), which root runs under
#   "timeout -s KILL <seconds>";
# - <reply> is the absolute path of the FIFO where the runner reads the answer; it is opened by a process of the
#   runner's user (gosu), never by root, so the request can only reach files that the runner's user could write
#   itself, and no file in the runner's directories is owned by root.
# A request that fails these checks is answered with the reason (or not at all, if <reply> is not an absolute path
# of plain characters).  What remains open: the process ID is checked just before the attach, and a process that took
# the ID of a process of the test in that instant would be attached to, if it is a process of the runner's user with
# the test's mark or in its group.  A request that ptrace's access rules refuse (the helper has no CAP_SYS_PTRACE)
# is answered with "refused: ptrace restricted: ...", which run_tests.sh reports once for the test.  Each answer ends
# with "qore-stack-capture-status: <exit code of the tool>", or 126 for a refused request; the runner waits for it with
# a limit of its own.
#
# Sourced by test-ubuntu.sh and test-alpine.sh, which run with "set -e": its functions do not fail; run as
# "sh ci-stack-capture.sh serve <user> <fifo>" by qore_ci_start_stack_capture().

# the longest capture that a request may ask for (seconds)
QORE_CI_STACK_CAPTURE_MAX_SECS=300

# Starts the helper for the given user (the runner's), if this script runs as root, and exports
# QORE_TEST_STACK_CAPTURE_HELPER, the FIFO of its requests, for run_tests.sh; otherwise does nothing.  The helper runs
# in a session of its own, so qore_ci_stop_stack_capture() ends it with the captures it runs.
qore_ci_start_stack_capture() {
    QORE_CI_STACK_CAPTURE_PID=""
    QORE_CI_STACK_CAPTURE_DIR=""
    if [ "$(id -u)" != 0 ] || ! id "$1" > /dev/null 2>&1 || ! command -v setsid > /dev/null 2>&1; then
        echo "-- no stack capture helper: it needs root, the user $1 and setsid --"
        return 0
    fi
    QORE_CI_STACK_CAPTURE_DIR=$(mktemp -d /tmp/qore-stack-capture.XXXXXX) || return 0
    chmod 0711 "${QORE_CI_STACK_CAPTURE_DIR}" || true
    if ! mkfifo -m 0620 "${QORE_CI_STACK_CAPTURE_DIR}/requests" \
            || ! chgrp "$(id -g "$1")" "${QORE_CI_STACK_CAPTURE_DIR}/requests"; then
        rm -rf "${QORE_CI_STACK_CAPTURE_DIR}"
        QORE_CI_STACK_CAPTURE_DIR=""
        echo "-- no stack capture helper: its FIFO cannot be made --"
        return 0
    fi
    # setsid in a background command is not a group leader, so it does not fork: its process ID is the helper's
    setsid sh "${QORE_SRC_DIR}/test/docker_test/ci-stack-capture.sh" serve "$1" \
        "${QORE_CI_STACK_CAPTURE_DIR}/requests" < /dev/null > /dev/null &
    QORE_CI_STACK_CAPTURE_PID=$!
    QORE_TEST_STACK_CAPTURE_HELPER="${QORE_CI_STACK_CAPTURE_DIR}/requests"
    export QORE_TEST_STACK_CAPTURE_HELPER
    echo "-- stack capture helper ${QORE_CI_STACK_CAPTURE_PID}: ${QORE_TEST_STACK_CAPTURE_HELPER} --"
}

# Ends the helper started by qore_ci_start_stack_capture() and every capture it runs, and removes its FIFO
qore_ci_stop_stack_capture() {
    if [ -n "${QORE_CI_STACK_CAPTURE_PID}" ]; then
        kill -KILL -"${QORE_CI_STACK_CAPTURE_PID}" 2>/dev/null || true
        wait "${QORE_CI_STACK_CAPTURE_PID}" 2>/dev/null || true
        QORE_CI_STACK_CAPTURE_PID=""
    fi
    if [ -n "${QORE_CI_STACK_CAPTURE_DIR}" ]; then
        rm -rf "${QORE_CI_STACK_CAPTURE_DIR}" || true
        QORE_CI_STACK_CAPTURE_DIR=""
    fi
    unset QORE_TEST_STACK_CAPTURE_HELPER
    return 0
}

# Returns 0 if the given argument is a whole number of 1 to the given number of digits, without leading zeros
qore_ci_stack_capture_number() {
    case "$1" in
        ''|0*|*[!0-9]*) return 1 ;;
    esac
    [ ${#1} -le $2 ]
}

# Returns 0 if the given process (/proc/<pid>) carries QORE_TEST_PROCESS_MARK=<mark> (the second argument); the read
# of its environment is limited, as it waits for a process that holds its memory map's lock (a fault stuck in D state)
qore_ci_stack_capture_has_mark() {
    timeout -s KILL 5 tr '\0' '\n' 2>/dev/null < "/proc/$1/environ" | grep -qxF "QORE_TEST_PROCESS_MARK=$2"
}

# Prints why the given process may not be captured for the given user ID and mark, or nothing if it may (see above)
qore_ci_stack_capture_check() {
    if [ ! -d "/proc/$1" ] || [ "$(stat -c %u "/proc/$1" 2>/dev/null)" != "$2" ]; then
        echo "process $1 is not a process of the runner's user"
        return
    fi
    # a process of another user's environment can be read only with ptrace's access (CAP_SYS_PTRACE), which a
    # debugger needs as well: without it, run_tests.sh reports the restriction once, as for a refused attach
    timeout -s KILL 5 head -c 1 2>/dev/null < "/proc/$1/environ" > /dev/null
    case $? in
        0) ;;
        124|137)
            echo "the environment of process $1 could not be read within 5s"
            return
            ;;
        *)
            echo "ptrace restricted: the helper may not read the environment of process $1 (it has no CAP_SYS_PTRACE)"
            return
            ;;
    esac
    if qore_ci_stack_capture_has_mark "$1" "$3"; then
        return
    fi
    # the process group: the 5th field of /proc/<pid>/stat, after the command name in parentheses
    _qore_ci_sc_stat=$(timeout -s KILL 5 cat 2>/dev/null < "/proc/$1/stat")
    _qore_ci_sc_stat=${_qore_ci_sc_stat##*) }
    set -- "$@" $_qore_ci_sc_stat
    _qore_ci_sc_group=$6
    if qore_ci_stack_capture_number "${_qore_ci_sc_group}" 9 && [ "${_qore_ci_sc_group}" != "$1" ] \
            && [ "$(stat -c %u "/proc/${_qore_ci_sc_group}" 2>/dev/null)" = "$2" ] \
            && qore_ci_stack_capture_has_mark "${_qore_ci_sc_group}" "$3"; then
        return
    fi
    echo "process $1 is not a process of the test with mark $3"
}

# Answers one request (the user, the user's ID and the request's line; see above)
qore_ci_stack_capture_request() {
    _qore_ci_sc_user=$1
    _qore_ci_sc_uid=$2
    set -f
    set -- $3
    set +f
    if [ $# -ne 5 ]; then
        echo "ci-stack-capture: a request that is not five fields is ignored" >&2
        return
    fi
    case "$5" in
        /*) ;;
        *)
            echo "ci-stack-capture: a request whose reply is not an absolute path is ignored" >&2
            return
            ;;
    esac
    case "$5" in
        *[!A-Za-z0-9._/-]*|*/../*|*/..)
            echo "ci-stack-capture: a request whose reply has other characters is ignored" >&2
            return
            ;;
    esac
    _qore_ci_sc_reply=$5
    # the reply waits for the capture's time limit and then some, or a short time if that is not valid
    _qore_ci_sc_reply_secs=10
    if qore_ci_stack_capture_number "$2" 3 && [ "$2" -le ${QORE_CI_STACK_CAPTURE_MAX_SECS} ]; then
        _qore_ci_sc_reply_secs=$(($2 + 10))
    fi
    {
        _qore_ci_sc_why=""
        if ! qore_ci_stack_capture_number "$1" 9; then
            _qore_ci_sc_why="the process ID is not a number"
        elif ! qore_ci_stack_capture_number "$2" 3 || [ "$2" -gt ${QORE_CI_STACK_CAPTURE_MAX_SECS} ]; then
            _qore_ci_sc_why="the time limit is not 1 to ${QORE_CI_STACK_CAPTURE_MAX_SECS} seconds"
        elif ! expr "x$3" : 'x[1-9][0-9]\{0,8\}\.[0-9]\{1,9\}$' > /dev/null; then
            _qore_ci_sc_why="the mark is not a test's"
        else
            case "$4" in
                gdb|eu-stack)
                    if ! command -v "$4" > /dev/null 2>&1; then
                        _qore_ci_sc_why="$4 is not installed"
                    fi
                    ;;
                *) _qore_ci_sc_why="the tool is neither gdb nor eu-stack" ;;
            esac
        fi
        if [ -z "${_qore_ci_sc_why}" ]; then
            _qore_ci_sc_why=$(qore_ci_stack_capture_check "$1" "${_qore_ci_sc_uid}" "$3")
        fi
        if [ -n "${_qore_ci_sc_why}" ]; then
            echo "qore-stack-capture: refused: ${_qore_ci_sc_why}"
            echo "qore-stack-capture-status: 126"
        else
            case "$4" in
                gdb)
                    DEBUGINFOD_URLS= timeout -s KILL "$2" gdb -nx -batch -iex "set debuginfod enabled off" -p "$1" \
                        -ex "thread apply all bt" < /dev/null
                    ;;
                eu-stack) DEBUGINFOD_URLS= timeout -s KILL "$2" eu-stack -p "$1" < /dev/null ;;
            esac
            echo "qore-stack-capture-status: $?"
        fi
    } 2>&1 | qore_ci_stack_capture_reply "${_qore_ci_sc_user}" "${_qore_ci_sc_uid}" "${_qore_ci_sc_reply}" \
        "${_qore_ci_sc_reply_secs}"
}

# Copies its input to the reply FIFO (the third argument) as the given user (name and ID), for at most the given
# number of seconds: the runner may have stopped waiting, and an open of a FIFO without a reader would block
qore_ci_stack_capture_reply() {
    if [ "$(id -u)" = "$2" ]; then
        timeout -s KILL "$4" sh -c 'exec cat > "$1"' sh "$3"
    else
        timeout -s KILL "$4" gosu "$1" sh -c 'exec cat > "$1"' sh "$3"
    fi
}

# Reads the requests from the FIFO (the user and the FIFO), each answered in a process of its own
qore_ci_stack_capture_serve() {
    _qore_ci_sc_uid=$(id -u "$1") || exit 1
    # open for reading and writing, so that the FIFO never ends while the helper runs
    exec 3<> "$2" || exit 1
    while IFS= read -r _qore_ci_sc_line <&3; do
        qore_ci_stack_capture_request "$1" "${_qore_ci_sc_uid}" "${_qore_ci_sc_line}" &
    done
}

# run, not sourced (a sourcing script's $0 is its own)
case "$0" in
    *ci-stack-capture.sh)
        if [ "$1" = serve ] && [ $# -eq 3 ]; then
            qore_ci_stack_capture_serve "$2" "$3"
        fi
        ;;
esac
