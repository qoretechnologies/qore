// AOT Phase 4 slice 8: minimal C-API host for a `.qoa` archive.
//
// Demonstrates the intended qorus-core integration pattern: the host
// translation unit pulls ONLY libqore's public C ABI, no
// `intern/` headers, no direct C++ `QoreProgram` methods.  Every
// interaction with Qore goes through the narrow surface declared in
// `qore/QoreAOT.h` (`qore_create_program`, `qore_destroy_program`,
// `qore_parse_source_string`, `qore_parse_commit`, `qore_run_callable`,
// `qore_last_error`) plus the existing `qore_init` / `qore_cleanup`
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

int main() {
    qore_init(QL_GPL, "UTF-8", true);

    // PO_ALLOW_REPARSE so we can stage additional source after the
    // archive's register_all commits its initial parse.
    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_ALLOW_REPARSE);
    if (!pgm) {
        fprintf(stderr, "qore_create_program failed (did qore_init run?)\n");
        qore_cleanup();
        return 1;
    }

    qore_qoa_register_all(pgm);

    // Stage a tiny driver function that exercises a class defined in
    // a secondary `.qc` of the archived module, using the public C
    // parse API (slice 9).  The host never reaches into C++ — parsing,
    // committing, and dispatching all go through the narrow C ABI.
    const char* src =
        "%new-style\n"
        "sub qoa_link_test_drive() {\n"
        "    AsyncSocketIo::AsyncSocketIoController ctl();\n"
        "    ctl.stop();\n"
        "}\n";
    if (qore_parse_source_string(pgm, src, "<qoa_link_test>") != 0) {
        fprintf(stderr, "qore_parse_source_string failed: %s\n",
            qore_last_error(pgm));
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }
    if (qore_parse_commit(pgm) != 0) {
        fprintf(stderr, "qore_parse_commit failed: %s\n",
            qore_last_error(pgm));
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }

    int rc = qore_run_callable(pgm, "qoa_link_test_drive", nullptr);
    if (rc != 0) {
        fprintf(stderr, "qore_run_callable returned %d: %s\n",
            rc, qore_last_error(pgm));
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
