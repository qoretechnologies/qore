/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAOTBatchMemory.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#include <qore/Qore.h>
#include "qore/intern/QoreLibIntern.h"
#include "qore/intern/QoreAOTBatchMemory.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <libproc.h>
#include <mach/mach.h>
#include <sys/proc_info.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#elif defined(__FreeBSD__) || defined(__DragonFly__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#if defined(__linux__)
// The memory sources read files with open/read/close into stack buffers: they run between the shared parse and the
// codegen workers and at each admission, and need no heap.

//! Reads up to size - 1 bytes of a file into buf and terminates them; returns the count, or -1
static ssize_t qore_aot_mem_read_file(const char* path, char* buf, size_t size) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }
    size_t total = 0;
    while (total < size - 1) {
        ssize_t n = read(fd, buf + total, size - 1 - total);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            return -1;
        }
        if (!n) {
            break;
        }
        total += static_cast<size_t>(n);
    }
    close(fd);
    buf[total] = '\0';
    return static_cast<ssize_t>(total);
}

//! Parses a leading unsigned decimal number after optional blanks; false if there is none (ex: "max")
static bool qore_aot_mem_parse_uint(const char* p, uint64_t& out) {
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
    if (*p < '0' || *p > '9') {
        return false;
    }
    uint64_t v = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + static_cast<uint64_t>(*p - '0');
        ++p;
    }
    out = v;
    return true;
}

//! Finds the value of a "key value" line (as in memory.stat or /proc/meminfo, where the key ends with ':')
static bool qore_aot_mem_find_key(const char* buf, const char* key, uint64_t& out) {
    size_t len = strlen(key);
    const char* p = buf;
    while (*p) {
        if (!strncmp(p, key, len) && (p[len] == ' ' || p[len] == '\t')) {
            return qore_aot_mem_parse_uint(p + len, out);
        }
        const char* nl = strchr(p, '\n');
        if (!nl) {
            break;
        }
        p = nl + 1;
    }
    return false;
}

//! Reads a file holding one number; false if it cannot be read or holds no number (ex: "max")
static bool qore_aot_mem_read_uint_file(const char* path, uint64_t& out) {
    char buf[64];
    if (qore_aot_mem_read_file(path, buf, sizeof buf) <= 0) {
        return false;
    }
    return qore_aot_mem_parse_uint(buf, out);
}

//! Finds this process's cgroup path in /proc/self/cgroup
/** @param v2 the unified hierarchy ("0::<path>"), otherwise the v1 hierarchy of the memory controller
*/
static bool qore_aot_mem_self_cgroup(bool v2, char* out, size_t out_size) {
    char buf[4096];
    if (qore_aot_mem_read_file("/proc/self/cgroup", buf, sizeof buf) <= 0) {
        return false;
    }
    // lines are "hierarchy-id:controllers:path"
    for (char* line = buf; line && *line; ) {
        char* nl = strchr(line, '\n');
        if (nl) {
            *nl = '\0';
        }
        char* c1 = strchr(line, ':');
        char* c2 = c1 ? strchr(c1 + 1, ':') : nullptr;
        if (c1 && c2) {
            bool match;
            if (v2) {
                match = (c1 - line == 1) && line[0] == '0' && c2 == c1 + 1;
            } else {
                *c2 = '\0';
                match = false;
                // the controller list is comma-separated
                for (char* tok = c1 + 1; tok && *tok; ) {
                    char* comma = strchr(tok, ',');
                    size_t len = comma ? static_cast<size_t>(comma - tok) : strlen(tok);
                    if (len == 6 && !strncmp(tok, "memory", 6)) {
                        match = true;
                        break;
                    }
                    tok = comma ? comma + 1 : nullptr;
                }
                *c2 = ':';
            }
            if (match) {
                size_t len = strlen(c2 + 1);
                if (!len || len >= out_size) {
                    return false;
                }
                memcpy(out, c2 + 1, len + 1);
                return true;
            }
        }
        line = nl ? nl + 1 : nullptr;
    }
    return false;
}

