/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreIRTypeProfile.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

//! @file QoreIRTypeProfile.h the inline recording of the IR guard type profiles, for the library only
/** Not in include/qore/intern: code outside of the library that includes the IR headers (the AOT IR test programs)
    cannot link the internal functions of the library that the slow path calls (issue #6421)
*/

#ifndef _QORE_LIB_QOREIRTYPEPROFILE_H
#define _QORE_LIB_QOREIRTYPEPROFILE_H

#include "qore/intern/QoreIR.h"

inline void TypeProfile::recordTypeInfo(const QoreTypeInfo* type_info) {
    if (type_info) {
        TypeProfileTypeKey key{type_info, qore_get_type_identity_serial(type_info)};
        bool counted = false;
        {
            std::lock_guard<std::mutex> lock(extended_mutex);
            if (extended_type_counts) {
                auto i = extended_type_counts->find(key);
                if (i != extended_type_counts->end()) {
                    ++i->second;
                    counted = true;
                }
            }
        }
        if (!counted) {
            recordNewTypeInfo(key);
        }
    }
    other_count.fetch_add(1, std::memory_order_relaxed);
}

inline void TypeProfile::record(const QoreValue& v) {
    switch (v.getType()) {
        case NT_INT:
            int_count.fetch_add(1, std::memory_order_relaxed);
            break;
        case NT_FLOAT:
            float_count.fetch_add(1, std::memory_order_relaxed);
            break;
        case NT_STRING:
            string_count.fetch_add(1, std::memory_order_relaxed);
            break;
        case NT_BOOLEAN:
            bool_count.fetch_add(1, std::memory_order_relaxed);
            break;
        case NT_NOTHING:
            nothing_count.fetch_add(1, std::memory_order_relaxed);
            break;
        case NT_PLUGIN_VALUE: {
            std::string module_name;
            uint16_t local_type_id = 0;
            const QoreTypeInfo* type_info = nullptr;
            if (qore_plugin_get_value_profile_info(v.getInternalNode(), module_name, local_type_id,
                    type_info)) {
                recordPluginTypeInfo(std::move(module_name), local_type_id, type_info);
            } else {
                recordTypeInfo(v.getFullTypeInfo());
            }
            break;
        }
        default:
            recordTypeInfo(v.getFullTypeInfo());
            break;
    }
}

#endif
