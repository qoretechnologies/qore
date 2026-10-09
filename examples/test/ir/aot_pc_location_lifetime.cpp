/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
#define _QORE_LIB_INTERN
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <qore/intern/QoreAOT.h>
#include <qore/intern/Function.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

extern "C" void init_PC_LOCATION_LIFETIME_qo(QoreProgram*);

static void check() {
    ExceptionSink xsink;
    std::string backing_file;
    QoreProgramHelper program(PO_MODERN, xsink);
    qore_aot_script_begin_batch(*program);
    init_PC_LOCATION_LIFETIME_qo(*program);
    assert(!qore_aot_script_end_batch(*program));
    QoreExternalProgramContextHelper context(&xsink, *program);
    assert(!xsink);
    auto invoke = [&](const char* name, int line) {
        ValueHolder result(program->callFunction(name, nullptr, &xsink), &xsink);
        assert(xsink);
        ReferenceHolder<QoreHashNode> info(xsink.getExceptionInfo(), &xsink);
        assert(!xsink && info);
        assert(info->getKeyValue("err").get<const QoreStringNode>()->equal("PC-LOCATION-LIFETIME"));
        const auto* file = info->getKeyValue("file").get<const QoreStringNode>();
        int actual_line = info->getKeyValue("line").getAsBigInt();
        if (line && actual_line != line) {
            std::fprintf(stderr, "%s: expected line %d, got %lld in %s\n", name, line,
                static_cast<long long>(info->getKeyValue("line").getAsBigInt()), file ? file->c_str() : "<none>");
        }
        assert(actual_line > 0 && (!line || actual_line == line));
        assert(file && std::strstr(file->c_str(), "pc_location_lifetime.qr"));
        if (backing_file.empty()) {
            backing_file = file->c_str();
        }
        return actual_line;
    };
    invoke("pcLifetimeDirect", 3);
    // This test checks lifetime, including inlined extra descriptors, against
    // the same artifact's initial location (not compiler-specific inlining).
    int inline_line = invoke("pcLifetimeInline", 0);
    const auto* direct = reinterpret_cast<const QoreFunction*>(program->findFunction("pcLifetimeDirect"));
    assert(direct);
    auto* direct_ctx = direct->first()->getUserVariantBase()->getCachedAOTContext();
    assert(direct_ctx);
    for (int i = 0; i < direct_ctx->num_locs; ++i) {
        const_cast<QoreProgramLocation*>(direct_ctx->locs[i])->setFile(backing_file.c_str());
        if (!(i % 100)) {
            assert(!qore_check_cancel(&xsink, "PC location string lifetime test"));
        }
    }
    // Reattachment snapshots a caller-owned string. Overwrite its live buffer
    // without reallocating: copying only the descriptor would still fail.
    direct_ctx->pc_loc_map_attached.store(false, std::memory_order_release);
    invoke("pcLifetimeDirect", 3);
    std::fill(backing_file.begin(), backing_file.end(), 'x');
    invoke("pcLifetimeDirect", 3);
    // Each Program owns its ordinary and inlined location descriptors. The
    // process-wide native-PC registry must own independent snapshots. Poison
    // while descriptors are still allocated so a borrowed pointer fails a
    // deterministic assertion rather than relying on allocator reuse/UAF.
    for (const char* name : {"pcLifetimeDirect", "pcLifetimeLeaf", "pcLifetimeInline"}) {
        const auto* function = reinterpret_cast<const QoreFunction*>(program->findFunction(name));
        assert(function);
        const UserVariantBase* uvb = function->first()->getUserVariantBase();
        assert(uvb);
        QoreAOTContext* ctx = uvb->getCachedAOTContext();
        if (!ctx) {
            continue; // An inlined leaf need not materialize its own context.
        }
        auto poison = [](const QoreProgramLocation* loc) {
            auto* owned = const_cast<QoreProgramLocation*>(loc);
            owned->start_line = -321;
            owned->setFile("retired-context");
            owned->setSource("retired-source");
            owned->setLanguage("retired-language");
        };
        for (int i = 0; i < ctx->num_locs; ++i) {
            poison(ctx->locs[i]);
            if (!(i % 100)) {
                assert(!qore_check_cancel(&xsink, "PC location lifetime test"));
            }
        }
        unsigned i = 0;
        for (const auto* loc : ctx->pc_extra_locs) {
            poison(loc);
            if (!(i++ % 100)) {
                assert(!qore_check_cancel(&xsink, "inlined PC location lifetime test"));
            }
        }
    }
    invoke("pcLifetimeDirect", 3);
    invoke("pcLifetimeInline", inline_line);
}

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    check();
    // Re-register the same native PCs after the original Program is gone;
    // independent foreign threads also replace/read the shared registry.
    auto worker = []() {
        for (unsigned i = 0; i < 8; ++i) {
            QoreForeignThreadHelper foreign;
            check();
        }
    };
    std::thread first(worker), second(worker);
    first.join();
    second.join();
    qore_cleanup();
    std::puts("aot-pc-location-lifetime: ok");
}
