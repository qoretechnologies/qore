/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  IdentifierFilter.cpp

  Qore AST Parser

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

#include "IdentifierFilter.h"

#include <cstdint>
#include <cstring>
#include <unordered_set>

#include "AstParser.h"
#include "CSTSearcher.h"
#include "CSTWalk.h"

namespace identifier_filter {

//! Returns a 64-bit hash of the string: FNV-1a followed by the splitmix64 finalizer to spread the bits
uint64_t hash64(const char* str, size_t len) {
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<unsigned char>(str[i]);
        h *= 1099511628211ULL;
    }
    h ^= h >> 30;
    h *= 0xbf58476d1ce4e5b9ULL;
    h ^= h >> 27;
    h *= 0x94d049bb133111ebULL;
    h ^= h >> 31;
    return h;
}

//! Calls the function with the bit position of each probe for the string in a filter of the given size
template <typename F>
void forEachProbe(const std::string& str, uint32_t bits, F f) {
    uint64_t h = hash64(str.data(), str.size());
    uint64_t h1 = h & 0xffffffffULL;
    // an odd step visits distinct positions for any filter size
    uint64_t h2 = (h >> 32) | 1;
    for (unsigned i = 0; i < IdentifierFilter::Probes; ++i) {
        f(static_cast<uint32_t>((h1 + i * h2) % bits));
    }
}

//! Adds the identifier and, for scoped identifiers, each "::"-separated component
void addIdentifier(std::unordered_set<std::string>& names, const std::string& text) {
    if (text.empty()) {
        return;
    }
    names.insert(text);
    size_t start = 0;
    while (true) {
        size_t sep = text.find("::", start);
        if (sep == std::string::npos) {
            break;
        }
        if (sep > start) {
            names.insert(text.substr(start, sep - start));
        }
        start = sep + 2;
    }
    if (start && start < text.size()) {
        names.insert(text.substr(start));
    }
}

} // namespace identifier_filter

bool IdentifierFilter::build(const AstParseResult* result, CSTCancelCheck& cancel, std::string& out) {
    std::unordered_set<std::string> names;
    if (result) {
        bool ok = cst_walk(result->getRootNode(), cancel, [&](const TSTreeCursor*, TSNode node, uint32_t) {
            if (CSTSearcher::isIdentifierLikeNode(ts_node_type(node))) {
                identifier_filter::addIdentifier(names, result->getNodeText(node));
                // the components of scoped identifiers were added from the node text
                return CSTWalkAction::Skip;
            }
            return CSTWalkAction::Descend;
        });
        if (!ok) {
            return false;
        }
    }

    uint64_t bits64 = static_cast<uint64_t>(names.size()) * BitsPerItem;
    uint32_t bits = bits64 < MinBits ? MinBits : (bits64 > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(bits64));
    out.assign(HeaderSize + (static_cast<size_t>(bits) + 7) / 8, '\0');
    for (size_t i = 0; i < HeaderSize; ++i) {
        out[i] = static_cast<char>((bits >> (8 * i)) & 0xff);
    }
    unsigned char* data = reinterpret_cast<unsigned char*>(&out[HeaderSize]);
    for (const std::string& name : names) {
        // the check calls qore_check_cancel() every CSTCancelCheck::Interval calls
        if (cancel()) {
            return false;
        }
        identifier_filter::forEachProbe(name, bits, [data](uint32_t bit) {
            data[bit / 8] |= static_cast<unsigned char>(1u << (bit % 8));
        });
    }
    return true;
}

bool IdentifierFilter::contains(const void* data, size_t size, const std::string& name, bool& valid) {
    valid = false;
    if (size < HeaderSize) {
        return false;
    }
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    uint32_t bits = 0;
    for (size_t i = 0; i < HeaderSize; ++i) {
        bits |= static_cast<uint32_t>(bytes[i]) << (8 * i);
    }
    if (bits < MinBits || size != HeaderSize + (static_cast<size_t>(bits) + 7) / 8) {
        return false;
    }
    valid = true;
    const unsigned char* filter = bytes + HeaderSize;
    bool found = true;
    identifier_filter::forEachProbe(name, bits, [filter, &found](uint32_t bit) {
        if (!(filter[bit / 8] & (1u << (bit % 8)))) {
            found = false;
        }
    });
    return found;
}