//! The binding limit of this process's cgroups
struct QoreAOTCgroupMemory {
    //! the bytes that can still be allocated under the binding limit
    uint64_t headroom = 0;
    //! the binding limit
    uint64_t limit = 0;
    //! the cgroup whose limit binds
    char path[512] = {};
    bool found = false;
};

//! Reads the limit, the usage and the inactive file cache of a cgroup level; false if it has no limit
/** The kernel reclaims the inactive file cache before it fails an allocation, so it is not counted as used; this is
    the "working set" that container runtimes report.
*/
static bool qore_aot_mem_cgroup_level(const char* dir, bool v2, uint64_t& limit, uint64_t& used) {
    char path[640];
    uint64_t current = 0;
    uint64_t inactive_file = 0;
    char stat[8192];
    if (v2) {
        bool have_limit = false;
        uint64_t v;
        snprintf(path, sizeof path, "%s/memory.max", dir);
        if (qore_aot_mem_read_uint_file(path, v)) {
            limit = v;
            have_limit = true;
        }
        // above memory.high, the kernel throttles and reclaims the cgroup: it counts as the limit
        snprintf(path, sizeof path, "%s/memory.high", dir);
        if (qore_aot_mem_read_uint_file(path, v) && (!have_limit || v < limit)) {
            limit = v;
            have_limit = true;
        }
        if (!have_limit) {
            return false;
        }
        snprintf(path, sizeof path, "%s/memory.current", dir);
        if (!qore_aot_mem_read_uint_file(path, current)) {
            return false;
        }
        snprintf(path, sizeof path, "%s/memory.stat", dir);
        if (qore_aot_mem_read_file(path, stat, sizeof stat) > 0) {
            qore_aot_mem_find_key(stat, "inactive_file", inactive_file);
        }
    } else {
        snprintf(path, sizeof path, "%s/memory.limit_in_bytes", dir);
        if (!qore_aot_mem_read_uint_file(path, limit)) {
            return false;
        }
        // an unset v1 limit is a huge value near the top of the address space
        if (limit >= (uint64_t(1) << 62)) {
            return false;
        }
        snprintf(path, sizeof path, "%s/memory.usage_in_bytes", dir);
        if (!qore_aot_mem_read_uint_file(path, current)) {
            return false;
        }
        snprintf(path, sizeof path, "%s/memory.stat", dir);
        if (qore_aot_mem_read_file(path, stat, sizeof stat) > 0) {
            qore_aot_mem_find_key(stat, "total_inactive_file", inactive_file);
        }
    }
    used = current - (inactive_file < current ? inactive_file : current);
    return true;
}

//! Finds the binding memory limit of this process's cgroups: the least headroom of its cgroup and its ancestors
static void qore_aot_mem_read_cgroup(QoreAOTCgroupMemory& rv) {
    char rel[512];
    bool v2 = true;
    const char* root = "/sys/fs/cgroup";
    if (!qore_aot_mem_self_cgroup(true, rel, sizeof rel) || rel[0] != '/') {
        if (!qore_aot_mem_self_cgroup(false, rel, sizeof rel) || rel[0] != '/') {
            return;
        }
        v2 = false;
        root = "/sys/fs/cgroup/memory";
    }
    // a path of "/" is the root of the hierarchy as this process sees it (ex: a container's own cgroup namespace)
    if (rel[1] == '\0') {
        rel[0] = '\0';
    }
    while (true) {
        char dir[600];
        snprintf(dir, sizeof dir, "%s%s", root, rel);
        uint64_t limit = 0;
        uint64_t used = 0;
        if (qore_aot_mem_cgroup_level(dir, v2, limit, used)) {
            uint64_t head = limit > used ? limit - used : 0;
            if (!rv.found || head < rv.headroom) {
                rv.headroom = head;
                rv.limit = limit;
                snprintf(rv.path, sizeof rv.path, "%s", rel[0] ? rel : "/");
                rv.found = true;
            }
        }
        // v1 limits are hierarchical too, but a v1 process does not see its ancestors in a container
        if (!rel[0]) {
            break;
        }
        char* slash = strrchr(rel, '/');
        if (!slash) {
            break;
        }
        *slash = '\0';
    }
}
#endif

