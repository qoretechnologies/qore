/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    JsonStreamReadHandler.cpp

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

#include "JsonStreamReadHandler.h"

JsonStreamReadHandler::JsonStreamReadHandler(QoreObject* s, const QoreEncoding* enc)
    : stream(s), encoding(enc ? enc : QCS_UTF8), has_error(false) {
    stream->ref();
}

JsonStreamReadHandler::~JsonStreamReadHandler() {
    ExceptionSink xsink;
    stream->deref(&xsink);
}

bool JsonStreamReadHandler::read(std::string& out, size_t size) {
    out.clear();
    if (has_error) {
        return false;
    }

    ExceptionSink xsink;

    while (true) {
        // First, return any pending data from encoding conversion
        if (!pending_data.empty()) {
            size_t to_copy = std::min(size, pending_data.size());
            out.assign(pending_data.data(), to_copy);
            pending_data.erase(0, to_copy);
            return true;
        }

        // Read from the stream
        std::string raw;
        if (!pending_input.empty()) {
            raw.swap(pending_input);
        }

        xsink.clear();
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
        args->push((int64)size, &xsink);

        ValueHolder rv(stream->evalMethod("read", *args, &xsink), &xsink);
        if (xsink) {
            has_error = true;
            error_message = "stream read error";
            QoreValue err = xsink.getExceptionErr();
            if (!err.isNothing()) {
                QoreStringValueHelper errstr(err);
                error_message = errstr->c_str();
            }
            xsink.clear();
            return false;
        }

        bool eof = false;
        if (rv->isNothing()) {
            eof = true;
        } else {
            if (rv->getType() != NT_BINARY) {
                has_error = true;
                error_message = "stream read returned non-binary value";
                return false;
            }

            const BinaryNode* chunk = rv->get<const BinaryNode>();
            if (chunk->size() == 0) {
                eof = true;
            } else {
                raw.append((const char*)chunk->getPtr(), chunk->size());
            }
        }

        // Convert encoding if needed
        if (encoding != QCS_UTF8) {
            if (raw.empty()) {
                if (eof) {
                    return true;
                }
                continue;
            }
            size_t use_len = raw.size();
            bool converted = false;
            for (size_t cut = 0; cut <= std::min((size_t)8, raw.size() - 1); ++cut) {
                xsink.clear();
                use_len = raw.size() - cut;
                SimpleRefHolder<QoreStringNode> str(new QoreStringNode(
                    raw.data(), use_len, encoding));
                TempEncodingHelper utf8(*str, QCS_UTF8, &xsink);
                if (!xsink) {
                    if (cut > 0) {
                        pending_input.assign(raw.data() + use_len, cut);
                    }
                    size_t utf8_len = utf8->strlen();
                    if (utf8_len <= size) {
                        out.assign(utf8->c_str(), utf8_len);
                    } else {
                        out.assign(utf8->c_str(), size);
                        pending_data.assign(utf8->c_str() + size, utf8_len - size);
                    }
                    converted = true;
                    break;
                }
            }
            if (converted) {
                return true;
            }
            if (eof) {
                has_error = true;
                error_message = "encoding conversion error";
                xsink.clear();
                return false;
            }
            pending_input.swap(raw);
            xsink.clear();
            continue;
        }

        // Already UTF-8
        if (raw.empty()) {
            if (eof) {
                return true;
            }
            continue;
        }
        size_t raw_size = raw.size();
        if (raw_size <= size) {
            out.assign(raw.data(), raw_size);
        } else {
            out.assign(raw.data(), size);
            pending_data.assign(raw.data() + size, raw_size - size);
        }
        return true;
    }
}
