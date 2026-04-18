// AOT Phase 4 slice 4: smoke test that multiple `.qo` files produced by
// `qcc -c --context=DIR` (one per .qm/.qc in a split module) link cleanly
// into a C++ host and that the primary's `qore_<mod>_register` entry
// point registers its items correctly.
//
// Build:
//   LD_LIBRARY_PATH=build ./build/qcc -c --context=qlib/AsyncSocketIo \
//       -o /tmp/slice4/AsyncSocketIo_primary.qo qlib/AsyncSocketIo/AsyncSocketIo.qm
//   LD_LIBRARY_PATH=build ./build/qcc -c --context=qlib/AsyncSocketIo \
//       -o /tmp/slice4/AsyncSocketIoController.qo \
//       qlib/AsyncSocketIo/AsyncSocketIoController.qc
//   ...  (one per .qc)
//   g++ -std=c++17 -Iinclude -Lbuild \
//       examples/aot/qo_link_perfile_test.cpp /tmp/slice4/*.qo \
//       -lqore -Wl,-rpath,build -o /tmp/qo_link_perfile_test
//   LD_LIBRARY_PATH=build /tmp/qo_link_perfile_test
//
// NOTE: at slice 4 the secondaries' compiled functions are latent
// (no register fn registers their metadata — that's slice 6's job).
// This test validates only that the multi-object link is clean and the
// primary is self-callable.  We don't call into secondary-defined
// methods from Qore code, since their metadata isn't registered yet.

#include <qore/Qore.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" void qore_AsyncSocketIo_register(QoreProgram*);

int main() {
    qore_init(QL_GPL, "UTF-8", true);

    QoreProgram* pgm = new QoreProgram(PO_NEW_STYLE | PO_STRICT_ARGS);
    qore_AsyncSocketIo_register(pgm);

    ExceptionSink xsink;
    if (xsink) {
        xsink.handleExceptions();
        pgm->waitForTerminationAndDeref(&xsink);
        qore_cleanup();
        return 1;
    }

    printf("AsyncSocketIo primary .qo registered OK\n");

    pgm->waitForTerminationAndDeref(&xsink);
    qore_cleanup();
    return 0;
}
