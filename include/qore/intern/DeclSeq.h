/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    DeclSeq.h

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
*/

#ifndef _QORE_INTERN_DECLSEQ_H

#define _QORE_INTERN_DECLSEQ_H

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>

//! returns the next declaration sequence number
/** The items that a namespace holds in hash maps (namespaces, functions, enums, the methods of a class) are
    enumerated in the order of their declaration, which the maps themselves do not keep: their iteration order is not
    defined, and a copy inserts the items in the order of the map that it copies, which reverses a hash map that is
    iterated (issue #7169, #7171).  Each item takes a sequence number from here when it is created, and a copy takes
    the number of the item that it is a copy of.
*/
inline uint64_t qore_next_decl_seq() {
    static std::atomic<uint64_t> seq{0};
    return seq.fetch_add(1, std::memory_order_relaxed);
}

//! returns the values of a map, or other range of pairs, sorted by the declaration sequence number that \a seq returns for each
template <typename Range, typename T, typename Seq>
inline std::vector<T> qore_decl_ordered(const Range& range, Seq seq) {
    std::vector<T> v;
    v.reserve(range.size());
    for (auto& i : range) {
        v.push_back(i.second);
    }
    std::stable_sort(v.begin(), v.end(), [&seq](const T& a, const T& b) { return seq(a) < seq(b); });
    return v;
}

#endif
