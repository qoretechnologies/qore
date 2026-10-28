/* Copyright 2026 Qore Technologies, s.r.o. -- SPDX-License-Identifier: MIT */
#define _QORE_LIB_INTERN
#define XXH_INLINE_ALL
#include <qore/intern/xxhash/xxhash.h>
#include <qore/Qore.h>
#include <qore/intern/QoreAOT.h>
#include <qore/intern/JITRuntime.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <array>

struct Probe {
    QoreProgram* caller;
    bool enabled;
    std::array<std::array<void*, 3>, 5> slots{};
    unsigned calls = 0;
    unsigned admitted = 0;
    unsigned rejected = 0;
};
static QoreValue inspect(const void* pointer, const QoreListNode* args, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    auto& probe = *const_cast<Probe*>(static_cast<const Probe*>(pointer));
    assert(!*xsink && getProgram() == probe.caller && rc.getProgram() == probe.caller);
    int depth = args->retrieveEntry(0).getAsBigInt();
    assert(depth >= 0 && depth <= 4);
    const auto* f = reinterpret_cast<const QoreFunction*>(probe.caller->findFunction("ImportedBodySlots::run"));
    assert(f);
    const auto* uvb = f->first()->getUserVariantBase();
    assert(uvb && uvb->hasCachedAOT());
    auto* ctx = uvb->getCachedAOTContext();
    assert(ctx && ctx->pgm != probe.caller);
    auto check = [&](void* slot, const char* control) {
        assert(bool(slot) == (probe.enabled && !std::getenv(control)));
        slot ? ++probe.admitted : ++probe.rejected;
        return slot;
    };
    unsigned found = 0;
    for (int i = 0; i < ctx->num_locals; ++i) {
        auto* local = ctx->locals[i];
        if (!local) { continue; }
        const char* names[] = {"slot_auto", "slot_list", "slot_object"};
        for (unsigned k = 0; k < 3; ++k) {
            if (std::strcmp(local->getName(), names[k])) { continue; }
            ++found;
            assert(!local->closureUse() && !local->isSelf() && !local->isTopLevel());
            void* slot = check(qore_rt_get_reload_local_slot_aot_v1(ctx, i, &rc),
                "QORE_DISABLE_AOT_PREPARED_LOCAL_RELOAD");
            if (k == 0 || k == 2) {
                void* other = k == 0
                    ? check(qore_rt_get_auto_local_slot_aot_v2(ctx, i, &rc), "QORE_DISABLE_AOT_AUTO_LOCAL_SLOT")
                    : check(qore_rt_get_class_local_slot_aot_v1(ctx, i, &rc), "QORE_DISABLE_AOT_CLASS_LOCAL_SLOT");
                assert(!slot || !other || slot == other);
                if (!slot) { slot = other; }
            }
            if (!slot) { continue; }
            auto* value = static_cast<LocalVarValue*>(slot);
            assert(value->local_var == local && !value->frame_boundary && !value->finalized);
            if (probe.slots[depth][k]) { assert(probe.slots[depth][k] == slot); }
            probe.slots[depth][k] = slot;
            for (int parent = depth + 1; parent <= 4; ++parent) { assert(probe.slots[parent][k] != slot); }
            if (k == 0) { assert(value->val.getValue().getAsBigInt() == depth); }
            if (k == 1) {
                auto* list = value->val.getValue().get<const QoreListNode>();
                assert(list && list->retrieveEntry(0).getAsBigInt() == depth + 17);
            }
            if (k == 2) { assert(value->val.getType() == NT_OBJECT); }
        }
        if (!(i % 100)) { assert(!qore_check_cancel(xsink, "imported slot probe")); }
    }
    assert(found == 3);
    assert(!qore_rt_get_reload_local_slot_aot_v1(ctx, -1, &rc));
    assert(!qore_rt_get_auto_local_slot_aot_v2(ctx, ctx->num_locals, &rc));
    assert(!qore_rt_get_class_local_slot_aot_v1(ctx, 0, nullptr));
    ++probe.calls;
    return QoreValue(int64_t(7));
}
static void run(bool enabled) {
    ExceptionSink xsink;
    QoreProgramHelper program(PO_MODERN, xsink);
    Probe probe{*program, enabled};
    {
        ProgramRuntimeExternalParseContextHelper parse(*program);
        assert(parse);
        program->getRootNS()->addBuiltinVariant(&probe, "inspectSlots", inspect,
            QCF_NO_FLAGS, QDOM_DEFAULT, bigIntTypeInfo, 1, bigIntTypeInfo, QORE_PARAM_NO_ARG, "depth");
    }
    program->setExecMode(QEM_IR, true);
    program->parse("%modern\n%requires ImportedBodySlots\n"
        "int sub host() { return ImportedBodySlots::run(\\inspectSlots(), 4); }\n", "imported-slot-host", &xsink);
    if (xsink) { xsink.handleExceptions(); assert(false); }
    QoreExternalProgramContextHelper active(&xsink, *program);
    ValueHolder result(program->callFunction("host", nullptr, &xsink), &xsink);
    if (xsink) { xsink.handleExceptions(); assert(false); }
    assert(result->getAsBigInt() == 70 && probe.calls == 10);
    assert(probe.admitted + probe.rejected == 50);
    std::printf("imported-body-slots: %u callbacks, %u admitted, %u rejected; recursive storage preserved\n",
        probe.calls, probe.admitted, probe.rejected);
}
int main(int argc, char** argv) {
    assert(argc == 2);
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    run(std::atoi(argv[1]));
    qore_cleanup();
}
