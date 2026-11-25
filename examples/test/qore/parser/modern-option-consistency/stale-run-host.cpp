/*
    stale-run-host.cpp

    Copyright (C) 2026 Qore Technologies, s.r.o.

    The entry points of an AOT executable: one compiled before issue #5682 can store parse options with %modern and a
    compatibility option that %modern does not allow; it is refused with an error that says to rebuild it, and its
    code does not run (qore_aot_run_v4(), given the AOT runtime identity of the runtime); the entry points of
    executables compiled before the AOT runtime identity check of executables (qore_aot_run(), qore_aot_run_v3()) are
    refused for their identity before that (issue #6898; see modern-option-consistency.qtest)
*/

#include <qore/Qore.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

// the entry points that the main() of an AOT executable calls (not in a public header)
struct QoreAOTFunc;
extern "C" int qore_aot_run(int argc, char** argv, const char* source, int source_len, const char* label,
    int64_t parse_options, const QoreAOTFunc* funcs, int num_funcs);
extern "C" int qore_aot_run_v3(int argc, char** argv, const uint8_t* data, int data_len, const char* label,
    int64_t parse_options_lo, int64_t parse_options_hi, const QoreAOTFunc* funcs, int num_funcs);
extern "C" int qore_aot_run_v4(int argc, char** argv, const char* identity, const uint8_t* data, int data_len,
    const char* label, int64_t parse_options_lo, int64_t parse_options_hi, const QoreAOTFunc* funcs, int num_funcs);
extern "C" const char* qore_aot_runtime_identity;

int main(int argc, char** argv) {
    // the options of a stale executable: %modern and %broken-range
    QoreParseOptions po = QoreParseOptions(PO_MODERN) | QoreParseOptions(PO_BROKEN_RANGE);
    int rc;
    if (argc > 1 && !strcmp(argv[1], "v4")) {
        rc = qore_aot_run_v4(1, argv, qore_aot_runtime_identity, nullptr, 0, "stale",
            static_cast<int64_t>(po.getLo()), static_cast<int64_t>(po.getHi()), nullptr, 0);
    } else if (argc > 1 && !strcmp(argv[1], "v3")) {
        rc = qore_aot_run_v3(1, argv, nullptr, 0, "stale", static_cast<int64_t>(po.getLo()),
            static_cast<int64_t>(po.getHi()), nullptr, 0);
    } else {
        const char* src = "printf(\"STALE-CODE-RAN\\n\");";
        rc = qore_aot_run(1, argv, src, static_cast<int>(strlen(src)), "stale", static_cast<int64_t>(po.getLo()),
            nullptr, 0);
    }
    printf("rc=%d\n", rc);
    return 0;
}
