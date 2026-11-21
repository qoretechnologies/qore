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
# - <seconds> is the capture's time limit (1 to 300), <mark> has the runner's form (<runner pid>.<test number>), and
#   <tool> names one of the fixed commands below (gdb or eu-stack, with fixed options and gdb's auto-loading off),
#   which root runs under "timeout -s KILL <seconds>";
# - <reply> is the absolute path of the FIFO where the runner reads the answer; it is opened by a process of the
#   runner's user (gosu), never by root, so the request can only reach files that the runner's user could write
#   itself, and no file in the runner's directories is owned by root;
# - <pid> must be a process of the runner's user (its effective user ID, the owner of /proc/<pid>) that carries
#   QORE_TEST_PROCESS_MARK=<mark> in its environment, or that is in the process group of such a process (a test leads
#   its group).  The mark keeps a capture from reaching a process outside the tests by accident (a stale or mistaken
#   request); it is no barrier against a hostile process of the runner's user, which can give itself any mark, but such
#   a process can only have processes of the runner's user captured, which it could debug itself.
# What is guaranteed about the process whose stacks are sent: the tool's output is kept in a file that only root can
# read until these checks have passed, and nothing of it is sent otherwise.
# - Its identity (/proc/<pid>/stat's start time, start of its stack and of its arguments and environment, which every
#   execution of a program places anew, its user ID and its executable) is read first, before every other check, and
#   its user ID must be the runner's; it is read again after the capture and must be unchanged ("process <pid> changed
#   during the capture"), so a process that executed another program, or whose ID another process took, in between is
#   not sent.
# - gdb also prints the process's /proc/<pid>/status while it is stopped for the capture (a stopped process cannot
#   execute a program), and every user ID in it must be the runner's and its group IDs one and the same: the stacks are
#   those of a process of the runner's user even if it executed a set-user-ID program and then its own program again
#   between the checks (which the identity alone would not show).  eu-stack has no such step: with it, a process that
#   did that within the instant of the capture, and placed its stack and arguments where they were, would not be told
#   apart.  The status lines are not sent.
# Root's debugger may still have attached to a process that fails these checks, for the instant of the capture.
# A request that fails the checks is answered with the reason (or not at all, if <reply> is not an absolute path of
# plain characters).  A request that ptrace's access rules refuse (the helper has no CAP_SYS_PTRACE) is answered with
# "refused: ptrace restricted: ...", which run_tests.sh reports once for the test.  Each answer ends with
# "qore-stack-capture-status: <exit code of the tool>", or 126 for a refused request; the runner waits for it with a
# limit of its own, and the helper's own write of the answer is limited once the capture has ended.  At most
# QORE_CI_STACK_CAPTURE_MAX_JOBS requests are answered at once; another one waits until the helper has answered it.
#
# Sourced by test-ubuntu.sh and test-alpine.sh, which run with "set -e": its functions do not fail; run as
# "sh ci-stack-capture.sh serve <user> <directory>" by qore_ci_start_stack_capture().

# the longest capture that a request may ask for (seconds)
QORE_CI_STACK_CAPTURE_MAX_SECS=300
# the requests answered at once
QORE_CI_STACK_CAPTURE_MAX_JOBS=8

