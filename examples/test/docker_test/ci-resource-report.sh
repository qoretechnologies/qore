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