QoreAOTMemoryReading qore_aot_read_memory() {
    QoreAOTMemoryReading rv;
#if defined(__linux__)
    uint64_t host_avail = 0;
    uint64_t host_total = 0;
    {
        char buf[4096];
        if (qore_aot_mem_read_file("/proc/meminfo", buf, sizeof buf) > 0) {
            uint64_t kb;
            if (qore_aot_mem_find_key(buf, "MemAvailable:", kb)) {
                host_avail = kb * 1024;
            }
            if (qore_aot_mem_find_key(buf, "MemTotal:", kb)) {
                host_total = kb * 1024;
            }
        }
    }
    QoreAOTCgroupMemory cg;
    qore_aot_mem_read_cgroup(cg);
    if (cg.found && (!host_avail || cg.headroom < host_avail)) {
        rv.available = cg.headroom;
        rv.limit = cg.limit;
        // a cgroup with no headroom is known to be full, not unknown
        if (!rv.available) {
            rv.available = 1;
        }
        return rv;
    }
    if (host_avail) {
        rv.available = host_avail;
        rv.limit = host_total;
        return rv;
    }
#elif defined(__APPLE__)
    // the reclaimable page classes: free, inactive and purgeable
    mach_port_t host = mach_host_self();
    vm_size_t page_size = 0;
    if (host_page_size(host, &page_size) == KERN_SUCCESS) {
        vm_statistics64_data_t vm;
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
        if (host_statistics64(host, HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm), &count)
                == KERN_SUCCESS) {
            rv.available = (static_cast<uint64_t>(vm.free_count) + static_cast<uint64_t>(vm.inactive_count)
                + static_cast<uint64_t>(vm.purgeable_count)) * static_cast<uint64_t>(page_size);
        }
    }
    mach_port_deallocate(mach_task_self(), host);
    uint64_t memsize = 0;
    size_t len = sizeof memsize;
    if (!sysctlbyname("hw.memsize", &memsize, &len, nullptr, 0)) {
        rv.limit = memsize;
    }
    if (rv.available) {
        return rv;
    }
#elif defined(__FreeBSD__) || defined(__DragonFly__)
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size > 0) {
        auto add_pages = [](const char* name, uint64_t& acc) -> bool {
            unsigned val = 0;
            size_t len = sizeof(val);
            if (!sysctlbyname(name, &val, &len, nullptr, 0)) {
                acc += static_cast<uint64_t>(val);
                return true;
            }
            return false;
        };
        uint64_t pages = 0;
        bool any = add_pages("vm.stats.vm.v_free_count", pages);
        any |= add_pages("vm.stats.vm.v_inactive_count", pages);
        add_pages("vm.stats.vm.v_cache_count", pages);
        if (any) {
            rv.available = pages * static_cast<uint64_t>(page_size);
            unsigned long physmem = 0;
            size_t len = sizeof physmem;
            if (!sysctlbyname("hw.physmem", &physmem, &len, nullptr, 0)) {
                rv.limit = physmem;
            }
            return rv;
        }
    }
#endif
    // the physical pages not in use, where the platform has nothing better
#if defined(_SC_AVPHYS_PAGES) && defined(_SC_PAGESIZE)
    long avail_pages = sysconf(_SC_AVPHYS_PAGES);
    long psize = sysconf(_SC_PAGESIZE);
    if (avail_pages > 0 && psize > 0) {
        rv.available = static_cast<uint64_t>(avail_pages) * static_cast<uint64_t>(psize);
#ifdef _SC_PHYS_PAGES
        long phys_pages = sysconf(_SC_PHYS_PAGES);
        if (phys_pages > 0) {
            rv.limit = static_cast<uint64_t>(phys_pages) * static_cast<uint64_t>(psize);
        }
#endif
    }
