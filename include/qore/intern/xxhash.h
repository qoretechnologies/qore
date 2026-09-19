// Copyright (C) 2026 Qore Technologies, s.r.o.
// MIT License
// xxHash 0.8.4: expose stack-allocated streaming states used by Qore's AOT checksums.
// The upstream header is kept verbatim; this internal interface is not installed.
#pragma once

// Qore's existing hash contract uses XXH32 and XXH64 only.
#define XXH_NO_XXH3
#define XXH_STATIC_LINKING_ONLY
#include <qore/intern/xxhash/xxhash.h>

struct qore_hash_str {
    using is_transparent = void;

    DLLLOCAL size_t operator()(const char* s1) const {
#if TARGET_BITS == 64
        return XXH64(s1, strlen(s1), 0);
#else
        return XXH32(s1, strlen(s1), 0);
#endif
    }

    DLLLOCAL size_t operator()(const qore_prehashed_str& key) const {
        return key.hash;
    }
};
