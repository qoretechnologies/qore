/* Copyright (C) 2026 Qore Technologies, s.r.o.  SPDX-License-Identifier: MIT */
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <dlfcn.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C" void qore_metadata_metadata_script_register(QoreProgram*);

// This host registers its single input on the main thread only.
static bool copy_input;
static int registration_error;
static std::vector<std::vector<unsigned char>> temporary_metadata;

// Interpose the host API to exercise a caller-owned buffer without changing generated AOT code.
extern "C" int qore_aot_script_register(QoreProgram* pgm, const unsigned char* data, int size,
        const char* label, const QoreAOTFunc* functions, int count) {
    using Register = decltype(&qore_aot_script_register);
    auto real_register = reinterpret_cast<Register>(dlsym(RTLD_NEXT, "qore_aot_script_register"));
    if (!real_register) {
        return registration_error = 99;
    }
    if (copy_input) {
        temporary_metadata.emplace_back(data, data + size);
        data = temporary_metadata.back().data();
    }
    int rc = real_register(pgm, data, size, label, functions, count);
    if (rc) {
        registration_error = rc;
    }
    return rc;
}

int main(int argc, char** argv) {
    copy_input = argc > 1 && !strcmp(argv[1], "copy");
    bool batch = argc > 2 && !strcmp(argv[2], "batch");
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(PO_NEW_STYLE | PO_STRICT_ARGS);
    if (!pgm) {
        qore_cleanup();
        return 1;
    }
    if (batch) {
        qore_aot_script_begin_batch(pgm);
    }
    qore_metadata_metadata_script_register(pgm);
    int rc = registration_error;
    if (batch && !rc) {
        rc = qore_aot_script_end_batch(pgm);
    }
    // Poison while the allocations are still live: stale readers fail deterministically, even when the
    // allocator would otherwise leave freed bytes intact. Both immediate and batch readers are gone now.
    for (auto& buffer : temporary_metadata) {
        std::fill(buffer.begin(), buffer.end(), 0xa5);
    }
    if (!rc) {
        rc = qore_run_callable(pgm, "retainedMain", nullptr);
    }
    qore_destroy_program(pgm);
    qore_cleanup();
    if (!rc) {
        puts("RETAINED-METADATA-OK");
    }
    return rc;
}
