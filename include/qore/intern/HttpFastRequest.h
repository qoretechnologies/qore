/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    HttpFastRequest.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#ifndef _QORE_INTERN_HTTPFASTREQUEST_H
#define _QORE_INTERN_HTTPFASTREQUEST_H

// The strict request parser of the HTTP server fast routes (see design/http-server-fast-routes.md, phase 1.5).
//
// Like the rest of the fast route core (HttpFastRoutes.h) this file uses only the standard library and
// qore/common.h: it runs on the async I/O threads, where nothing may run Qore code, allocate unboundedly or block.

#include <qore/common.h>

#include <cstddef>

#include "qore/intern/HttpFastRoutes.h"

//! Reads the header of a request straight from its bytes into an HttpFastRequestView, without building a hash
/** The parser accepts only a conservative subset of HTTP/1.1: a request line of exactly
    <tt>GET &lt;target&gt; HTTP/1.1</tt> with CRLF line ends, a target of unreserved and sub-delimiter characters that
    starts with <tt>/</tt> (no percent-encoding, so that the path means the same to every parser), header lines of a
    token name, a colon and a value of visible ASCII (no obsolete line folding, no whitespace before the colon, no
    control or non-ASCII byte, no bare CR or LF, no NUL), exactly one \c Host header, and at most one each of the
    headers that decide the framing and the connection.  Anything else is rejected and takes the normal parser, which
    decides what to do with it, including the \c 400 response.

    The safety rule is one-directional: the parser must never accept what the normal parser rejects or reads
    differently.  It may reject what the normal parser accepts.
*/
class HttpFastRequestParser {
public:
    //! The largest header (through the empty line) that is parsed; a larger one is rejected
    static constexpr size_t MaxHeaderSize = 8192;

    //! The most header lines that are parsed
    static constexpr size_t MaxHeaders = 64;

    //! The longest request target
    static constexpr size_t MaxTargetSize = 2048;

    //! Parses a request header
    /** @param data the bytes of the header, through the empty line that ends it (CRLF CRLF) and not beyond
        @param size the number of bytes
        @param view set to the method, path, version and the headers that matter for the decision; the views refer
        to \a data.  \c pending is not set.

        @return true if the header is accepted; false if it is rejected, in which case \a view is not valid
    */
    DLLLOCAL static bool parse(const char* data, size_t size, HttpFastRequestView& view);
};

#endif
