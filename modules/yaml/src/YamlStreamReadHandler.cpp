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
    if (reader) {
        reader->deref(&xsink);
    }
    stream->deref(&xsink);
}

void YamlStreamReadHandler::setError(ExceptionSink& xsink, const char* default_message) {
    has_error = true;
    error_message = default_message;
    QoreValue err = xsink.getExceptionErr();
    if (!err.isNothing()) {
        QoreStringValueHelper errstr(err);
        error_code = errstr->c_str();
        QoreValue desc = xsink.getExceptionDesc();
        if (!desc.isNothing()) {
            QoreStringValueHelper descstr(desc);
            error_message = descstr->c_str();
        }
    }
    xsink.clear();
}

int YamlStreamReadHandler::readText(size_t size, std::string& utf8) {
    utf8.clear();
    ExceptionSink xsink;
    if (!reader) {
        QoreProgram* pgm = stream->getProgram();
        const QoreClass* cls = pgm ? pgm->findClass("Qore::StreamReader", &xsink) : nullptr;
        if (!cls) {
            if (!xsink) {
                xsink.raiseException("STREAM-READ-ERROR", "the StreamReader class is not available");
            }
            setError(xsink, "cannot create a StreamReader");
            return -1;
        }
        ReferenceHolder<QoreListNode> cargs(new QoreListNode(autoTypeInfo), &xsink);
        cargs->push(stream->refSelf(), &xsink);
        cargs->push(new QoreStringNode(encoding->getCode()), &xsink);
        reader = cls->execConstructor(*cargs, &xsink);
        if (xsink) {
            if (reader) {
                reader->deref(&xsink);
                reader = nullptr;
            }
            setError(xsink, "cannot create a StreamReader");
            return -1;
        }
    }
    // whole characters, at most four UTF-8 bytes each, so that the text fits the buffer
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
    args->push((int64)(size >= 4 ? size / 4 : 1), &xsink);
    ValueHolder rv(reader->evalMethod("readString", *args, &xsink), &xsink);
    if (xsink) {
        setError(xsink, "stream read error");
        return -1;
    }
    if (rv->getType() != NT_STRING) {
        // the end of the stream
        return 0;
    }
    const QoreStringNode* str = rv->get<const QoreStringNode>();
    TempEncodingHelper text(str, QCS_UTF8, &xsink);
    if (xsink) {
        setError(xsink, "encoding conversion error");
        return -1;
    }
    utf8.assign(text->c_str(), text->size());
    return 0;
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

    // text in another encoding than UTF-8 is read in whole characters and converted to UTF-8
    if (handler->encoding != QCS_UTF8) {
        std::string utf8;
        if (handler->readText(size, utf8)) {
            return 0;
        }
        size_t utf8_len = utf8.size();
        if (utf8_len <= size) {
            memcpy(buffer, utf8.data(), utf8_len);
            *size_read = utf8_len;
        } else {
            memcpy(buffer, utf8.data(), size);
            handler->pending_data.assign(utf8.data() + size, utf8_len - size);
            *size_read = size;
        }
        return 1;
    }

    // Read from the stream
    ExceptionSink xsink;
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
    args->push((int64)size, &xsink);

    ValueHolder rv(handler->stream->evalMethod("read", *args, &xsink), &xsink);
    if (xsink) {
        handler->setError(xsink, "stream read error");
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

    // UTF-8 bytes are passed to the parser as they are
    size_t chunk_size = chunk->size();
    if (chunk_size <= size) {
        memcpy(buffer, chunk->getPtr(), chunk_size);
        *size_read = chunk_size;
    } else {
        memcpy(buffer, chunk->getPtr(), size);
        handler->pending_data.assign((const char*)chunk->getPtr() + size, chunk_size - size);
        *size_read = size;
    }

    return 1;
}
