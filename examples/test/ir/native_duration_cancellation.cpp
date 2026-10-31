/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
#include <qore/Qore.h>
#include <array>
#include <string>
#include <cstdio>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    {
        std::array<std::string, 4> inputs = {
            "P" + std::string(205, '0') + "1D",
            "PT1." + std::string(205, '0') + "S",
            "P0000-00-00T00:00:01." + std::string(205, '0'),
            "P0000-00-00T00:00:01" + std::string(205, ' '),
        };
        for (const std::string& input : inputs) {
            ExceptionSink xsink;
            // No language checkpoint can consume the flag before the parser.
            assert(!qore_cancel_thread(q_gettid(), "duration cancellation test"));
            DateTime cancelled(input.c_str(), &xsink);
            assert(xsink);
            assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("THREAD-CANCELLED"));
            qore_clear_thread_cancel();
            xsink.clear();
            DateTime restored(input.c_str(), &xsink);
            assert(!xsink && restored.isRelative());
        }
    }
    qore_cleanup();
    std::puts("native-duration-cancellation: ok");
}
