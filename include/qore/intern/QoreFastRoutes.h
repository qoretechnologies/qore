/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QoreFastRoutes.h

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

#ifndef _QORE_INTERN_QOREFASTROUTES_H

#define _QORE_INTERN_QOREFASTROUTES_H

#include <qore/Qore.h>
#include <qore/AbstractPrivateData.h>

#include "qore/intern/HttpFastRoutes.h"

//! The private data of a FastCell object: the cell, which routes share
/** This is the Qore-facing side of the fast routes.  It is not part of the isolated core that runs on the I/O threads.
*/
class QoreFastCell : public AbstractPrivateData {
public:
    DLLLOCAL QoreFastCell(int64 initial, int64 max_age_us) : c(std::make_shared<HttpFastCell>(initial, max_age_us)) {
    }

    const std::shared_ptr<HttpFastCell> c;
};

//! The parts of a response description (a hash with \c code, \c body, \c content_type and \c hdr), as C++ data
struct FastRouteResponseParts {
    int64 code = 200;
    std::string body;
    //! true if the body was a string (a charset is added to the content type)
    bool string_body = false;
    std::string content_type;
    std::vector<std::pair<std::string, std::string>> headers;
};

DLLLOCAL void fast_route_error(ExceptionSink* xsink, const char* fmt, ...);
DLLLOCAL bool fast_route_parse_parts(const char* what, QoreValue val, FastRouteResponseParts& parts,
        ExceptionSink* xsink);
DLLLOCAL std::shared_ptr<const HttpFastResponse> fast_route_build_response(const char* what,
        FastRouteResponseParts parts, const std::vector<std::pair<std::string, std::string>>* base, bool normalize,
        ExceptionSink* xsink);

//! The private data of a FastResponseSlot object: the slot, which routes share
/** The slot holds the serialized response.  This object also keeps the parts of the last response and the default
    headers, as C++ data, so that the response can be built again when the defaults are set after it.  The mutex
    serializes the Qore threads that publish; the I/O thread never takes it.
*/
class QoreFastResponseSlot : public AbstractPrivateData {
public:
    DLLLOCAL QoreFastResponseSlot(int64 max_age_us) : s(std::make_shared<HttpFastSlot>(max_age_us)) {
    }

    //! Builds and publishes a response; returns -1 with an exception on error
    DLLLOCAL int publish(FastRouteResponseParts parts, ExceptionSink* xsink) {
        std::lock_guard<std::mutex> l(m);
        std::shared_ptr<const HttpFastResponse> r = fast_route_build_response("response", parts,
            have_defaults ? &base : nullptr, have_defaults, xsink);
        if (!r) {
            return -1;
        }
        s->set(std::move(r));
        last = std::move(parts);
        have_last = true;
        return 0;
    }

    //! Sets the default headers and the 200-without-a-body rule; the current response is built again with them
    DLLLOCAL int setDefaults(std::vector<std::pair<std::string, std::string>> headers, ExceptionSink* xsink) {
        std::lock_guard<std::mutex> l(m);
        if (have_last) {
            std::shared_ptr<const HttpFastResponse> r = fast_route_build_response("response", last, &headers, true,
                xsink);
            if (!r) {
                return -1;
            }
            s->set(std::move(r));
        }
        base = std::move(headers);
        have_defaults = true;
        return 0;
    }

    //! Removes the response
    DLLLOCAL void clear() {
        std::lock_guard<std::mutex> l(m);
        s->set(nullptr);
        have_last = false;
    }

    const std::shared_ptr<HttpFastSlot> s;

private:
    std::mutex m;
    FastRouteResponseParts last;
    bool have_last = false;
    std::vector<std::pair<std::string, std::string>> base;
    bool have_defaults = false;
};

//! The private data of an HttpFastRoutes object: the routes of a listener
class QoreHttpFastRoutes : public AbstractPrivateData {
public:
    DLLLOCAL QoreHttpFastRoutes() : r(std::make_shared<HttpFastRoutes>()) {
    }

    const std::shared_ptr<HttpFastRoutes> r;
};

#endif