#endif
    return rv;
}

//! Returns the resident bytes of a process, or 0
static uint64_t qore_aot_mem_resident(pid_t pid) {
#if defined(__linux__)
    char path[64];
    snprintf(path, sizeof path, "/proc/%lld/statm", static_cast<long long>(pid));
    char buf[256];
    if (qore_aot_mem_read_file(path, buf, sizeof buf) <= 0) {
        return 0;
    }
    // "size resident shared text lib data dt", in pages
    const char* p = strchr(buf, ' ');
    uint64_t pages;
    if (!p || !qore_aot_mem_parse_uint(p, pages)) {
        return 0;
    }
    long psize = sysconf(_SC_PAGESIZE);
    return psize > 0 ? pages * static_cast<uint64_t>(psize) : 0;
#elif defined(__APPLE__)
    struct proc_taskinfo ti;
    if (proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &ti, sizeof ti) != static_cast<int>(sizeof ti)) {
        return 0;
    }
    return ti.pti_resident_size;
#else
    (void)pid;
    return 0;
#endif
}

//! Returns a value that identifies one run of a process, or 0 if it has gone
static uint64_t qore_aot_mem_start_id(pid_t pid) {
#if defined(__linux__)
    char path[64];
    snprintf(path, sizeof path, "/proc/%lld/stat", static_cast<long long>(pid));
    char buf[1024];
    if (qore_aot_mem_read_file(path, buf, sizeof buf) <= 0) {
        return 0;
    }
    // the command name in parentheses may hold anything; the start time is the 20th field after it
    const char* p = strrchr(buf, ')');
    if (!p) {
        return 0;
    }
    ++p;
    for (int field = 0; field < 19; ++field) {
        while (*p == ' ') {
            ++p;
        }
        while (*p && *p != ' ') {
            ++p;
        }
        if (!*p) {
            return 0;
        }
    }
    uint64_t start;
    if (!qore_aot_mem_parse_uint(p, start)) {
        return 0;
    }
    // a process that started at boot has 0
    return start + 1;
#elif defined(__APPLE__)
    struct proc_bsdinfo bi;
    if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &bi, sizeof bi) != static_cast<int>(sizeof bi)) {
        return 0;
    }
    return static_cast<uint64_t>(bi.pbi_start_tvsec) * 1000000ull + static_cast<uint64_t>(bi.pbi_start_tvusec) + 1;
#else
    // without a way to tell runs apart, a process counts as live while its pid is
    if (kill(pid, 0) && errno == ESRCH) {
        return 0;
    }
    return 1;
#endif
}

QoreAOTMemorySource qore_aot_system_memory_source() {
    QoreAOTMemorySource src;
    src.read = qore_aot_read_memory;
#if defined(__linux__) || defined(__APPLE__)
    src.resident = qore_aot_mem_resident;
#else
    // the resident memory cannot be read: reservations never count as used, which errs on the safe side
    src.resident = [](pid_t) -> uint64_t { return 0; };
#endif
    src.start_id = qore_aot_mem_start_id;
    src.self = getpid();
#if defined(__linux__)
    QoreAOTCgroupMemory cg;
    qore_aot_mem_read_cgroup(cg);
    if (cg.found) {
        src.domain = std::string("cgroup:") + cg.path;
    } else {
        src.domain = "host";
    }
#else
    src.domain = "host";
#endif
    return src;
}

std::string qore_aot_memory_ledger_path(const std::string& domain) {
    // FNV-1a of the domain
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : domain) {
        h ^= c;
        h *= 1099511628211ull;
    }
    std::string dir;
    {
        QoreEnvironmentValue tmp("TMPDIR");
        if (tmp.c_str() && *tmp.c_str()) {
            dir = tmp.c_str();
        }
    }
    if (dir.empty()) {
        dir = "/tmp";
    }
    while (dir.size() > 1 && dir.back() == '/') {
        dir.pop_back();
    }
    char name[96];
    snprintf(name, sizeof name, "/qore-aot-memory-%lld-%016llx.ledger", static_cast<long long>(geteuid()),
        static_cast<unsigned long long>(h));
    return dir + name;
}

