/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ql_websocket.h

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef QORE_QL_WEBSOCKET_H
#define QORE_QL_WEBSOCKET_H

#include <cstddef>
#include <cstdint>
#include <string>

DLLLOCAL void init_websocket_functions(QoreNamespace& ns);

//! Returns true if the bytes are valid UTF-8, as the payload of a text message and the reason of a close frame must
//! be (RFC 6455 sections 5.5.1, 5.6 and 8.1; qore#6604)
/** UTF-8 is checked as RFC 3629 defines it (q_utf8_char_len()); an ASCII prefix is skipped a word at a time

    @param p the bytes
    @param len the number of bytes
*/
DLLLOCAL bool q_ws_utf8_valid(const void* p, size_t len);

//! Returns true if a received close frame may have the given status code (RFC 6455 section 7.4; qore#6755)
/** The codes 1000 - 1003 and 1007 - 1014 of the IANA registry and the codes 3000 - 4999 for libraries, frameworks
    and applications; the reserved code 1004, the codes that must not be sent in a close frame (1005, 1006, 1015),
    the unused codes below 1000 and the codes 1016 - 2999 and above 4999, which are not defined, are not
*/
DLLLOCAL bool q_ws_close_code_valid(uint16_t code);

//! Parses the payload of a received close frame
/** @param p the payload
    @param len the size of the payload
    @param code the status code of the frame; 1005 (no status code received, RFC 6455 section 7.1.5) for an empty
    payload
    @param reason the reason of the frame, UTF-8
    @param error the description of the error, when the frame is not valid

    @return 0 if the frame is valid, otherwise the status code that the connection is closed with: 1002 (protocol
    error) for a one-byte payload (the status code is two bytes, RFC 6455 section 5.5.1) or a status code that the
    frame may not have (see q_ws_close_code_valid()), 1007 (invalid payload data) for a reason that is not valid
    UTF-8 (qore#6604, qore#6755)
*/
DLLLOCAL uint16_t q_ws_parse_close_payload(const void* p, size_t len, uint16_t& code, std::string& reason,
        const char*& error);

#endif // QORE_QL_WEBSOCKET_H
