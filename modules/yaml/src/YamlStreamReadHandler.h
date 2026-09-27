/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    YamlStreamReadHandler.h

    Qore Programming Language

    Copyright 2003 - 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_YAML_STREAM_READ_HANDLER_H
#define _QORE_YAML_STREAM_READ_HANDLER_H

#include <qore/Qore.h>
#include <yaml.h>

#include <string>

//! Stream read handler for libyaml - enables true streaming input from Qore InputStreams
/** This class provides a shared implementation for reading from Qore InputStream objects
    and feeding the data to libyaml's parser. It handles:
    - Pending data buffering for encoding conversion
    - InputStream.read() method calls
    - EOF detection and non-binary type checking
    - Encoding conversion to UTF-8
    - libyaml callback interface
*/
class YamlStreamReadHandler {
public:
    //! Constructor
    /** @param s the Qore InputStream object to read from
        @param enc the character encoding of the input stream
    */
    DLLLOCAL YamlStreamReadHandler(QoreObject* s, const QoreEncoding* enc);

    //! Destructor - releases reference to the stream object
    DLLLOCAL ~YamlStreamReadHandler();

    //! libyaml read callback - reads from Qore InputStream
    /** @param data pointer to the YamlStreamReadHandler instance
        @param buffer destination buffer for data
        @param size maximum number of bytes to read
        @param size_read output parameter for actual bytes read

        @return 1 on success, 0 on error
    */
    DLLLOCAL static int yaml_read_handler(void* data, unsigned char* buffer,
                                           size_t size, size_t* size_read);

    //! Setup parser to use this handler
    /** @param parser the libyaml parser to configure
    */
    DLLLOCAL void setupParser(yaml_parser_t* parser);

    //! Returns true if an error occurred during reading
    DLLLOCAL bool hasError() const { return has_error; }

    //! Returns the error message if an error occurred
    DLLLOCAL const std::string& getErrorMessage() const { return error_message; }

private:
    QoreObject* stream;
    const QoreEncoding* encoding;
    bool has_error;
    std::string error_message;
    //! Buffer for encoding conversion
    std::string pending_data;
};

#endif
