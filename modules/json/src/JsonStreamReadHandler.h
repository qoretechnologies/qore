/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    JsonStreamReadHandler.h

    Qore Programming Language - JSON Module

    Copyright (C) 2026 Qore Technologies, s.r.o.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _QORE_JSON_STREAM_READ_HANDLER_H
#define _QORE_JSON_STREAM_READ_HANDLER_H

#include <qore/Qore.h>

#include <string>

//! Stream read handler for JSON SAX parsing - enables true streaming input
/** This class provides a shared implementation for reading from Qore InputStream objects
    and converting the data to UTF-8 for the JSON SAX parser.
*/
class JsonStreamReadHandler {
public:
    //! Constructor
    /** @param s the Qore InputStream object to read from
        @param enc the character encoding of the input stream
    */
    DLLLOCAL JsonStreamReadHandler(QoreObject* s, const QoreEncoding* enc);

    //! Destructor - releases reference to the stream object
    DLLLOCAL ~JsonStreamReadHandler();

    //! Reads data from the stream into the output buffer
    /** @param out output buffer to append to
        @param size maximum number of bytes to read
        @return true on success, false on error
    */
    DLLLOCAL bool read(std::string& out, size_t size);

    //! Returns true if an error occurred during reading
    DLLLOCAL bool hasError() const { return has_error; }

    //! Returns the error message if an error occurred
    DLLLOCAL const std::string& getErrorMessage() const { return error_message; }

private:
    QoreObject* stream;
    const QoreEncoding* encoding;
    bool has_error;
    std::string error_message;
    //! Buffer for partial multibyte sequences in the input encoding
    std::string pending_input;
    //! Buffer for encoding conversion
    std::string pending_data;
};

#endif