// The default model, measured at -O3 on the units of the four largest Qorus batches (728 units in all, and the largest
// units of the largest batch), one unit at a time, with the resident memory each unit added to the process: once
// lowered, a unit takes 590 to 810 bytes per LLVM instruction at most (3.4 GiB for 5.4 million instructions, the
// largest), and the model's 896 bytes, plus a term for the square of the size of very large functions, covers every
// unit with a margin of 15 to 40% for the large ones.  The size of a unit's source predicts its memory poorly (comments
// and code differ by an order of magnitude), so the source estimate errs on the large side and only holds until the
// unit is lowered.
static constexpr uint64_t QORE_AOT_MEM_MIB = 1024ull * 1024ull;

QoreAOTBatchMemoryModel QoreAOTBatchMemoryModel::getDefault() {
    QoreAOTBatchMemoryModel model;
    model.unit_base = 64 * QORE_AOT_MEM_MIB;
    model.unit_per_source_kib = 1536 * 1024;
    model.parse_per_source_kib = 100 * 1024;
    model.lowered_per_inst = 896;
    model.lowered_sq_divisor = 4096;
    model.unit_override = 0;
    QoreEnvironmentValue pw("QORE_AOT_BATCH_MEM_PER_JOB_MB");
    if (pw.c_str()) {
        char* end = nullptr;
        long long mb = strtoll(pw.c_str(), &end, 10);
        if (mb > 0 && end != pw.c_str()) {
            model.unit_override = static_cast<uint64_t>(mb) * QORE_AOT_MEM_MIB;
        }
    }
    return model;
}

//! multiplies a byte count by a factor in 1/1024 units, saturating
static uint64_t qore_aot_mem_scale(uint64_t bytes, uint64_t per_kib) {
    if (bytes && per_kib > UINT64_MAX / bytes) {
        return UINT64_MAX;
    }
    return bytes * per_kib / 1024;
}

uint64_t QoreAOTBatchMemoryModel::estimateUnit(uint64_t source_bytes) const {
    if (unit_override) {
        return unit_override;
    }
    uint64_t e = qore_aot_mem_scale(source_bytes, unit_per_source_kib);
    return e > UINT64_MAX - unit_base ? UINT64_MAX : unit_base + e;
}

uint64_t QoreAOTBatchMemoryModel::estimateParse(uint64_t total_source_bytes) const {
    return qore_aot_mem_scale(total_source_bytes, parse_per_source_kib);
}

uint64_t QoreAOTBatchMemoryModel::estimateLowered(uint64_t insts, uint64_t sum_sq_insts) const {
    if (unit_override) {
        return unit_override;
    }
    uint64_t e = (lowered_per_inst && insts > UINT64_MAX / lowered_per_inst) ? UINT64_MAX : insts * lowered_per_inst;
    uint64_t sq = lowered_sq_divisor ? sum_sq_insts / lowered_sq_divisor : 0;
    e = e > UINT64_MAX - sq ? UINT64_MAX : e + sq;
    return e > UINT64_MAX - unit_base ? UINT64_MAX : unit_base + e;
}

QoreAOTBatchMemoryLedger::QoreAOTBatchMemoryLedger(const std::string& path) : path(path) {
    if (!path.empty()) {
        fd = openFile();
        usable = fd >= 0;
    }
}

QoreAOTBatchMemoryLedger::~QoreAOTBatchMemoryLedger() {
    if (fd >= 0) {
        close(fd);
    }
}

int QoreAOTBatchMemoryLedger::openFile() const {
    // the file is in a shared directory: never follow a link, and use only a regular file of this user's that
    // nobody else can access
    int f = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (f < 0) {
        return -1;
    }
    struct stat st;
    if (fstat(f, &st) || !S_ISREG(st.st_mode) || st.st_uid != geteuid()
            || (st.st_mode & (S_IRWXG | S_IRWXO))) {
        close(f);
        return -1;
    }
    return f;
}