# Starts the helper for the given user (the runner's), if this script runs as root, and exports
# QORE_TEST_STACK_CAPTURE_HELPER, the FIFO of its requests, for run_tests.sh; otherwise does nothing.  The helper runs
# in a session of its own, which qore_ci_stop_stack_capture() ends with every capture in it.  Its messages go to a log
# of its own, which qore_ci_stop_stack_capture() prints: the job's output must not be held by a process of the helper.
qore_ci_start_stack_capture() {
    QORE_CI_STACK_CAPTURE_PID=""
    QORE_CI_STACK_CAPTURE_DIR=""
    if [ "$(id -u)" != 0 ] || ! id "$1" > /dev/null 2>&1 || ! command -v setsid > /dev/null 2>&1; then
        echo "-- no stack capture helper: it needs root, the user $1 and setsid --"
        return 0
    fi
    QORE_CI_STACK_CAPTURE_DIR=$(mktemp -d /tmp/qore-stack-capture.XXXXXX) || return 0
    chmod 0711 "${QORE_CI_STACK_CAPTURE_DIR}" || true
    # the outputs of the captures, which only root may read until they are sent
    if ! mkdir -m 0700 "${QORE_CI_STACK_CAPTURE_DIR}/work" \
            || ! mkfifo -m 0620 "${QORE_CI_STACK_CAPTURE_DIR}/requests" \
            || ! chgrp "$(id -g "$1")" "${QORE_CI_STACK_CAPTURE_DIR}/requests"; then
        rm -rf "${QORE_CI_STACK_CAPTURE_DIR}" || true
        QORE_CI_STACK_CAPTURE_DIR=""
        echo "-- no stack capture helper: its directory cannot be made --"
        return 0
    fi
    # setsid in a background command is not a group leader, so it does not fork: its process ID is the helper's, and
    # the ID of its session
    setsid sh "${QORE_SRC_DIR}/test/docker_test/ci-stack-capture.sh" serve "$1" "${QORE_CI_STACK_CAPTURE_DIR}" \
        < /dev/null > "${QORE_CI_STACK_CAPTURE_DIR}/work/helper.log" 2>&1 &
    QORE_CI_STACK_CAPTURE_PID=$!
    QORE_TEST_STACK_CAPTURE_HELPER="${QORE_CI_STACK_CAPTURE_DIR}/requests"
    export QORE_TEST_STACK_CAPTURE_HELPER
    echo "-- stack capture helper ${QORE_CI_STACK_CAPTURE_PID}: ${QORE_TEST_STACK_CAPTURE_HELPER} --"
}

# Prints the IDs of the processes of the given session that have not ended (the 6th field of /proc/<pid>/stat; the
# state is the 3rd, both after the command name in parentheses)
qore_ci_stack_capture_session() {
    awk -v sid="$1" 'BEGIN {
        for (i = 1; i < ARGC; ++i) {
            f = ARGV[i]
            if ((getline line < f) > 0) {
                last = 0
                while ((j = index(substr(line, last + 1), ")")) > 0) {
                    last += j
                }
                split(substr(line, last + 2), field, " ")
                if (field[4] == sid && field[1] != "Z" && field[1] != "X") {
                    pid = f
                    sub(/^\/proc\//, "", pid)
                    sub(/\/stat$/, "", pid)
                    print pid
                }
            }
            close(f)
        }
        exit
    }' /proc/[0-9]*/stat 2>/dev/null
}

