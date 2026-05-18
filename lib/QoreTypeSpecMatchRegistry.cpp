/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreTypeSpecMatchRegistry.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

    Note that the Qore library is dual-licensed under LGPL and MIT licenses; see
    LICENSE.LGPL and LICENSE.MIT in the source code directory for details.
*/

#include <qore/Qore.h>
#include "qore/intern/QoreTypeInfo.h"
#include "qore/intern/QoreTypeSpecMatchRegistry.h"

#include <array>
#include <string>

// Defined in QoreTypeInfo.cpp — used by match handlers
DLLLOCAL qore_type_result_e match_type(const QoreTypeInfo* this_type, const QoreTypeInfo* that_type,
    bool& may_not_match, bool& may_need_filter);

// Include all handler functions
#include "QoreTypeSpecMatchHandlers.cpp"

// Registry table: maps q_typespec_t to handler function
const QoreTypeSpecMatchHandlerInfo QORE_TYPE_SPEC_MATCH_REGISTRY[] = {
    {
        "QTS_TYPE",
        QTS_TYPE,
        match_QTS_TYPE,
        "Basic types (NT_*), empty list, empty hash - dispatches to checkMatchType with reference fallback"
    },
    {
        "QTS_CLASS",
        QTS_CLASS,
        match_QTS_CLASS,
        "Class type matching with object/reference fallback"
    },
    {
        "QTS_HASHDECL",
        QTS_HASHDECL,
        match_QTS_HASHDECL,
        "Hashdecl type matching with ancestry checks and hash/reference fallback"
    },
    {
        "QTS_HARDREF",
        QTS_HARDREF,
        match_QTS_HARDREF,
        "Hard reference type - delegates to checkMatchType"
    },
    {
        "QTS_COMPLEXHASH",
        QTS_COMPLEXHASH,
        match_QTS_COMPLEXHASH,
        "Complex hash type (hash<T>) - element type matching"
    },
    {
        "QTS_COMPLEXLIST",
        QTS_COMPLEXLIST,
        match_QTS_COMPLEXLIST,
        "Complex list type (list<T>) - element type matching, handles both COMPLEXLIST and COMPLEXSOFTLIST"
    },
    {
        "QTS_COMPLEXHARDREF",
        QTS_COMPLEXHARDREF,
        match_QTS_COMPLEXHARDREF,
        "Complex hard reference type - type acceptance matching"
    },
    {
        "QTS_COMPLEXREF",
        QTS_COMPLEXREF,
        match_QTS_COMPLEXREF,
        "Complex reference type - subtype matching with empty list/hash handling"
    },
    {
        "QTS_COMPLEXSOFTLIST",
        QTS_COMPLEXSOFTLIST,
        match_QTS_COMPLEXLIST,
        "Complex soft list type (softlist<T>) - shared handler with COMPLEXLIST"
    },
    {
        "QTS_EMPTYLIST",
        QTS_EMPTYLIST,
        match_QTS_TYPE,
        "Empty list type - shared handler with QTS_TYPE"
    },
    {
        "QTS_EMPTYHASH",
        QTS_EMPTYHASH,
        match_QTS_TYPE,
        "Empty hash type - shared handler with QTS_TYPE"
    },
    {
        "QTS_ENUM",
        QTS_ENUM,
        match_QTS_ENUM,
        "Enum type - same-enum-declaration identity check"
    },
    {
        "QTS_PARAMCLASS",
        QTS_PARAMCLASS,
        match_QTS_PARAMCLASS,
        "Parameterized class type - invariant type-argument matching with object/reference fallback"
    },
    {
        "QTS_TYPEPARAM",
        QTS_TYPEPARAM,
        match_QTS_TYPEPARAM,
        "Symbolic generic class type-parameter placeholder"
    },
    {
        "QTS_WILDCARD",
        QTS_WILDCARD,
        match_QTS_WILDCARD,
        "Use-site generic wildcard type argument"
    },
    { nullptr, (q_typespec_t)0, nullptr, nullptr } // sentinel
};

const QoreTypeSpecMatchHandlerInfo* getQoreTypeSpecMatchHandler(q_typespec_t typespec) {
    for (const QoreTypeSpecMatchHandlerInfo* info = QORE_TYPE_SPEC_MATCH_REGISTRY; info->name; ++info) {
        if (info->typespec == typespec) {
            return info;
        }
    }
    return nullptr;
}

bool qore_type_spec_validate_match_registry(std::string& error) {
    std::array<bool, QORE_TYPE_SPEC_COUNT> seen{};
    for (const QoreTypeSpecMatchHandlerInfo* info = QORE_TYPE_SPEC_MATCH_REGISTRY; info->name; ++info) {
        if (!info->name || !*info->name) {
            error = "type-spec match registry contains an unnamed entry";
            return false;
        }
        if (!info->description || !*info->description) {
            error = "type-spec match registry entry '" + std::string(info->name) + "' has no description";
            return false;
        }
        if (!info->handler) {
            error = "type-spec match registry entry '" + std::string(info->name) + "' has no handler";
            return false;
        }
        unsigned typespec = static_cast<unsigned>(info->typespec);
        if (typespec >= QORE_TYPE_SPEC_COUNT) {
            error = "type-spec match registry entry '" + std::string(info->name)
                + "' uses invalid type-spec id " + std::to_string(typespec);
            return false;
        }
        if (seen[typespec]) {
            error = "duplicate type-spec match registry entry for type-spec id " + std::to_string(typespec)
                + " ('" + std::string(info->name) + "')";
            return false;
        }
        seen[typespec] = true;
    }

    for (unsigned i = 0; i < QORE_TYPE_SPEC_COUNT; ++i) {
        if (!seen[i]) {
            error = "type-spec match registry is missing type-spec id " + std::to_string(i);
            return false;
        }
    }
    return true;
}
