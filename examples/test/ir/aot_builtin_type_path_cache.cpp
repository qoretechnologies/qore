/* -*- mode: c++; indent-tabs-mode: nil -*- */
// Copyright (C) 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <qore/intern/LocalVar.h>
#include <qore/intern/QoreIR.h>
#include <qore/intern/QoreAOT.h>
#include <qore/intern/JITRuntime.h>

#include <atomic>
#include <barrier>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>
#if defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif

// Call the real helper used by generated AOT code, with an immutable path during each context's lifetime.
// Reusing the buffer for the next context simulates unloading/reloading an image at the same address.
static bool check(QoreAOTContext* ctx, const char* path, const char* expected) {
    ExceptionSink xsink;
    uint64_t bits = qore_rt_make_list_by_type_path_cached(ctx, nullptr, 0, path, &xsink);
    QoreValue value;
    memcpy(&value, &bits, sizeof(value));
    ValueHolder holder(value, &xsink);
    const char* actual = value.getFullTypeName();
    if (xsink || strcmp(actual, expected)) {
        fprintf(stderr, "path %s: expected %s, got %s\n", path, expected, actual);
        xsink.handleExceptions();
        return false;
    }
    return true;
}

static bool reuse() {
    // Keep this address in a different global slot from the guarded and concurrent fixtures.
    alignas(4096) char storage[4096];
    char* path = storage + 64;
    const char* types[] = {"list<int>", "list<string>", "list<int>", "list<*int>",
        "list<int>", "list<float>", "list<int>", "list<list<int>>", "list<int>"};
    for (const char* type : types) {
        strcpy(path, type);
        ExceptionSink xsink;
        QoreProgramHelper pgm(PO_MODERN, xsink);
        QoreProgramContextHelper current(*pgm);
        QoreAOTContext ctx;
        ctx.pgm = *pgm;
        // Exercise the miss, repeat hit, and the uncached-context form.
        if (!check(&ctx, path, type) || !check(&ctx, path, type) || !check(nullptr, path, type)) {
            return false;
        }
    }
    return true;
}

static bool guardedReuse() {
#if defined(__linux__) || defined(__APPLE__)
    long page = sysconf(_SC_PAGESIZE);
    if (page <= 0) {
        return false;
    }
    char* memory = static_cast<char*>(mmap(nullptr, page * 2, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (memory == MAP_FAILED) {
        return false;
    }
    char* path = memory + page - sizeof("list<int>");
    bool ok = true;
    for (const char* type : {"list<list<int>>", "list<int>"}) {
        strcpy(path, type);
        if (!strcmp(type, "list<int>") && mprotect(memory + page, page, PROT_NONE)) {
            ok = false;
            break;
        }
        ExceptionSink xsink;
        QoreProgramHelper pgm(PO_MODERN, xsink);
        QoreProgramContextHelper current(*pgm);
        QoreAOTContext ctx;
        ctx.pgm = *pgm;
        if (!check(&ctx, path, type) || !check(&ctx, path, type)) {
            ok = false;
            break;
        }
    }
    munmap(memory, page * 2);
    return ok;
#else
    return true;
#endif
}

static bool concurrentContexts(bool shared) {
    // Put the two paths in different global cache slots but the same validation bitset word.
    alignas(4096) static const char paths[][32] = {"list<*string>", "list<softint>"};
    ExceptionSink xsink;
    QoreProgramHelper pgm(PO_MODERN, xsink);
    QoreAOTContext common;
    common.pgm = *pgm;
    std::atomic<bool> ok{true};
    std::vector<std::thread> threads;
    std::barrier start(4);
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&]() {
            QoreForeignThreadHelper foreign;
            QoreProgramContextHelper current(*pgm);
            QoreAOTContext own;
            own.pgm = *pgm;
            QoreAOTContext* ctx = shared ? &common : &own;
            start.arrive_and_wait();
            for (int n = 0; n < 1000; ++n) {
                for (const auto& path : paths) {
                    if (!check(ctx, path, path)) {
                        ok = false;
                        return;
                    }
                }
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    return ok;
}

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    bool ok = guardedReuse() && reuse() && concurrentContexts(false) && concurrentContexts(true);
    qore_cleanup();
    if (ok) {
        puts("PASS: builtin AOT type paths, address reuse, guarded shorter path and concurrent contexts");
    }
    return ok ? 0 : 1;
}
