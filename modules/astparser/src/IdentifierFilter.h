/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  IdentifierFilter.h

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

#ifndef _QLS_IDENTIFIERFILTER_H
#define _QLS_IDENTIFIERFILTER_H

#include <cstddef>
#include <string>

class AstParseResult;
class CSTCancelCheck;

//! A Bloom filter of the identifiers in a syntax tree
/** The filter answers whether an identifier can occur in the tree without keeping the tree or its source: it has
    no false negatives and about 1% false positives.  The filter contains the full text of each identifier-like
    node (as compared by reference searches) and each \c "::"-separated component of scoped identifiers.

    Serialized format: a 32-bit little-endian bit count followed by the bits, least significant bit first.
*/
class IdentifierFilter {
public:
    //! The number of hash probes per identifier
    static constexpr unsigned Probes = 7;

    //! The number of filter bits per distinct identifier
    static constexpr unsigned BitsPerItem = 10;

    //! The minimum number of filter bits
    static constexpr unsigned MinBits = 64;

    //! The size of the bit count header in bytes
    static constexpr size_t HeaderSize = 4;

    //! Builds the serialized filter for the identifiers in a parse result
    /** @param result the parse result
        @param cancel the cancellation check
        @param out receives the serialized filter

        @return false if the operation was cancelled
    */
    static bool build(const AstParseResult* result, CSTCancelCheck& cancel, std::string& out);

    //! Returns true if the identifier can occur in the tree that the serialized filter was built from
    /** @param data the serialized filter
        @param size the size of the serialized filter in bytes
        @param name the identifier to test
        @param valid set to false if the data is not a serialized filter

        @return true if the identifier can occur in the tree; false if it does not occur or the data is invalid
    */
    static bool contains(const void* data, size_t size, const std::string& name, bool& valid);
};

#endif // _QLS_IDENTIFIERFILTER_H
