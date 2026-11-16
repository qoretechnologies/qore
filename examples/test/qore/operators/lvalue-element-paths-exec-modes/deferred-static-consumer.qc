# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# compiled by lvalue-element-paths-exec-modes.qtest before its provider exists, so that its references to the static
# members of DeferredStaticProvider are deferred when it is parsed

%modern
class DeferredStaticConsumer {
    #! changes the static members directly
    static string direct() {
        ++DeferredStaticProvider::count;
        DeferredStaticProvider::count += 2;
        DeferredStaticProvider::count = DeferredStaticProvider::count * 3;
        DeferredStaticProvider::h.k += 5;
        ++DeferredStaticProvider::h{"k"};
        auto r = remove DeferredStaticProvider::h.j;
        delete DeferredStaticProvider::h.d;
        return sprintf("direct: %y %y %y", DeferredStaticProvider::count, DeferredStaticProvider::h, r);
    }

    #! changes the static members in an on_exit handler, which is serialized as IR
    static string handler() {
        {
            on_exit {
                DeferredStaticProvider::count -= 10;
                DeferredStaticProvider::h.k *= 2;
                DeferredStaticProvider::h.n = remove DeferredStaticProvider::h.k;
            }
        }
        return sprintf("handler: %y %y", DeferredStaticProvider::count, DeferredStaticProvider::h);
    }
}
