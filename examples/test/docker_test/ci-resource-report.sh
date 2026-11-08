# -*- mode: sh -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Sourced by the CI test scripts: reports the CPUs and the memory that the job may use, and how much memory it has
# used, so that the number of tests run at the same time (QORE_TEST_JOBS, see run_tests.sh) can be chosen from
# measurements: a Linux job reports its cgroup's limits (cpu.max, memory.max) and, at the end, the peak of its memory
# use (memory.peak); a macOS job reports the machine's CPUs and memory.  It only reads; a value that cannot be read is
# reported as unknown.

# Prints the value of a file, or "unknown"
qore_ci_read_value() {
    if [ -r "$1" ]; then
        tr '\n' ' ' < "$1" | sed 's/ *$//'
    else
        echo unknown
    fi
}

# Prints the resources of the job, with a label (ex: "before the tests"), without the shell's trace (set -x) of the
# commands that read them
qore_ci_resource_report() {
    case $- in
        *x*) set +x; qore_ci_resource_report_values "$1"; set -x ;;
        *) qore_ci_resource_report_values "$1" ;;
    esac
    return 0
}

qore_ci_resource_report_values() {
    echo "=== CI resources ($1) ==="
    if [ "$(uname -s)" = Darwin ]; then
        echo "cpus=$(sysctl -n hw.ncpu 2>/dev/null || echo unknown)"
        echo "memory_bytes=$(sysctl -n hw.memsize 2>/dev/null || echo unknown)"
        # the free and the compressed pages
        vm_stat 2>/dev/null | grep -E '^(Pages free|Pages occupied by compressor|Swapouts)' || true
        return 0
    fi
    echo "cpus_online=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo unknown)"
    echo "nproc=$(nproc 2>/dev/null || echo unknown)"
    echo "mem_total_kb=$(awk '/^MemTotal:/ { print $2 }' /proc/meminfo 2>/dev/null || echo unknown)"
    echo "loadavg=$(qore_ci_read_value /proc/loadavg)"
    if [ -r /sys/fs/cgroup/cgroup.controllers ]; then
        # cgroup v2: the job's container
        for f in cpu.max cpu.weight memory.max memory.high memory.current memory.peak memory.swap.max; do
            echo "cgroup.$f=$(qore_ci_read_value /sys/fs/cgroup/$f)"
        done
        echo "cgroup.cpu.stat=$(qore_ci_read_value /sys/fs/cgroup/cpu.stat)"
        echo "cgroup.memory.events=$(qore_ci_read_value /sys/fs/cgroup/memory.events)"
    else
        # cgroup v1
        echo "cgroup.cpu.cfs_quota_us=$(qore_ci_read_value /sys/fs/cgroup/cpu/cpu.cfs_quota_us)"
        echo "cgroup.cpu.cfs_period_us=$(qore_ci_read_value /sys/fs/cgroup/cpu/cpu.cfs_period_us)"
        echo "cgroup.memory.limit_in_bytes=$(qore_ci_read_value /sys/fs/cgroup/memory/memory.limit_in_bytes)"
        echo "cgroup.memory.usage_in_bytes=$(qore_ci_read_value /sys/fs/cgroup/memory/memory.usage_in_bytes)"
        echo "cgroup.memory.max_usage_in_bytes=$(qore_ci_read_value /sys/fs/cgroup/memory/memory.max_usage_in_bytes)"
    fi
    return 0
}

# The number of tests a Linux CI job runs at a time (QORE_TEST_JOBS, see run_tests.sh), from the resources of its pod:
# one test per CPU the pod may use (cgroup cpu.max, or cfs_quota_us / cfs_period_us of cgroup v1, rounded up; the
# CPUs online without a limit), at most one per QORE_CI_TEST_JOB_MEMORY_MB of the pod's memory limit (1536 MB by
# default: a debug-build test with the binaries and the services it starts), and at most QORE_CI_MAX_TEST_JOBS (4 by
# default), as a pod without limits shares its node with the pods of other jobs.  The arguments (for the tests of this
# function): the number of CPUs online and the cgroup directory (/sys/fs/cgroup by default).
qore_ci_test_jobs() {
    _cpus=${1:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 1)}
    _cgroup=${2:-/sys/fs/cgroup}
    _memory_mb=${QORE_CI_TEST_JOB_MEMORY_MB:-1536}
    _max=${QORE_CI_MAX_TEST_JOBS:-4}
    _quota=""
    _period=""
    _limit=""
    # a file a pod does not have is no limit; the callers run under set -e, where a failing read or command
    # substitution in an assignment would end the job's script
    if [ -r "$_cgroup/cpu.max" ]; then
        read -r _quota _period < "$_cgroup/cpu.max" || _quota=""
        _limit=$(cat "$_cgroup/memory.max" 2>/dev/null) || _limit=""
    else
        _quota=$(cat "$_cgroup/cpu/cpu.cfs_quota_us" 2>/dev/null) || _quota=""
        _period=$(cat "$_cgroup/cpu/cpu.cfs_period_us" 2>/dev/null) || _period=""
        _limit=$(cat "$_cgroup/memory/memory.limit_in_bytes" 2>/dev/null) || _limit=""
    fi
    awk -v cpus="$_cpus" -v quota="$_quota" -v period="$_period" -v limit="$_limit" -v per="$_memory_mb" \
            -v max="$_max" 'BEGIN {
        jobs = cpus + 0
        # a quota of "max" or -1 is no limit
        if (quota ~ /^[0-9]+$/ && period ~ /^[0-9]+$/ && period > 0) {
            q = int((quota + period - 1) / period)
            if (q < jobs) {
                jobs = q
            }
        }
        # a limit of "max", or the huge number cgroup v1 reports without one, is no limit
        if (limit ~ /^[0-9]+$/ && per > 0 && limit < 2 ^ 60) {
            m = int(limit / (per * 1048576))
            if (m < jobs) {
                jobs = m
            }
        }
        if (max > 0 && jobs > max) {
            jobs = max
        }
        if (jobs < 1) {
            jobs = 1
        }
        print jobs
    }'
}
