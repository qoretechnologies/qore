/* -*- mode: c++; indent-tabs-mode: nil -*- */
// Copyright (C) 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: LGPL-2.1-or-later

// Link check (issue #7260): code built with the internal headers can emit the virtual methods of UserSignature that
// it calls directly.  The qualified calls below force those references at every optimization level, so this program
// links only if the methods do not depend on symbols that are hidden in libqore.

#include <qore/Qore.h>
#include <qore/intern/Function.h>

static const QoreTypeInfo* callParseGetReturnTypeInfo(const UserSignature& signature, int& err) {
    return signature.UserSignature::parseGetReturnTypeInfo(err);
}

static void callDestructor(UserSignature* signature) {
    signature->UserSignature::~UserSignature();
}

int main() {
    // through volatile pointers, so that the functions and their references are kept
    const QoreTypeInfo* (* volatile get_type)(const UserSignature&, int&) = callParseGetReturnTypeInfo;
    void (* volatile destroy)(UserSignature*) = callDestructor;
    if (!get_type || !destroy) {
        fputs("the UserSignature methods were not linked\n", stderr);
        return 1;
    }
    puts("user signature methods linked");
    return 0;
}
