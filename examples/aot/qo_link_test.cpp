// AOT Phase 4 slice 3: smoke test that a .qo built with `qcc -c` can be linked
// into a C++ host binary and registered into a QoreProgram without dlopen.
//
// Build:
//   LD_LIBRARY_PATH=build ./build/qcc -c -o /tmp/Util.qo qlib/Util.qm
//   g++ -std=c++17 -Iinclude -Lbuild -lqore \
//       examples/aot/qo_link_test.cpp /tmp/Util.qo -o /tmp/qo_link_test
//   LD_LIBRARY_PATH=build /tmp/qo_link_test

#include <qore/Qore.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" void qore_Util_register(QoreProgram*);

int main() {
    qore_init(QL_GPL, "UTF-8", true);

    QoreProgram* pgm = new QoreProgram(PO_NEW_STYLE | PO_STRICT_ARGS);
    qore_Util_register(pgm);

    ExceptionSink xsink;
    ReferenceHolder<QoreListNode> args(new QoreListNode(stringTypeInfo), &xsink);
    args->push(new QoreStringNode("/tmp/foo//bar/"), &xsink);
    ValueHolder result(pgm->callFunction("normalize_dir", *args, &xsink), &xsink);
    if (xsink) {
        xsink.handleExceptions();
        pgm->waitForTerminationAndDeref(&xsink);
        qore_cleanup();
        return 1;
    }

    QoreStringValueHelper s(*result);
    printf("normalize_dir result: %s\n", s ? s->c_str() : "(null)");

    pgm->waitForTerminationAndDeref(&xsink);
    qore_cleanup();
    return 0;
}
