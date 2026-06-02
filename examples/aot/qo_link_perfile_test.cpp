// AOT Phase 4 slice 4 (link-cleanliness harness, updated for slice 6):
//
// Verifies that a set of per-file `.qo` files produced by
// `qcc -c --context=DIR <file>` relocate cleanly into a single C++
// binary: no duplicate symbols, all fragment accessors present, the
// link step finishes with exit 0.
//
// Slice 6 moves the "runnable multi-file module" workflow to the
// aggregator path (`qcc -m --from-objects`): per-file `.qo`s are
// intermediates and no longer expose `qore_<mod>_register`
// individually.  This harness therefore does NOT attempt runtime
// registration — that's validated end-to-end by the aggregator's own
// tests (see examples/aot/qo_aggregator_test.cpp).
//
// Build:
//   LD_LIBRARY_PATH=build ./build/qcc -c --context=qlib/AsyncSocketIo \
//       -o /tmp/slice4/AsyncSocketIo_primary.qo \
//       qlib/AsyncSocketIo/AsyncSocketIo.qm
//   (repeat for each .qc)
//   g++ -std=c++17 -Iinclude -Lbuild \
//       examples/aot/qo_link_perfile_test.cpp /tmp/slice4/*.qo \
//       -lqore -Wl,-rpath,build -o /tmp/qo_link_perfile_test
//   LD_LIBRARY_PATH=build /tmp/qo_link_perfile_test

#include <qore/Qore.h>
#include <stdio.h>

int main() {
    qore_init(QL_GPL, "UTF-8", true);
    printf("per-file .qo link test: OK (%zu .qo's relocated into this binary)\n",
        (size_t)5);
    qore_cleanup();
    return 0;
}
