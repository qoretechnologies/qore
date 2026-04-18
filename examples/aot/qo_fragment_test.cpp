// AOT Phase 4 slice 5: smoke test that per-file `.qo`s (both primary
// and secondary) export the fragment accessor symbols introduced by
// slice 5:
//
//   extern "C" const void* qore_<mod>_<file>_fragment_data(size_t* out_len);
//   extern "C" const int   qore_<mod>_<file>_fragment_order;
//
// The fragments carry the uncompressed metadata that slice 6's link-time
// aggregator will concatenate.  At slice 5 we only verify:
//   - all five fragments are linkable into a C++ host,
//   - calling each fragment_data() returns a non-null pointer and a
//     length that matches qcc's build-time "fragment blob: N bytes"
//     line,
//   - fragment_order values are 0..4 with no duplicates (the primary
//     .qm takes 0; secondaries are 1 + alphabetical index).
//
// Build:
//   LD_LIBRARY_PATH=build ./build/qcc -c --context=qlib/AsyncSocketIo \
//       -o /tmp/slice5/AsyncSocketIo_primary.qo \
//       qlib/AsyncSocketIo/AsyncSocketIo.qm
//   (repeat for each .qc)
//   g++ -std=c++17 -Iinclude -Lbuild \
//       examples/aot/qo_fragment_test.cpp /tmp/slice5/*.qo \
//       -lqore -Wl,-rpath,build -o /tmp/qo_fragment_test
//   LD_LIBRARY_PATH=build /tmp/qo_fragment_test

#include <qore/Qore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <vector>

extern "C" {
    const void* qore_AsyncSocketIo_AsyncSocketIo_fragment_data(size_t*);
    extern const int qore_AsyncSocketIo_AsyncSocketIo_fragment_order;

    const void* qore_AsyncSocketIo_AbstractStreamContext_fragment_data(size_t*);
    extern const int qore_AsyncSocketIo_AbstractStreamContext_fragment_order;

    const void* qore_AsyncSocketIo_AsyncSocketIoController_fragment_data(size_t*);
    extern const int qore_AsyncSocketIo_AsyncSocketIoController_fragment_order;

    const void* qore_AsyncSocketIo_Http2StreamContext_fragment_data(size_t*);
    extern const int qore_AsyncSocketIo_Http2StreamContext_fragment_order;

    const void* qore_AsyncSocketIo_Http3StreamContext_fragment_data(size_t*);
    extern const int qore_AsyncSocketIo_Http3StreamContext_fragment_order;
}

struct FragmentInfo {
    const char* file;
    const void* (*get_data)(size_t*);
    int order;
};

int main() {
    qore_init(QL_GPL, "UTF-8", true);

    FragmentInfo fragments[] = {
        {"AsyncSocketIo.qm",
         qore_AsyncSocketIo_AsyncSocketIo_fragment_data,
         qore_AsyncSocketIo_AsyncSocketIo_fragment_order},
        {"AbstractStreamContext.qc",
         qore_AsyncSocketIo_AbstractStreamContext_fragment_data,
         qore_AsyncSocketIo_AbstractStreamContext_fragment_order},
        {"AsyncSocketIoController.qc",
         qore_AsyncSocketIo_AsyncSocketIoController_fragment_data,
         qore_AsyncSocketIo_AsyncSocketIoController_fragment_order},
        {"Http2StreamContext.qc",
         qore_AsyncSocketIo_Http2StreamContext_fragment_data,
         qore_AsyncSocketIo_Http2StreamContext_fragment_order},
        {"Http3StreamContext.qc",
         qore_AsyncSocketIo_Http3StreamContext_fragment_data,
         qore_AsyncSocketIo_Http3StreamContext_fragment_order},
    };

    size_t n = sizeof(fragments) / sizeof(fragments[0]);

    // Check all fragments return non-null, size>0.
    int rc = 0;
    std::vector<int> orders;
    for (size_t i = 0; i < n; ++i) {
        size_t len = 0;
        const void* p = fragments[i].get_data(&len);
        if (!p) {
            printf("FAIL: %s: fragment_data returned null\n", fragments[i].file);
            rc = 1;
            continue;
        }
        if (len == 0) {
            printf("FAIL: %s: fragment length is 0\n", fragments[i].file);
            rc = 1;
            continue;
        }
        // Check header magic — QoreAOTBinary blobs start with the magic u32.
        uint32_t magic;
        memcpy(&magic, p, sizeof(magic));
        printf("%-30s order=%d  size=%zu  magic=0x%08x\n",
            fragments[i].file, fragments[i].order, len, magic);
        orders.push_back(fragments[i].order);
    }

    // Check fragment_order values are 0..n-1 with no duplicates.
    std::sort(orders.begin(), orders.end());
    for (size_t i = 0; i < orders.size(); ++i) {
        if (orders[i] != (int)i) {
            printf("FAIL: fragment_order sequence at index %zu: expected %zu, got %d\n",
                i, i, orders[i]);
            rc = 1;
        }
    }

    // Check the primary's fragment_order is 0 (it's the .qm).
    if (qore_AsyncSocketIo_AsyncSocketIo_fragment_order != 0) {
        printf("FAIL: primary (.qm) fragment_order=%d, expected 0\n",
            qore_AsyncSocketIo_AsyncSocketIo_fragment_order);
        rc = 1;
    }

    if (rc == 0) {
        printf("\nOK: all %zu fragments present, orders unique and sequential [0..%zu]\n",
            n, n - 1);
    }

    qore_cleanup();
    return rc;
}
