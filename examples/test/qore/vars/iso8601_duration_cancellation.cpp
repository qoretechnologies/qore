/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
#include <qore/Qore.h>
#include <string>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>

static void checkCancellation(const std::string& lexical) {
    ExceptionSink xsink;
    assert(!qore_cancel_thread(q_gettid(), "duration cancellation test"));
    {
        ReferenceHolder<DateTimeNode> value(new DateTimeNode(lexical.c_str(), &xsink), &xsink);
        assert(xsink);
        assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("THREAD-CANCELLED"));
    }
    qore_clear_thread_cancel();
    xsink.clear();
    {
        ReferenceHolder<DateTimeNode> value(new DateTimeNode(lexical.c_str(), &xsink), &xsink);
        assert(!xsink);
        assert(value->isRelative());
    }
    assert(!xsink);
}

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    checkCancellation("P" + std::string(1000, '0') + "1D");
    checkCancellation("PT0." + std::string(1000, '0') + "1S");
    checkCancellation("P1D" + std::string(1000, ' '));
    std::string components("P");
    for (int i = 0; i < 64; ++i) {
        components += "1D";
    }
    checkCancellation(components);
    qore_cleanup();
    std::puts("iso8601-duration-cancellation: ok");
}
