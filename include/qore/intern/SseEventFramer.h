/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SseEventFramer.h

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

#ifndef _QORE_INTERN_SSEEVENTFRAMER_H
#define _QORE_INTERN_SSEEVENTFRAMER_H

#include <string>

//! Splits a server-sent event stream into events one byte at a time
/** Line endings (CRLF, LF, or CR) are normalized to LF, and an event ends with an empty line, as with
    Socket::readServerSentEvent().  A CR that ends an event is remembered, so that the LF of a CRLF split across
    events is not taken as an empty line of the next event.
*/
class SseEventFramer {
public:
    //! Adds a byte of the stream
    /** @return true if the event is complete; see take()
    */
    DLLLOCAL bool add(char c) {
        if (got_cr) {
            got_cr = false;
            if (c == '\n') {
                return false;
            }
        }
        if (c == '\r') {
            event += '\n';
            got_cr = true;
            return ++eol_count == 2;
        }
        if (c == '\n') {
            event += '\n';
            return ++eol_count == 2;
        }
        eol_count = 0;
        event += c;
        return false;
    }

    //! Returns the size of the event read so far in bytes
    DLLLOCAL size_t size() const {
        return event.size();
    }

    //! Returns the event read so far and starts the next one
    DLLLOCAL std::string take() {
        std::string rv;
        rv.swap(event);
        eol_count = 0;
        return rv;
    }

    //! Resets the framer for a new stream
    DLLLOCAL void reset() {
        event.clear();
        eol_count = 0;
        got_cr = false;
    }

private:
    std::string event;
    int eol_count = 0;
    bool got_cr = false;
};

#endif
