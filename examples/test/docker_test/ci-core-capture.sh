# -*- mode: sh -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Sourced by the Linux CI scripts, which run as root: core dump capture that every job on a node agrees on.
#
# kernel.core_pattern (and fs.suid_dumpable) is one setting for the whole node, not one per container: every CI pod
# scheduled on the node shares it, and the last job to set it decides where the cores of every pod's processes go.
# The kernel writes a core at the pattern's path as the crashing process sees it, in that process's own pod, and
# drops it when the directory does not exist there.  The qore jobs used to set a path in their checkout
# (/builds/mirror/qore/crash-dumps), which the pods of other projects do not have, so a job on the same node lost
# its cores whenever a qore job had set the pattern last (a Qorus job lost a qorus-core core that way; see Qorus
# test/docker_test/launch_qorus_k8s.sh), and a qore job lost its own when another job had.
#
# Every job therefore sets the same pattern, which names /tmp, a directory every container has: setting it again
# changes nothing for any other job, and each pod finds the cores of its own processes in its own /tmp.

# the pattern every CI job sets
QORE_CI_CORE_PATTERN="/tmp/core.%e.%p.%t"

# Sets the node's core pattern to the one every CI job sets, when the container may set it (a privileged pod)
qore_ci_setup_core_capture() {
    # suid_dumpable=2 writes cores to the core_pattern location securely (needed for gosu)
    if [ -f /proc/sys/fs/suid_dumpable ]; then
        echo 2 > /proc/sys/fs/suid_dumpable 2>/dev/null || true
    fi
    # core capture is diagnostic: a pattern that cannot be set does not end the job
    if [ -w /proc/sys/kernel/core_pattern ]; then
        echo "${QORE_CI_CORE_PATTERN}" > /proc/sys/kernel/core_pattern 2>/dev/null || true
    elif command -v sysctl > /dev/null 2>&1; then
        sysctl -w "kernel.core_pattern=${QORE_CI_CORE_PATTERN}" 2>/dev/null || true
    fi
    echo "kernel.core_pattern: $(cat /proc/sys/kernel/core_pattern 2>/dev/null)"
}

# Moves the cores left in /tmp to the given directory (the job's crash-dumps/ artifact): run_tests.sh moves the core
# of a test that crashed, but not those of the processes a test started
qore_ci_collect_cores() {
    for core in /tmp/core.*; do
        [ -f "${core}" ] || continue
        echo "core dump kept with the job artifacts: ${core}"
        mv "${core}" "$1/" || true
    done
}
