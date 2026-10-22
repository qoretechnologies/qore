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

//! Drops one leading UTF-8 byte order mark from a server-sent event stream
/** The event stream format ignores one leading U+FEFF BYTE ORDER MARK of a stream; a byte order mark later in the
    stream is data.  The bytes of the stream are passed through the filter one at a time; the bytes read as the start
    of a byte order mark are passed on when the stream turns out not to start with one.
*/
class SseBomFilter {
public:
    //! Passes a byte of the stream through the filter
    /** @param c the byte
        @param process called with each byte that is data; returns true if the byte completed an event

        @return true if a byte processed completed an event
    */
    template <typename F>
    DLLLOCAL bool add(char c, F process) {
        if (pos < BomSize) {
            static const unsigned char bom[BomSize] = {0xef, 0xbb, 0xbf};
            if (static_cast<unsigned char>(c) == bom[pos]) {
                ++pos;
                return false;
            }
            // the stream does not start with a byte order mark: the bytes read as its start are data; they are not
            // line breaks, so they cannot complete an event
            unsigned matched = pos;
            pos = BomSize;
            for (unsigned i = 0; i < matched; ++i) {
                process(static_cast<char>(bom[i]));
            }
        }
        return process(c);
    }

    //! Resets the filter for a new stream
    DLLLOCAL void reset() {
        pos = 0;
    }

private:
    static constexpr unsigned BomSize = 3;
    //! the number of bytes of the byte order mark read; BomSize once the start of the stream has been read
    unsigned pos = 0;
};

//! Splits a server-sent event stream into events one byte at a time
/** Line endings (CRLF, LF, or CR) are normalized to LF, and an event ends with an empty line, as with
    Socket::readServerSentEvent().  A CR that ends an event is remembered, so that the LF of a CRLF split across
    events is not taken as an empty line of the next event.  One leading byte order mark of the stream is ignored, as
    the event stream format requires.
*/
class SseEventFramer {
public:
    //! Adds a byte of the stream
    /** @return true if the event is complete; see take()
    */
    DLLLOCAL bool add(char c) {
        return bom.add(c, [this](char b) { return addIntern(b); });
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
        bom.reset();
    }

private:
    std::string event;
    int eol_count = 0;
    bool got_cr = false;
    SseBomFilter bom;

    DLLLOCAL bool addIntern(char c) {
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
};

#endif
