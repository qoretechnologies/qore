# -*- mode: sh -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Sourced by the Linux CI scripts: no process a CI job starts may outlive the job's script.  The runner ends a job
# when its output is closed, so a process that still holds the output, such as a server a test started and never
# stopped, makes the job wait for it until the job's time limit: job 213206 waited 9 minutes after its last test and
# then failed on its time limit.  run_tests.sh fails a test that leaves processes running; this also ends the
# processes the script started otherwise, and fails the job loudly when there are any.

# Marks the processes the job starts from now on: each one, and each one they start, inherits QORE_CI_JOB_MARK in
# its environment, also when it leaves the job's session or is adopted by another parent
qore_ci_mark_job_processes() {
    QORE_CI_JOB_MARK="qore-ci-job.$$"
    export QORE_CI_JOB_MARK
}

# Lists and kills the processes the job started that are still running (Linux only): those with the job's mark, and
# any other that holds the script's output (a pipe) as its standard output or error, which keeps the job running even
# when it was started with an environment of its own; the script and the processes that started it hold the output
# as they should.  Returns 1 if there were any.
qore_ci_end_job_processes() {
    _qore_ci_job_mark="QORE_CI_JOB_MARK=${QORE_CI_JOB_MARK}"
    # the commands run below must not have the mark
    unset QORE_CI_JOB_MARK
    if [ ! -r /proc/self/environ ]; then
        return 0
    fi
    _qore_ci_job_pipes=""
    for _qore_ci_fd in 1 2; do
        _qore_ci_target=$(readlink /proc/$$/fd/${_qore_ci_fd} 2>/dev/null)
        case "${_qore_ci_target}" in
            pipe:*) _qore_ci_job_pipes="${_qore_ci_job_pipes} ${_qore_ci_target}" ;;
        esac
    done
    _qore_ci_job_own=$$
    _qore_ci_pid=$$
    while [ "${_qore_ci_pid}" -gt 1 ]; do
        _qore_ci_pid=$(awk '/^PPid:/ { print $2 }' /proc/${_qore_ci_pid}/status 2>/dev/null)
        if [ -z "${_qore_ci_pid}" ]; then
            break
        fi
        _qore_ci_job_own="${_qore_ci_job_own} ${_qore_ci_pid}"
    done
    _qore_ci_job_left=""
    if [ -n "${_qore_ci_job_pipes}" ]; then
        # the commands that look for them must not hold the output themselves
        _qore_ci_job_left=$({ ls -l /proc/[0-9]*/fd/1 /proc/[0-9]*/fd/2 | awk -v pipes="${_qore_ci_job_pipes}" \
            -v own="${_qore_ci_job_own}" 'BEGIN {
                n = split(pipes, list, " ")
                for (i = 1; i <= n; ++i) {
                    pipe[list[i]] = 1
                }
                n = split(own, list, " ")
                for (i = 1; i <= n; ++i) {
                    skip[list[i]] = 1
                }
            }
            $(NF - 1) == "->" && ($NF in pipe) {
                split($(NF - 2), path, "/")
                if (!(path[3] in skip) && !(path[3] in seen)) {
                    seen[path[3]] = 1
                    printf "%s ", path[3]
                }
            }'; } 2>/dev/null)
    fi
    _qore_ci_job_left="${_qore_ci_job_left}"$(awk -v mark="${_qore_ci_job_mark}" 'BEGIN {
        RS = "\0"
        for (i = 1; i < ARGC; ++i) {
            f = ARGV[i]
            while ((getline e < f) > 0) {
                if (e == mark) {
                    sub(/^\/proc\//, "", f)
                    sub(/\/environ$/, "", f)
                    printf "%s ", f
                    break
                }
            }
            close(f)
        }
        exit
    }' /proc/[0-9]*/environ)
    if [ -n "${_qore_ci_job_left}" ]; then
        _qore_ci_job_left=$(printf '%s\n' ${_qore_ci_job_left} | sort -un | tr '\n' ' ')
    fi
    # the watcher BusyBox timeout leaves while it runs its command checks once a second whether the command is still
    # running, so it can still be there for up to a second after the command ended: one is ended without failing the
    # job (the command, if it is still running, is listed itself)
    _qore_ci_job_rest=""
    for pid in ${_qore_ci_job_left}; do
        case "$(readlink /proc/${pid}/exe 2>/dev/null)" in
            */busybox)
                _qore_ci_arg0=$(tr '\0' '\n' < /proc/${pid}/cmdline 2>/dev/null | head -1)
                if [ "${_qore_ci_arg0##*/}" = timeout ]; then
                    kill -KILL ${pid} 2>/dev/null || true
                    continue
                fi
                ;;
        esac
        _qore_ci_job_rest="${_qore_ci_job_rest} ${pid}"
    done
    _qore_ci_job_left=${_qore_ci_job_rest}
    if [ -z "${_qore_ci_job_left}" ]; then
        echo "-- no process the job started is still running --"
        return 0
    fi
    echo "*** ERROR: processes the job started are still running; they are killed: ***"
    for pid in ${_qore_ci_job_left}; do
        echo "  pid ${pid}: $(tr '\0' ' ' < /proc/${pid}/cmdline 2>/dev/null)"
    done
    kill -KILL ${_qore_ci_job_left} 2>/dev/null || true
    return 1
}
