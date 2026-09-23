// Copyright (C) 2026 Qore Technologies, s.r.o.
// MIT License
// Compile the vendored xxHash implementation once, including in unity builds.
// Only the existing XXH32/XXH64 algorithms are part of Qore's hash implementation.
#define XXH_NO_XXH3
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include <qore/intern/xxhash/xxhash.h>
#undef XXH_IMPLEMENTATION