QoreAOTBatchMemoryLedger::Locked::Locked(QoreAOTBatchMemoryLedger& ledger) : ledger(ledger) {
    memset(slots, 0, sizeof slots);
    if (!ledger.usable) {
        return;
    }
    // The last process to leave the ledger removes the file; a process that was waiting for the lock of the removed
    // file then opens the file at the path again.  A path that keeps changing is not used.
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        if (ledger.fd < 0) {
            ledger.fd = ledger.openFile();
            if (ledger.fd < 0) {
                return;
            }
        }
        int rc;
        while ((rc = flock(ledger.fd, LOCK_EX)) && errno == EINTR) {
        }
        if (rc) {
            return;
        }
        struct stat held;
        struct stat current;
        if (!fstat(ledger.fd, &held) && !lstat(ledger.path.c_str(), &current) && held.st_dev == current.st_dev
                && held.st_ino == current.st_ino) {
            locked = true;
            break;
        }
        flock(ledger.fd, LOCK_UN);
        close(ledger.fd);
        ledger.fd = -1;
    }
    if (!locked) {
        return;
    }
    // a short or empty file (a new ledger) reads as free slots
    char* buf = reinterpret_cast<char*>(slots);
    size_t total = 0;
    while (total < sizeof slots) {
        ssize_t n = pread(ledger.fd, buf + total, sizeof slots - total, static_cast<off_t>(total));
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (!n) {
            break;
        }
        total += static_cast<size_t>(n);
    }
    if (total < sizeof slots) {
        memset(buf + total, 0, sizeof slots - total);
    }
}

QoreAOTBatchMemoryLedger::Locked::~Locked() {
    if (!locked) {
        return;
    }
    bool empty = true;
    for (const Slot& s : slots) {
        if (s.pid) {
            empty = false;
            break;
        }
    }
    if (empty) {
        // nobody has a reservation: the file is removed while it is locked, so that no process uses it after this
        unlink(ledger.path.c_str());
    } else if (dirty) {
        const char* buf = reinterpret_cast<const char*>(slots);
        size_t total = 0;
        while (total < sizeof slots) {
            ssize_t n = pwrite(ledger.fd, buf + total, sizeof slots - total, static_cast<off_t>(total));
            if (n < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }
            total += static_cast<size_t>(n);
        }
    }
    flock(ledger.fd, LOCK_UN);
}

uint64_t QoreAOTBatchMemoryLedger::Locked::othersUnused(const QoreAOTMemorySource& src) {
    uint64_t sum = 0;
    for (Slot& s : slots) {
        if (!s.pid || s.pid == static_cast<int64_t>(src.self)) {
            continue;
        }
        pid_t pid = static_cast<pid_t>(s.pid);
        uint64_t start = src.start_id(pid);
        if (!start || start != s.start_id) {
            // the process has gone
            memset(&s, 0, sizeof s);
            dirty = true;
            continue;
        }
        uint64_t res = src.resident(pid);
        uint64_t used = res > s.resident_base ? res - s.resident_base : 0;
        if (s.reserved > used) {
            sum += s.reserved - used;
        }
    }
    return sum;
}

void QoreAOTBatchMemoryLedger::Locked::setOwn(const QoreAOTMemorySource& src, uint64_t self_start,
        uint64_t resident_base, uint64_t reserved) {
    Slot* own = nullptr;
    Slot* free_slot = nullptr;
    for (Slot& s : slots) {
        if (s.pid == static_cast<int64_t>(src.self)) {
            own = &s;
            break;
        }
        if (!s.pid && !free_slot) {
            free_slot = &s;
        }
    }
    if (!reserved) {
        if (own) {
            memset(own, 0, sizeof *own);
            dirty = true;
        }
        return;
    }
    if (!own) {
        own = free_slot;
        if (!own) {
            // the ledger is full: this process's reservations stay its own
            return;
        }
    }
    own->pid = static_cast<int64_t>(src.self);
    own->start_id = self_start;
    own->resident_base = resident_base;
    own->reserved = reserved;
    dirty = true;
}

