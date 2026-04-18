// AOT Phase 4 slice 8: minimal C-API host for a `.qoa` archive.
//
// Demonstrates the intended qorus-core integration pattern: the host
// translation unit pulls ONLY libqore's public C ABI, no
// `intern/` headers, no direct C++ `QoreProgram` methods.  Every
// interaction with Qore goes through the narrow surface declared in
// `qore/QoreAOT.h` (`qore_create_program`, `qore_destroy_program`,
// `qore_run_callable`) plus the existing `qore_init` / `qore_cleanup`
// from `qore/common.h`, plus the user-provided `qore_qoa_register_all`
// that `qcc -a` emitted into the `.qoa`.
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

// Pull in qore_init / qore_cleanup (they live in the umbrella
// <qore/Qore.h>, not common.h).  Everything driving the program
// afterwards goes through the narrow public C ABI in <qore/QoreAOT.h>.
#include <qore/Qore.h>
#include <qore/QoreAOT.h>

#include <stdio.h>
#include <stdlib.h>

// Emitted by qcc -a into the .qoa archive.  One public entry point
// per binary; calling it registers every module packaged by the
// archive into the given program.
extern "C" void qore_qoa_register_all(QoreProgram* pgm);

// Defined at the bottom of this file — parsing still requires C++
// QoreProgram::parse() today.  Slice 9 will evaluate whether to grow
// the public C API to cover parsing too.
extern "C" int qoa_link_test_parse_stage(QoreProgram* pgm);

int main() {
    qore_init(QL_GPL, "UTF-8", true);

    QoreProgram* pgm = qore_create_program(PO_NEW_STYLE | PO_STRICT_ARGS);
    if (!pgm) {
        fprintf(stderr, "qore_create_program failed (did qore_init run?)\n");
        qore_cleanup();
        return 1;
    }

    qore_qoa_register_all(pgm);

    // Stage a tiny driver function written in Qore that exercises a
    // class defined in a secondary `.qc` of the archived module.  We
    // lean on the standard library's `call_function` by bouncing the
    // driver through a top-level function — the public C ABI only
    // exposes `qore_run_callable` for named functions, not raw parsing.
    //
    // NB: parsing is still a C++-only API today (`QoreProgram::parse`
    // takes an `ExceptionSink&`).  Hosts that need to mix source and
    // pre-built modules continue to reach into C++.  The archive
    // itself has already registered every class/function we need;
    // this test only needs a trivial entry fn, which qcc/qrun usually
    // supplies — for slice 8 we embed one via the C++ parse API so
    // the slice 9 qorus-core trial can validate which host patterns
    // actually work through the C ABI alone.  Keep the C++ include
    // local to this staging step.
    if (qoa_link_test_parse_stage(pgm) != 0) {
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }

    int rc = qore_run_callable(pgm, "qoa_link_test_drive", nullptr);
    if (rc != 0) {
        fprintf(stderr, "qore_run_callable returned %d\n", rc);
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }

    printf("qoa link test: AsyncSocketIo.qoa registered + class from .qc "
        "constructed + method from .qc invoked (via public C API)\n");

    qore_destroy_program(pgm);
    qore_cleanup();
    return 0;
}

// ---------------------------------------------------------------------------
// Staging helper — NOT part of the public C API.  Parsing still requires
// a C++ include today (`QoreProgram::parse` is a member fn).  Keeping the
// C++ surface here isolated from main() makes the boundary clear: a host
// that only wants to CALL pre-registered functions can skip this entirely.
// ---------------------------------------------------------------------------
extern "C" int qoa_link_test_parse_stage(QoreProgram* pgm) {
    const char* src =
        "%new-style\n"
        "sub qoa_link_test_drive() {\n"
        "    AsyncSocketIo::AsyncSocketIoController ctl();\n"
        "    ctl.stop();\n"
        "}\n";
    ExceptionSink xsink;
    pgm->parse(src, "<qoa_link_test>", &xsink);
    if (xsink.isException()) {
        xsink.handleExceptions();
        return 1;
    }
    return 0;
}
