// AOT Phase 4 slice 7: static linkage test for `.qoa` archives
// produced by `qcc -a --context=DIR *.qo -o <name>.qoa`.
//
// The archive is a standard Unix `ar rcs` static library containing:
//   - per-file .qo objects (one per .qm / .qc / .ql in the split module),
//   - a synthesized glue .o that exports `qore_qoa_register_all(QoreProgram*)`
//     (plus the slice 3 `qore_<mod>_register` entry point).
//
// C++ hosts link the `.qoa` alongside `-lqore` and call
// `qore_qoa_register_all(pgm)` once at startup — no dlopen, no .qm/.qmod
// on disk, no source files shipped.  Matches the intended qorus-core
// integration pattern.
//
// Build:
//   LD_LIBRARY_PATH=build ./build/qcc -c --context=qlib/AsyncSocketIo \
//       -o /tmp/slice7/AsyncSocketIo_primary.qo \
//       qlib/AsyncSocketIo/AsyncSocketIo.qm
//   (repeat for each .qc)
//   LD_LIBRARY_PATH=build ./build/qcc -a --context=qlib/AsyncSocketIo \
//       /tmp/slice7/*.qo -o /tmp/slice7/AsyncSocketIo.qoa
//   g++ -std=c++17 -Iinclude -Lbuild \
//       examples/aot/qoa_link_test.cpp /tmp/slice7/AsyncSocketIo.qoa \
//       -lqore -Wl,-rpath,build -o /tmp/qoa_link_test
//   LD_LIBRARY_PATH=build /tmp/qoa_link_test

#include <qore/Qore.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" void qore_qoa_register_all(QoreProgram*);

int main() {
    qore_init(QL_GPL, "UTF-8", true);

    QoreProgram* pgm = new QoreProgram(PO_NEW_STYLE | PO_STRICT_ARGS);
    qore_qoa_register_all(pgm);

    ExceptionSink xsink;
    if (xsink) {
        xsink.handleExceptions();
        pgm->waitForTerminationAndDeref(&xsink);
        qore_cleanup();
        return 1;
    }

    // Parse a tiny Qore fragment that exercises a class from a secondary .qc
    // to confirm the archive wires up cross-file symbols correctly.
    const char* drive_src =
        "%new-style\n"
        "AsyncSocketIo::AsyncSocketIoController ctl();\n"
        "ctl.stop();\n";
    pgm->parse(drive_src, "<qoa_link_test>", &xsink);
    if (xsink) {
        xsink.handleExceptions();
        pgm->waitForTerminationAndDeref(&xsink);
        qore_cleanup();
        return 1;
    }
    pgm->run(&xsink);
    if (xsink) {
        xsink.handleExceptions();
        pgm->waitForTerminationAndDeref(&xsink);
        qore_cleanup();
        return 1;
    }

    printf("qoa link test: AsyncSocketIo.qoa registered + class from .qc "
        "constructed + method from .qc invoked\n");

    pgm->waitForTerminationAndDeref(&xsink);
    qore_cleanup();
    return 0;
}
