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
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

extern "C" void init_PC_LOCATION_LIFETIME_qo(QoreProgram*);

static const QoreProgramLocation* check() {
    ExceptionSink xsink;
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
        return actual_line;
    };
    invoke("pcLifetimeDirect", 3);
    invoke("pcLifetimeInline", 6);
    const auto* direct = reinterpret_cast<const QoreFunction*>(program->findFunction("pcLifetimeDirect"));
    assert(direct);
    const auto* uvb = direct->first()->getUserVariantBase();
    assert(uvb && uvb->getCachedAOTContext());
    const QoreProgramLocation* image_location = uvb->getAOTBodyLocation();
    assert(image_location && image_location->start_line > 0 && image_location->getFile());
    assert(std::strstr(image_location->getFile(), "pc_location_lifetime.qr"));
    // The location is held by the linked native image. Return it across teardown
    // of every Program-owned context and check it again from foreign threads.
    return image_location;
}

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    const auto* location = check();
    int line = location->start_line;
    std::string file(location->getFile());
    auto worker = [location, line, file]() {
        for (unsigned i = 0; i < 8; ++i) {
            QoreForeignThreadHelper foreign;
            assert(check() == location);
            assert(location->start_line == line && location->getFile() == file);
        }
    };
    std::thread first(worker), second(worker);
    first.join();
    second.join();
    qore_cleanup();
    std::puts("aot-image-location-lifetime: ok");
}