# Ends the helper started by qore_ci_start_stack_capture() and every capture it runs, prints its log and removes its
# directory.  Every process of the helper's session is killed, not its process group: timeout (uutils on Ubuntu) moves
# itself into a group of its own.  A process is listed again after the kill until none is left (a capture that forked
# meanwhile), at most 100 times.  A process of the list that ended before its kill, and whose ID another process took
# in that instant, would be killed instead: every process ID would have to be used in between, which the job's
# container does not come near.
qore_ci_stop_stack_capture() {
    if [ -n "${QORE_CI_STACK_CAPTURE_PID}" ]; then
        _qore_ci_sc_rounds=0
        while [ ${_qore_ci_sc_rounds} -lt 100 ]; do
            _qore_ci_sc_left=$(qore_ci_stack_capture_session "${QORE_CI_STACK_CAPTURE_PID}")
            if [ -z "${_qore_ci_sc_left}" ]; then
                break
            fi
            kill -KILL ${_qore_ci_sc_left} 2>/dev/null || true
            _qore_ci_sc_rounds=$((_qore_ci_sc_rounds + 1))
        done
        wait "${QORE_CI_STACK_CAPTURE_PID}" 2>/dev/null || true
        _qore_ci_sc_left=$(qore_ci_stack_capture_session "${QORE_CI_STACK_CAPTURE_PID}")
        if [ -n "${_qore_ci_sc_left}" ]; then
            echo "-- the stack capture helper left processes: ${_qore_ci_sc_left} --"
        fi
        QORE_CI_STACK_CAPTURE_PID=""
    fi
    if [ -n "${QORE_CI_STACK_CAPTURE_DIR}" ]; then
        if [ -s "${QORE_CI_STACK_CAPTURE_DIR}/work/helper.log" ]; then
            echo "-- the stack capture helper's log --"
            cat "${QORE_CI_STACK_CAPTURE_DIR}/work/helper.log" || true
        fi
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

# Prints the fields of /proc/<pid>/stat after the command name (in parentheses, which it may contain): the state
# first; the file is an argument of the limited command, so that its open is limited too
qore_ci_stack_capture_stat() {
    _qore_ci_sc_s=$(timeout -s KILL 5 cat "/proc/$1/stat" 2>/dev/null)
    if [ -n "${_qore_ci_sc_s}" ]; then
        printf '%s\n' "${_qore_ci_sc_s##*) }"
    fi
}

# Returns 0 if the given process has ended (it is gone, or a zombie)
qore_ci_stack_capture_ended() {
    set -- $(qore_ci_stack_capture_stat "$1")
    case "$1" in
        ''|Z|X|x) return 0 ;;
    esac
    return 1
}

# Prints the identity of the given process (see above): its user ID (the owner of /proc/<pid>), the fields of its
# stat that every execution of a program sets anew (22: start time, 28: start of the stack, 48 to 50: start and end of
# the arguments, start of the environment; field N is argument N - 1 below, the state, field 3, being the 2nd) and
# its executable; nothing if it has ended
qore_ci_stack_capture_identity() {
    set -- "$1" $(qore_ci_stack_capture_stat "$1")
    if [ $# -lt 49 ]; then
        return
    fi
    eval "_qore_ci_sc_fields=\"\${21} \${27} \${47} \${48} \${49}\""
    printf '%s %s %s\n' "$(stat -c %u "/proc/$1" 2>/dev/null)" "${_qore_ci_sc_fields}" \
        "$(readlink "/proc/$1/exe" 2>/dev/null)"
}

# Returns 0 if the given gdb output (a file) shows the process, while it was stopped, with every user ID the given one
# and one group ID (the "Uid:" and "Gid:" lines of "info proc status"), or shows no stack at all (an attach that
# failed); prints the output without the status lines
qore_ci_stack_capture_gdb_status() {
    awk -F '\t' -v uid="$2" -v out="$3" '
        /^process [0-9]+$/ { next }
        /^Uid:\t/ { uids = 1; for (i = 2; i <= NF; ++i) if ($i != uid) bad = 1; next }
        /^Gid:\t/ { gids = 1; for (i = 3; i <= NF; ++i) if ($i != $2) bad = 1; next }
        /^[A-Za-z_]+:\t/ { next }
        /^#[0-9]/ { stacks = 1 }
        { print > out }
        END { exit !(!bad && (uids && gids || !stacks)) }' "$1"
}

# Returns 0 if the given process (/proc/<pid>) carries QORE_TEST_PROCESS_MARK=<mark> (the second argument); the read
# of its environment is limited, as it waits for a process that holds its memory map's lock (a fault stuck in D state)
qore_ci_stack_capture_has_mark() {
    timeout -s KILL 5 cat "/proc/$1/environ" 2>/dev/null | tr '\0' '\n' | grep -qxF "QORE_TEST_PROCESS_MARK=$2"
}

# Prints why the given process may not be captured for the given user ID and mark, or nothing if it may (see above)
qore_ci_stack_capture_check() {
    if qore_ci_stack_capture_ended "$1"; then
        echo "process $1 has ended"
        return
    fi
    if [ "$(stat -c %u "/proc/$1" 2>/dev/null)" != "$2" ]; then
        echo "process $1 is not a process of the runner's user"
        return
    fi
    # a process of another user's environment can be read only with ptrace's access (CAP_SYS_PTRACE), which a
    # debugger needs as well: without it, run_tests.sh reports the restriction once, as for a refused attach; a
    # process that ended meanwhile is no restriction
    _qore_ci_sc_err=$(timeout -s KILL 5 head -c 1 "/proc/$1/environ" 2>&1 > /dev/null)
    case $? in
        0) ;;
        124|137)
            echo "the environment of process $1 could not be read within 5s"
            return
            ;;
        *)
            if qore_ci_stack_capture_ended "$1"; then
                echo "process $1 has ended"
                return
            fi
            case "${_qore_ci_sc_err}" in
                *"Permission denied"*|*"Operation not permitted"*)
                    echo "ptrace restricted: the helper may not read the environment of process $1 (it has no" \
                        "CAP_SYS_PTRACE)"
                    ;;
                *) echo "the environment of process $1 cannot be read: ${_qore_ci_sc_err}" ;;
            esac
            return
            ;;
    esac
    if qore_ci_stack_capture_has_mark "$1" "$3"; then
        return
    fi
    # the process group: the 3rd field after the command name
    set -- "$@" $(qore_ci_stack_capture_stat "$1")
    _qore_ci_sc_group=$6
    if qore_ci_stack_capture_number "${_qore_ci_sc_group}" 9 && [ "${_qore_ci_sc_group}" != "$1" ] \
            && [ "$(stat -c %u "/proc/${_qore_ci_sc_group}" 2>/dev/null)" = "$2" ] \
            && qore_ci_stack_capture_has_mark "${_qore_ci_sc_group}" "$3"; then
        return
    fi
    echo "process $1 is not a process of the test with mark $3"
}

