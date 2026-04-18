// AOT Phase 4 slice 10d: C++ host driving a multi-file script app
// built via `qcc -c -L<dir>` per file, linked together with libqore.
//
// Each input .qo exports `qore_<app>_<file>_script_register(QoreProgram*)`
// — a thin wrapper around `qore_aot_script_register` that loads that
// file's metadata + compiled function table into the caller's
// QoreProgram.  The host calls each one in dependency order, then
// drives the resulting program.
//
// This is the pattern a Qorus-core-style C++ host follows once slice
// 11 lands: replace the existing decompress+parsePending loop with a
// short series of script_register calls.
//
// Build:
//   LD_LIBRARY_PATH=build ./build/qcc -c -o /tmp/slice10d/lib.qo lib.qc
//   LD_LIBRARY_PATH=build ./build/qcc -c -L /tmp/slice10d -o /tmp/slice10d/main.qo main.q
//   g++ -std=c++17 -Iinclude -Lbuild \
//       examples/aot/qo_script_test.cpp /tmp/slice10d/*.qo \
//       -lqore -Wl,-rpath,build -o /tmp/qo_script_test
//   LD_LIBRARY_PATH=build /tmp/qo_script_test

#include <qore/Qore.h>
#include <qore/QoreAOT.h>

#include <stdio.h>
#include <stdlib.h>

// Emitted by qcc -c on each input source (slice 10d per-file
// register entries).  Link order: base classes' files first, callers
// after.  For this test: lib.qc defines class Helper, main.q's
// compute() uses it.
extern "C" void qore_lib_lib_script_register(QoreProgram*);
extern "C" void qore_main_main_script_register(QoreProgram*);

int main() {
    qore_init(QL_GPL, "UTF-8", true);

    QoreProgram* pgm = qore_create_program(
        PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
    if (!pgm) {
        fprintf(stderr, "qore_create_program failed\n");
        qore_cleanup();
        return 1;
    }

    // Register in dependency order — lib provides Helper, main uses
    // Helper's method.
    qore_lib_lib_script_register(pgm);
    qore_main_main_script_register(pgm);

    // compute() returns 6 * 7 = 42.  Invoke via the public C API
    // (slice 8).  We just want to confirm the cross-.qo call path
    // works end-to-end; return-value inspection beyond "no
    // exception" is deferred.
    int rc = qore_run_callable(pgm, "compute", nullptr);
    if (rc != 0) {
        fprintf(stderr, "qore_run_callable(\"compute\") failed rc=%d\n", rc);
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }

    printf("script-test: registered lib.qo + main.qo, invoked compute() "
        "successfully\n");

    qore_destroy_program(pgm);
    qore_cleanup();
    return 0;
}