QoreAOTBatchMemoryGovernor::QoreAOTBatchMemoryGovernor(QoreAOTMemorySource src,
        const QoreAOTBatchMemoryModel& model, const std::string& ledger_path)
        : src(std::move(src)), model(model), ledger(ledger_path) {
    self_start = this->src.start_id(this->src.self);
}

QoreAOTBatchMemoryGovernor::~QoreAOTBatchMemoryGovernor() {
    if (!published) {
        return;
    }
    QoreAOTBatchMemoryLedger::Locked l(ledger);
    if (l.ok()) {
        l.setOwn(src, self_start, resident_base, 0);
    }
}

uint64_t QoreAOTBatchMemoryGovernor::headroomFor(uint64_t limit) {
    if (!limit) {
        return 512 * QORE_AOT_MEM_MIB;
    }
    // a tenth of the domain for what nothing reserves: the build tool, the linker, other compilers, and the
    // estimates' error
    uint64_t h = limit / 10;
    if (h < 256 * QORE_AOT_MEM_MIB) {
        h = 256 * QORE_AOT_MEM_MIB;
    } else if (h > 2048 * QORE_AOT_MEM_MIB) {
        h = 2048 * QORE_AOT_MEM_MIB;
    }
    return h;
}

void QoreAOTBatchMemoryGovernor::takeBase() {
    if (!based) {
        resident_base = src.resident(src.self);
        based = true;
    }
}

uint64_t QoreAOTBatchMemoryGovernor::ownUnused() const {
    uint64_t reserved = parse_bytes + inflight_bytes;
    uint64_t res = src.resident(src.self);
    uint64_t used = res > resident_base ? res - resident_base : 0;
    return reserved > used ? reserved - used : 0;
}

void QoreAOTBatchMemoryGovernor::publish() {
    if (!ledger.ok()) {
        return;
    }
    QoreAOTBatchMemoryLedger::Locked l(ledger);
    if (l.ok()) {
        l.setOwn(src, self_start, resident_base, parse_bytes + inflight_bytes);
        published = (parse_bytes + inflight_bytes) != 0;
    }
}

void QoreAOTBatchMemoryGovernor::beginParse(uint64_t total_source_bytes) {
    std::lock_guard<std::mutex> g(m);
    takeBase();
    parse_bytes = model.estimateParse(total_source_bytes);
    stats.parse_estimate = parse_bytes;
    publish();
}

void QoreAOTBatchMemoryGovernor::beginCodegen() {
    std::lock_guard<std::mutex> g(m);
    takeBase();
    uint64_t res = src.resident(src.self);
    if (res) {
        // the parse is resident now: what it took replaces its estimate
        parse_bytes = res > resident_base ? res - resident_base : 0;
    }
    stats.parse_measured = parse_bytes;
    publish();
}

QoreAOTBatchMemoryGovernor::Decision QoreAOTBatchMemoryGovernor::decide(uint64_t bytes, bool commit) {
    takeBase();
    Decision d;
    // the free memory and the other processes' reservations are read under the ledger's lock, so that two
    // processes deciding at once see each other's reservations
    QoreAOTBatchMemoryLedger::Locked l(ledger);
    QoreAOTMemoryReading r = src.read();
    d.available = r.available;
    d.headroom = headroomFor(r.limit);
    d.reserved = ownUnused() + (l.ok() ? l.othersUnused(src) : 0);
    if (!inflight_count) {
        // the first unit always proceeds, so that the batch cannot stall
        d.admitted = true;
        d.alone = r.available && r.available < d.reserved + bytes + d.headroom;
    } else if (growing) {
        // a unit in work waits for its reservation to grow: it goes first
        d.admitted = false;
    } else if (!r.available) {
        // the memory cannot be read: the worker count is limited by the CPUs only, as without admission
        d.admitted = true;
    } else {
        d.admitted = r.available >= d.reserved + bytes + d.headroom;
    }
    if (!commit) {
        return d;
    }
    if (!d.admitted) {
        ++stats.deferred;
        return d;
    }
    inflight_bytes += bytes;
    ++inflight_count;
    if (d.alone) {
        ++stats.alone;
    } else {
        ++stats.admitted;
    }
    if (inflight_count > stats.max_inflight) {
        stats.max_inflight = inflight_count;
    }
    if (inflight_bytes > stats.max_inflight_bytes) {
        stats.max_inflight_bytes = inflight_bytes;
    }
    if (l.ok()) {
        l.setOwn(src, self_start, resident_base, parse_bytes + inflight_bytes);
        published = true;
    }
    return d;
}

