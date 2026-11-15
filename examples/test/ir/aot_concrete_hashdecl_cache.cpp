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
#include <string>
#include <thread>
#include <vector>

static bool check(QoreAOTContext* ctx, const char* path, int expected, bool concrete = true) {
    ExceptionSink xsink;
    QoreValue nothing;
    uint64_t bits;
    memcpy(&bits, &nothing, sizeof(bits));
    bits = concrete ? qore_rt_new_hash_decl_from_hash_by_concrete_path_cached(ctx, path, bits, 0, &xsink)
        : qore_rt_new_hash_decl_from_hash_by_path_cached(ctx, path, bits, 0, &xsink);
    QoreValue value;
    memcpy(&value, &bits, sizeof(value));
    ValueHolder result(value, &xsink);
    if (xsink || value.getType() != NT_HASH
            || value.get<QoreHashNode>()->getKeyValue("x").getAsBigInt() != expected) {
        fprintf(stderr, "hashdecl %s: expected x = %d\n", path, expected);
        xsink.handleExceptions();
        return false;
    }
    return true;
}

static bool missing(QoreAOTContext* ctx) {
    ExceptionSink xsink;
    QoreValue nothing;
    uint64_t bits;
    memcpy(&bits, &nothing, sizeof(bits));
    bits = qore_rt_new_hash_decl_from_hash_by_concrete_path_cached(ctx, "Missing", bits, 0, &xsink);
    QoreValue result;
    memcpy(&result, &bits, sizeof(result));
    bool ok = xsink && result.isNothing();
    result.discard(&xsink);
    xsink.clear();
    return ok;
}

static bool prepare(QoreProgram* pgm, int offset, ExceptionSink& xsink) {
    std::string source = "%modern\n";
    // More paths than either front cache can hold: includes collisions and fallback hits.
    for (int i = 0; i < 64; ++i) {
        source += "hashdecl H" + std::to_string(i) + " { int x = " + std::to_string(offset + i) + "; }\n";
    }
    pgm->parse(source.c_str(), "concrete-hashdecl-cache", &xsink);
    if (xsink) {
        xsink.handleExceptions();
        return false;
    }
    return true;
}

static bool programReuse() {
    char paths[64][16];
    for (int i = 0; i < 64; ++i) {
        snprintf(paths[i], sizeof(paths[i]), "H%d", i);
    }
    // A no-Program context must follow the current Program, even when that Program is destroyed and replaced.
    QoreAOTContext unowned;
    for (int run = 0; run < 12; ++run) {
        ExceptionSink xsink;
        QoreProgramHelper pgm(PO_MODERN, xsink);
        if (!prepare(*pgm, run * 100, xsink)) {
            return false;
        }
        QoreProgramContextHelper current(*pgm);
        QoreAOTContext owned;
        owned.pgm = *pgm;
        for (int round = 0; round < 2; ++round) {
            for (int i = 0; i < 64; ++i) {
                if (!check(&owned, paths[i], run * 100 + i)
                        || !check(&unowned, paths[i], run * 100 + i)
                        || !check(&owned, paths[i], run * 100 + i, false)) {
                    return false;
                }
            }
        }
        if (!missing(&owned)) {
            return false;
        }
    }
    return true;
}

static bool concurrent(bool shared) {
    ExceptionSink xsink;
    QoreProgramHelper pgm(PO_MODERN, xsink);
    if (!prepare(*pgm, 1000, xsink)) {
        return false;
    }
    QoreProgramContextHelper current(*pgm);
    QoreAOTContext common;
    common.pgm = *pgm;
    std::atomic<bool> ok{true};
    std::barrier start(4);
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&]() {
            QoreForeignThreadHelper foreign;
            QoreProgramContextHelper current(*pgm);
            QoreAOTContext own;
            own.pgm = *pgm;
            auto* ctx = shared ? &common : &own;
            start.arrive_and_wait();
            for (int j = 0; j < 1000; ++j) {
                if (!check(ctx, "H0", 1000) || !check(ctx, "H1", 1001)) {
                    ok = false;
                    return;
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
    bool ok = programReuse() && concurrent(false) && concurrent(true);
    qore_cleanup();
    if (ok) {
        puts("PASS: concrete hashdecl cache, Program reuse, collisions, errors and concurrent contexts");
    }
    return ok ? 0 : 1;
}
