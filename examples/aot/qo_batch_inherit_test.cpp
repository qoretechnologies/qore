// Regression test for the AOT batch phase-sync fixes
// (bfe4e3e2e + 129a15d02).
//
// Problem shape: when `qore_aot_script_begin_batch` /
// `end_batch` register N blobs whose classes form an inheritance
// chain crossing session boundaries, MultiDeserializer's
// resolveAll used to run each session's FULL resolveAll serially
// — so session X's derived class could have its parseCommit
// recurse into session Y's not-yet-deserialized base class (base
// silently committed empty, BCA lost, derived class's ctor
// delegation fails at runtime) AND session X's
// importInheritedMembers could copy an empty member list from
// session Y's base (inherited members never appear in the
// derived class's member_init_list, so initMembers skips them —
// NOTHING-valued inherited member at runtime).
//
// Fix: phase-split resolveAll into named sub-phases and
// interleave them across all sessions.  This harness registers
// blobs in REVERSE dep order (main, Top, Mid, Base) to force the
// cross-session path; the fix makes both invariants hold
// regardless of register order.
//
// Build:
//   LD_LIBRARY_PATH=build ./build/qcc -c --output-dir=build/batch_inherit \
//       examples/aot/batch_inherit_demo/Base.qc \
//       examples/aot/batch_inherit_demo/Mid.qc \
//       examples/aot/batch_inherit_demo/Top.qc \
//       examples/aot/batch_inherit_demo/main.q
//   g++ -std=c++17 -Iinclude -Lbuild \
//       examples/aot/qo_batch_inherit_test.cpp build/batch_inherit/*.qo \
//       -lqore -Wl,-rpath,build -o /tmp/qo_batch_inherit_test
//   LD_LIBRARY_PATH=build /tmp/qo_batch_inherit_test

#include <qore/Qore.h>
#include <qore/QoreAOT.h>

#include <stdio.h>

// Per-file script_register entry points (slice 10d).
extern "C" void qore_Base_Base_script_register(QoreProgram*);
extern "C" void qore_Mid_Mid_script_register(QoreProgram*);
extern "C" void qore_Top_Top_script_register(QoreProgram*);
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

    qore_aot_script_begin_batch(pgm);

    // Register in REVERSE dependency order: main first, then
    // Top → Mid → Base.  The last-registered (Base) is the root
    // of the inheritance chain.  Without the phase-sync fix,
    // Top's session commit would fire before Base/Mid had
    // deserialized methods and members.
    qore_main_main_script_register(pgm);
    qore_Top_Top_script_register(pgm);
    qore_Mid_Mid_script_register(pgm);
    qore_Base_Base_script_register(pgm);

    int flush_rc = qore_aot_script_end_batch(pgm);
    if (flush_rc != 0) {
        fprintf(stderr, "qore_aot_script_end_batch rc=%d\n", flush_rc);
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }

    int rc = qore_run_callable(pgm, "batch_test", nullptr);
    if (rc != 0) {
        fprintf(stderr, "qore_run_callable(\"batch_test\") failed rc=%d\n", rc);
        qore_destroy_program(pgm);
        qore_cleanup();
        return 1;
    }

    printf("batch-inherit-test: registered 4 .qo files in REVERSE dep "
        "order; cross-session ctor delegation + inherited member init "
        "both held under end_batch's interleaved phase sync.\n");

    qore_destroy_program(pgm);
    qore_cleanup();
    return 0;
}
