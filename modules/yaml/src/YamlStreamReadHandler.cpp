/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    YamlStreamReadHandler.cpp

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

#include "YamlStreamReadHandler.h"

YamlStreamReadHandler::YamlStreamReadHandler(QoreObject* s, const QoreEncoding* enc)
    : stream(s), encoding(enc ? enc : QCS_UTF8), has_error(false) {
    stream->ref();
}

YamlStreamReadHandler::~YamlStreamReadHandler() {
    ExceptionSink xsink;
    stream->deref(&xsink);
}

void YamlStreamReadHandler::setupParser(yaml_parser_t* parser) {
    yaml_parser_set_input(parser, yaml_read_handler, this);
}

int YamlStreamReadHandler::yaml_read_handler(void* data, unsigned char* buffer,
                                              size_t size, size_t* size_read) {
    YamlStreamReadHandler* handler = static_cast<YamlStreamReadHandler*>(data);
    *size_read = 0;

    if (handler->has_error) {
        return 0;
    }

    // First, return any pending data from encoding conversion
    if (!handler->pending_data.empty()) {
        size_t to_copy = std::min(size, handler->pending_data.size());
        memcpy(buffer, handler->pending_data.data(), to_copy);
        handler->pending_data.erase(0, to_copy);
        *size_read = to_copy;
        return 1;
    }

    // Read from the stream
    ExceptionSink xsink;
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
    args->push((int64)size, &xsink);

    ValueHolder rv(handler->stream->evalMethod("read", *args, &xsink), &xsink);
    if (xsink) {
        handler->has_error = true;
        handler->error_message = "stream read error";
        QoreValue err = xsink.getExceptionErr();
        if (!err.isNothing()) {
            QoreStringValueHelper errstr(err);
            handler->error_message = errstr->c_str();
        }
        xsink.clear();
        return 0;
    }

    if (rv->isNothing()) {
        // End of stream - this is success with 0 bytes read
        *size_read = 0;
        return 1;
    }

    // Check if return value is binary - non-binary is an error
    if (rv->getType() != NT_BINARY) {
        handler->has_error = true;
        handler->error_message = "stream read returned non-binary value";
        return 0;
    }

    const BinaryNode* chunk = rv->get<const BinaryNode>();
    if (chunk->size() == 0) {
        // Empty binary means end of stream
        *size_read = 0;
        return 1;
    }

    // Convert encoding if needed
    if (handler->encoding != QCS_UTF8) {
        SimpleRefHolder<QoreStringNode> str(new QoreStringNode(
            (const char*)chunk->getPtr(), chunk->size(), handler->encoding));
        TempEncodingHelper utf8(*str, QCS_UTF8, &xsink);
        if (xsink) {
            handler->has_error = true;
            handler->error_message = "encoding conversion error";
            xsink.clear();
            return 0;
        }

        size_t utf8_len = utf8->strlen();
        if (utf8_len <= size) {
            memcpy(buffer, utf8->c_str(), utf8_len);
            *size_read = utf8_len;
        } else {
            // Buffer too small - copy what we can and save the rest
            memcpy(buffer, utf8->c_str(), size);
            handler->pending_data.assign(utf8->c_str() + size, utf8_len - size);
            *size_read = size;
        }
    } else {
        // Already UTF-8
        size_t chunk_size = chunk->size();
        if (chunk_size <= size) {
            memcpy(buffer, chunk->getPtr(), chunk_size);
            *size_read = chunk_size;
        } else {
            memcpy(buffer, chunk->getPtr(), size);
            handler->pending_data.assign((const char*)chunk->getPtr() + size, chunk_size - size);
            *size_read = size;
        }
    }

    return 1;
}