# Writes the answer (the file given) to the reply FIFO as the given user (name and ID), for at most 15 seconds: the
# runner may have stopped waiting, and an open of a FIFO without a reader would block
qore_ci_stack_capture_reply() {
    if [ "$(id -u)" = "$2" ]; then
        timeout -s KILL 15 sh -c 'exec cat > "$1"' sh "$3" < "$4"
    else
        timeout -s KILL 15 gosu "$1" sh -c 'exec cat > "$1"' sh "$3" < "$4"
    fi
}

# Answers one request (the user, the user's ID, the work directory and the request's line; see above)
qore_ci_stack_capture_request() {
    _qore_ci_sc_user=$1
    _qore_ci_sc_uid=$2
    _qore_ci_sc_work=$3
    set -f
    set -- $4
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
    # the answer, and the tool's output until it is checked, in files that only root can read
    _qore_ci_sc_answer=$(mktemp "${_qore_ci_sc_work}/answer.XXXXXX") || return
    _qore_ci_sc_output=$(mktemp "${_qore_ci_sc_work}/output.XXXXXX") || { rm -f "${_qore_ci_sc_answer}"; return; }
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
    # the identity first, before the other checks, so that a process that executes another program after it is not sent
    if [ -z "${_qore_ci_sc_why}" ]; then
        _qore_ci_sc_before=$(qore_ci_stack_capture_identity "$1")
        if [ -z "${_qore_ci_sc_before}" ]; then
            _qore_ci_sc_why="process $1 has ended"
        elif [ "${_qore_ci_sc_before%% *}" != "${_qore_ci_sc_uid}" ]; then
            _qore_ci_sc_why="process $1 is not a process of the runner's user"
        fi
    fi
    if [ -z "${_qore_ci_sc_why}" ]; then
        _qore_ci_sc_why=$(qore_ci_stack_capture_check "$1" "${_qore_ci_sc_uid}" "$3")
    fi
    if [ -z "${_qore_ci_sc_why}" ]; then
        case "$4" in
            gdb)
                DEBUGINFOD_URLS= timeout -s KILL "$2" gdb -nx -batch -iex "set auto-load off" \
                    -iex "set debuginfod enabled off" -p "$1" -ex "info proc status" -ex "thread apply all bt" \
                    < /dev/null > "${_qore_ci_sc_output}" 2>&1
                ;;
            eu-stack)
                DEBUGINFOD_URLS= timeout -s KILL "$2" eu-stack -p "$1" < /dev/null > "${_qore_ci_sc_output}" 2>&1
                ;;
        esac
        _qore_ci_sc_rc=$?
        _qore_ci_sc_after=$(qore_ci_stack_capture_identity "$1")
        if qore_ci_stack_capture_ended "$1"; then
            _qore_ci_sc_why="process $1 has ended"
        elif [ "${_qore_ci_sc_before}" != "${_qore_ci_sc_after}" ]; then
            _qore_ci_sc_why="process $1 changed during the capture"
        elif [ "$4" = gdb ] && ! qore_ci_stack_capture_gdb_status "${_qore_ci_sc_output}" "${_qore_ci_sc_uid}" \
                "${_qore_ci_sc_answer}"; then
            _qore_ci_sc_why="process $1 was not a process of the runner's user while it was stopped for the capture"
        else
            if [ "$4" != gdb ]; then
                cat "${_qore_ci_sc_output}" > "${_qore_ci_sc_answer}"
            fi
            echo "qore-stack-capture-status: ${_qore_ci_sc_rc}" >> "${_qore_ci_sc_answer}"
        fi
    fi
    if [ -n "${_qore_ci_sc_why}" ]; then
        printf 'qore-stack-capture: refused: %s\nqore-stack-capture-status: 126\n' "${_qore_ci_sc_why}" \
            > "${_qore_ci_sc_answer}"
    fi
    rm -f "${_qore_ci_sc_output}"
    qore_ci_stack_capture_reply "${_qore_ci_sc_user}" "${_qore_ci_sc_uid}" "$5" "${_qore_ci_sc_answer}"
    rm -f "${_qore_ci_sc_answer}"
}

