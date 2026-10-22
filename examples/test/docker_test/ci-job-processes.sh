# -*- mode: sh -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Sourced by the Linux CI scripts: no process a CI job starts may outlive the job's script.  The runner ends a job
# when its output is closed, so a process that still holds the output, such as a server a test started and never
# stopped, makes the job wait for it until the job's time limit: job 213206 waited 9 minutes after its last test and
# then failed on its time limit.  run_tests.sh fails a test that leaves processes running; this also ends the
# processes the script started otherwise, and fails the job loudly when there are any.  run_tests.sh sources it as
# well, for qore_ci_running_processes().

# Prints the state of each process ID given that is still running, one line per process (Linux only):
# - "watcher <pid>": the watcher that BusyBox timeout leaves while it runs its command, which checks once a second
#   whether the command is still running, so it can still be there for up to a second after the command ended; the
#   caller ends it without counting it (the command, if it is still running, is listed itself)
# - "running <pid> <command line>": any other process
# Nothing is printed for a process that has ended: one that was listed but ended before it was examined here, such
# as a watcher whose command had ended, is not running, whether it is gone or a zombie that its parent has not reaped
# yet (an orphaned watcher waits for the process with PID 1 to reap it).  The executable is read before the command
# line and the state, so a process that ends in between is not taken for a process other than a watcher.  /proc is
# ${QORE_CI_PROC} if set.
qore_ci_running_processes() {
    _qore_ci_proc=${QORE_CI_PROC:-/proc}
    for _qore_ci_rp in "$@"; do
        # the executable of a process of another user cannot be read
        _qore_ci_rp_exe=$(readlink "${_qore_ci_proc}/${_qore_ci_rp}/exe" 2>/dev/null) || _qore_ci_rp_exe=""
        if ! _qore_ci_rp_cmd=$(tr '\0' ' ' 2>/dev/null < "${_qore_ci_proc}/${_qore_ci_rp}/cmdline"); then
            continue
        fi
        # the state follows the command name, which is in parentheses and can contain spaces and parentheses itself;
        # a zombie (Z) or dead (X) process has ended
        if ! _qore_ci_rp_stat=$(cat 2>/dev/null < "${_qore_ci_proc}/${_qore_ci_rp}/stat"); then
            continue
        fi
        _qore_ci_rp_state=${_qore_ci_rp_stat##*) }
        case "${_qore_ci_rp_state%% *}" in
            Z|X|x) continue ;;
        esac
        case "${_qore_ci_rp_exe}" in
            */busybox)
                _qore_ci_rp_arg0=${_qore_ci_rp_cmd%% *}
                if [ "${_qore_ci_rp_arg0##*/}" = timeout ]; then
                    echo "watcher ${_qore_ci_rp}"
                    continue
                fi
                ;;
        esac
        echo "running ${_qore_ci_rp} ${_qore_ci_rp_cmd}"
    done
}

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
    # a BusyBox timeout watcher is ended without failing the job, and a process that has ended meanwhile is not
    # running (see qore_ci_running_processes())
    _qore_ci_job_rest=""
    _qore_ci_job_lines=""
    _qore_ci_job_states=""
    if [ -n "${_qore_ci_job_left}" ]; then
        _qore_ci_job_states=$(qore_ci_running_processes ${_qore_ci_job_left})
    fi
    while read -r _qore_ci_kind _qore_ci_pid _qore_ci_cmd; do
        case "${_qore_ci_kind}" in
            watcher)
                kill -KILL ${_qore_ci_pid} 2>/dev/null || true
                ;;
            running)
                _qore_ci_job_rest="${_qore_ci_job_rest} ${_qore_ci_pid}"
                _qore_ci_job_lines="${_qore_ci_job_lines}  pid ${_qore_ci_pid}: ${_qore_ci_cmd}
"
                ;;
        esac
    done <<EOF
${_qore_ci_job_states}
EOF
    _qore_ci_job_left=${_qore_ci_job_rest}
    if [ -z "${_qore_ci_job_left}" ]; then
        echo "-- no process the job started is still running --"
        return 0
    fi
    echo "*** ERROR: processes the job started are still running; they are killed: ***"
    printf '%s' "${_qore_ci_job_lines}"
    kill -KILL ${_qore_ci_job_left} 2>/dev/null || true
    return 1
}
