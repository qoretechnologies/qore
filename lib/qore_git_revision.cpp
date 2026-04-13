/*
    qore_git_revision.cpp

    Compiled as a separate translation unit so that git revision changes
    only trigger a recompile of this tiny file, not the entire
    single-compilation-unit build.

    Copyright (C) 2026 Qore Technologies, s.r.o.
*/

#include "qore/intern/git-revision.h"

extern "C" {
    const char* qore_git_hash = BUILD;
}