# Reads the requests from the FIFO in the given directory (the user and the directory), each answered in a process of
# its own, at most QORE_CI_STACK_CAPTURE_MAX_JOBS at once
qore_ci_stack_capture_serve() {
    _qore_ci_sc_uid=$(id -u "$1") || exit 1
    # open for reading and writing, so that the FIFO never ends while the helper runs
    exec 3<> "$2/requests" || exit 1
    _qore_ci_sc_jobs=""
    while IFS= read -r _qore_ci_sc_line <&3; do
        # the requests still being answered
        _qore_ci_sc_running=""
        _qore_ci_sc_count=0
        for _qore_ci_sc_job in ${_qore_ci_sc_jobs}; do
            if kill -0 "${_qore_ci_sc_job}" 2>/dev/null; then
                _qore_ci_sc_running="${_qore_ci_sc_running} ${_qore_ci_sc_job}"
                _qore_ci_sc_count=$((_qore_ci_sc_count + 1))
            fi
        done
        _qore_ci_sc_jobs=${_qore_ci_sc_running}
        if [ ${_qore_ci_sc_count} -ge ${QORE_CI_STACK_CAPTURE_MAX_JOBS} ]; then
            qore_ci_stack_capture_request "$1" "${_qore_ci_sc_uid}" "$2/work" "${_qore_ci_sc_line}"
        else
            qore_ci_stack_capture_request "$1" "${_qore_ci_sc_uid}" "$2/work" "${_qore_ci_sc_line}" &
            _qore_ci_sc_jobs="${_qore_ci_sc_jobs} $!"
        fi
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