bool QoreAOTBatchMemoryGovernor::wouldAdmit(uint64_t bytes) {
    std::lock_guard<std::mutex> g(m);
    return decide(bytes, false).admitted;
}

QoreAOTBatchMemoryGovernor::Decision QoreAOTBatchMemoryGovernor::reserve(uint64_t bytes) {
    std::lock_guard<std::mutex> g(m);
    return decide(bytes, true);
}

void QoreAOTBatchMemoryGovernor::release(uint64_t bytes) {
    std::lock_guard<std::mutex> g(m);
    assert(inflight_count);
    assert(inflight_bytes >= bytes);
    if (inflight_count) {
        --inflight_count;
    }
    inflight_bytes = inflight_bytes > bytes ? inflight_bytes - bytes : 0;
    publish();
    cond.notify_all();
}

uint64_t QoreAOTBatchMemoryGovernor::resize(uint64_t from, uint64_t to) {
    std::unique_lock<std::mutex> g(m);
    assert(inflight_count);
    assert(inflight_bytes >= from);
    if (to <= from) {
        inflight_bytes -= from - to;
        publish();
        cond.notify_all();
        return to;
    }
    ++stats.grown;
    uint64_t extra = to - from;
    ++growing;
    // a waiting unit may now be the only one that is not waiting
    cond.notify_all();
    bool waited = false;
    while (true) {
        // every unit in work but those waiting to grow is running
        bool alone = inflight_count == growing;
        bool fits;
        {
            QoreAOTBatchMemoryLedger::Locked l(ledger);
            QoreAOTMemoryReading r = src.read();
            uint64_t reserved = ownUnused() + (l.ok() ? l.othersUnused(src) : 0);
            fits = !r.available || r.available >= reserved + extra + headroomFor(r.limit);
        }
        if (fits || alone) {
            if (!fits) {
                ++stats.grown_alone;
            }
            break;
        }
        if (!waited) {
            waited = true;
            ++stats.grow_waits;
        }
        // a release of this process, or a change of the units waiting, can let it proceed
#ifdef DEBUG
        ++dbg_grow_waiting;
        dbg_cond.notify_all();
#endif
        cond.wait(g);
#ifdef DEBUG
        --dbg_grow_waiting;
#endif
    }
    --growing;
    inflight_bytes += extra;
    if (inflight_bytes > stats.max_inflight_bytes) {
        stats.max_inflight_bytes = inflight_bytes;
    }
    publish();
    cond.notify_all();
    return to;
}

unsigned QoreAOTBatchMemoryGovernor::inflight() const {
    std::lock_guard<std::mutex> g(m);
    return inflight_count;
}

QoreAOTBatchMemoryGovernor::Stats QoreAOTBatchMemoryGovernor::getStats() const {
    std::lock_guard<std::mutex> g(m);
    return stats;
}

#ifdef DEBUG
void QoreAOTBatchMemoryGovernor::dbgWaitForGrowWaiters(unsigned n) {
    std::unique_lock<std::mutex> g(m);
    dbg_cond.wait(g, [&]() { return dbg_grow_waiting >= n; });
}
#endif
