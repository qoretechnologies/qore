// AOT Phase 4 slice 10g: out-of-order batch script register.
//
// This harness deliberately registers main.qo BEFORE lib.qo even
// though main.q references lib.qc's `Helper` class.  Without a batch
// the register call for main.qo would fail — slice 10d runs full
// resolution per-blob, and phase-2 `resolveClassBases` would not
// find Helper when main.qo's shells are being wired up first.
//
// With `qore_aot_script_begin_batch` / `qore_aot_script_end_batch`,
// each register call only loads shells; resolution runs once at
// end_batch after every blob is in.  Inheritance and type refs
// resolve regardless of call order.
//
// Build:
//   LD_LIBRARY_PATH=build ./build/qcc -c --output-dir=build/slice10g \
//       examples/aot/script_demo/lib.qc examples/aot/script_demo/main.q
//   g++ -std=c++17 -Iinclude -Lbuild \
//       examples/aot/qo_script_batch_test.cpp build/slice10g/*.qo \
//       -lqore -Wl,-rpath,build -o /tmp/qo_script_batch_test
//   LD_LIBRARY_PATH=build /tmp/qo_script_batch_test

#include <qore/Qore.h>
#include <qore/QoreAOT.h>

#include <stdio.h>

// Slice 10d per-file entry points (wrappers around
// qore_aot_script_register).
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

    // Enter batch mode: subsequent register calls only do phase 1.
    qore_aot_script_begin_batch(pgm);

    // Register in REVERSE of dependency order.  main.qo uses
    // lib.qc's Helper class; without batch mode this would fail
    // at main.qo's phase-2 resolveClassBases.
    qore_main_main_script_register(pgm);
    qore_lib_lib_script_register(pgm);

    // Flush: cross-blob resolve + per-blob register + init.
    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        fprintf(stderr, "qore_aot_script_end_batch rc=%d\n", flush_rc);
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }

    int rc = qore_run_callable(pgm, "compute", nullptr);
    if (rc != 0) {
        fprintf(stderr, "qore_run_callable(\"compute\") failed rc=%d\n", rc);
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }

    printf("script-batch-test: registered main.qo BEFORE lib.qo "
        "(reverse dep order) via begin/end_batch, invoked compute() "
        "successfully\n");

    qore_destroy_program(pgm);
    qore_cleanup();
    return 0;
}
