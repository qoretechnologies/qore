/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreYamlStreamWriter.cpp

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

#include "QC_YamlStreamWriter.h"
#include "yaml-module.h"

#include <cmath>
#include <limits>

// Write handler implementation - streams data directly to output stream
QoreYamlStreamWriteHandler::QoreYamlStreamWriteHandler(QoreObject* stream)
    : output_stream(stream), has_error(false) {
    output_stream->ref();
}

QoreYamlStreamWriteHandler::~QoreYamlStreamWriteHandler() {
    ExceptionSink xsink;
    output_stream->deref(&xsink);
}

int QoreYamlStreamWriteHandler::write(unsigned char* buffer, size_t size) {
    if (has_error) return 0;

    // Create binary data from buffer and write directly to stream
    ExceptionSink xsink;
    SimpleRefHolder<BinaryNode> bin(new BinaryNode());
    bin->append(buffer, size);

    // Call stream write method
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
    args->push(bin.release(), &xsink);

    ValueHolder rv(output_stream->evalMethod("write", *args, &xsink), &xsink);
    if (xsink) {
        has_error = true;
        error_message = "stream write error";
        QoreValue err = xsink.getExceptionErr();
        if (!err.isNothing()) {
            QoreStringValueHelper errstr(err);
            error_message = errstr->c_str();
        }
        xsink.clear();
        return 0;
    }
    return 1;
}

// Static callback for libyaml
int QoreYamlStreamWriter::yaml_write_handler(void* data, unsigned char* buffer, size_t size) {
    QoreYamlStreamWriteHandler* handler = static_cast<QoreYamlStreamWriteHandler*>(data);
    return handler->write(buffer, size);
}

// Stream writer implementation
QoreYamlStreamWriter::QoreYamlStreamWriter(QoreObject* stream, int flags, int width, int indent,
                                           ExceptionSink* xsink)
    : output_stream(stream), emitter_initialized(false), state(YWS_INITIAL),
      emit_flags(flags), document_count(0), write_handler(nullptr) {

    output_stream->ref();

    block_style = (flags & QYE_BLOCK_STYLE) != 0;
    explicit_start_doc = (flags & QYE_EXPLICIT_START_DOC) != 0;
    explicit_end_doc = (flags & QYE_EXPLICIT_END_DOC) != 0;

    // Create write handler (streams data directly to output)
    write_handler.reset(new QoreYamlStreamWriteHandler(stream));

    // Initialize emitter
    if (!yaml_emitter_initialize(&emitter)) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "failed to initialize YAML emitter");
        return;
    }
    emitter_initialized = true;

    // Set output handler
    yaml_emitter_set_output(&emitter, yaml_write_handler, write_handler.get());
    yaml_emitter_set_encoding(&emitter, YAML_UTF8_ENCODING);
    yaml_emitter_set_unicode(&emitter, !(flags & QYE_ESCAPE_UNICODE));

    if (flags & QYE_CANONICAL) {
        yaml_emitter_set_canonical(&emitter, 1);
    }

    if (width > 0) {
        yaml_emitter_set_width(&emitter, width);
    }

    if (indent > 0) {
        yaml_emitter_set_indent(&emitter, indent);
    }

    // Start stream
    if (!yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING)) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "failed to initialize stream start event");
        return;
    }

    if (emitEvent("stream start", xsink)) {
        return;
    }

    state = YWS_STREAM_STARTED;
}

QoreYamlStreamWriter::~QoreYamlStreamWriter() {
    if (emitter_initialized) {
        yaml_emitter_delete(&emitter);
    }
    ExceptionSink xsink;
    output_stream->deref(&xsink);
}

int QoreYamlStreamWriter::emitEvent(const char* event_name, ExceptionSink* xsink) {
    if (!yaml_emitter_emit(&emitter, &event)) {
        // Check if the error was from stream writing
        if (write_handler && write_handler->hasError()) {
            xsink->raiseException(QY_STREAM_EMIT_ERR, "failed to emit %s event: stream write error: %s",
                event_name, write_handler->getErrorMessage());
        } else {
            xsink->raiseException(QY_STREAM_EMIT_ERR, "failed to emit %s event: %s",
                event_name, emitter.problem ? emitter.problem : "unknown error");
        }
        return -1;
    }
    return 0;
}

int QoreYamlStreamWriter::startDocument(ExceptionSink* xsink) {
    if (state == YWS_CLOSED) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "writer is closed");
        return -1;
    }
    if (state != YWS_STREAM_STARTED) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "cannot start document in current state");
        return -1;
    }

    // Clear anchors from previous document - anchors are document-scoped per YAML spec
    anchors.clear();

    yaml_version_directive_t* version = nullptr;
    if (emit_flags & QYE_VER_1_1) {
        version = &yaml_ver_1_1;
    } else if (emit_flags & QYE_VER_1_2) {
        version = &yaml_ver_1_2;
    }

    if (!yaml_document_start_event_initialize(&event, version, nullptr, nullptr,
                                               !explicit_start_doc)) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "failed to initialize document start event");
        return -1;
    }

    if (emitEvent("document start", xsink)) {
        return -1;
    }

    state = YWS_IN_DOCUMENT;
    document_count++;
    return 0;
}

int QoreYamlStreamWriter::endDocument(ExceptionSink* xsink) {
    if (state != YWS_IN_DOCUMENT) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "no document to end");
        return -1;
    }

    if (!yaml_document_end_event_initialize(&event, !explicit_end_doc)) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "failed to initialize document end event");
        return -1;
    }

    if (emitEvent("document end", xsink)) {
        return -1;
    }

    state = YWS_STREAM_STARTED;
    return 0;
}

int QoreYamlStreamWriter::startSequence(const char* anchor, ExceptionSink* xsink) {
    if (state != YWS_IN_DOCUMENT && state != YWS_IN_MAPPING &&
        state != YWS_IN_SEQUENCE && state != YWS_EXPECT_VALUE) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "cannot start sequence in current state");
        return -1;
    }

    yaml_sequence_style_t style = block_style ? YAML_BLOCK_SEQUENCE_STYLE : YAML_ANY_SEQUENCE_STYLE;

    if (!yaml_sequence_start_event_initialize(&event, (yaml_char_t*)anchor,
        (yaml_char_t*)YAML_SEQ_TAG, !anchor, style)) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "failed to initialize sequence start event");
        return -1;
    }

    if (anchor) {
        anchors.insert(anchor);
    }

    if (emitEvent("sequence start", xsink)) {
        return -1;
    }

    // Push current state onto stack before changing
    state_stack.push_back(state == YWS_EXPECT_VALUE ? YWS_IN_MAPPING : state);
    state = YWS_IN_SEQUENCE;
    return 0;
}

int QoreYamlStreamWriter::endSequence(ExceptionSink* xsink) {
    if (state != YWS_IN_SEQUENCE) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "no sequence to end");
        return -1;
    }

    if (!yaml_sequence_end_event_initialize(&event)) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "failed to initialize sequence end event");
        return -1;
    }

    if (emitEvent("sequence end", xsink)) {
        return -1;
    }

    // Pop state from stack
    if (!state_stack.empty()) {
        state = state_stack.back();
        state_stack.pop_back();
    } else {
        state = YWS_IN_DOCUMENT;
    }
    return 0;
}

int QoreYamlStreamWriter::startMapping(const char* anchor, ExceptionSink* xsink) {
    if (state != YWS_IN_DOCUMENT && state != YWS_IN_MAPPING &&
        state != YWS_IN_SEQUENCE && state != YWS_EXPECT_VALUE) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "cannot start mapping in current state");
        return -1;
    }

    yaml_mapping_style_t style = block_style ? YAML_BLOCK_MAPPING_STYLE : YAML_ANY_MAPPING_STYLE;

    if (!yaml_mapping_start_event_initialize(&event, (yaml_char_t*)anchor,
        (yaml_char_t*)YAML_MAP_TAG, !anchor, style)) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "failed to initialize mapping start event");
        return -1;
    }

    if (anchor) {
        anchors.insert(anchor);
    }

    if (emitEvent("mapping start", xsink)) {
        return -1;
    }

    // Push current state onto stack before changing
    state_stack.push_back(state == YWS_EXPECT_VALUE ? YWS_IN_MAPPING : state);
    state = YWS_IN_MAPPING;
    return 0;
}

int QoreYamlStreamWriter::endMapping(ExceptionSink* xsink) {
    if (state != YWS_IN_MAPPING) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "no mapping to end");
        return -1;
    }

    if (!yaml_mapping_end_event_initialize(&event)) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "failed to initialize mapping end event");
        return -1;
    }

    if (emitEvent("mapping end", xsink)) {
        return -1;
    }

    // Pop state from stack
    if (!state_stack.empty()) {
        state = state_stack.back();
        state_stack.pop_back();
    } else {
        state = YWS_IN_DOCUMENT;
    }
    return 0;
}

int QoreYamlStreamWriter::writeKey(const QoreStringNode* key, ExceptionSink* xsink) {
    if (state != YWS_IN_MAPPING) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "can only write key inside a mapping");
        return -1;
    }

    TempEncodingHelper str(key, QCS_UTF8, xsink);
    if (*xsink) return -1;

    if (!yaml_scalar_event_initialize(&event, nullptr, (yaml_char_t*)YAML_STR_TAG,
        (yaml_char_t*)str->c_str(), str->strlen(), 1, 1, YAML_PLAIN_SCALAR_STYLE)) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "failed to initialize key scalar event");
        return -1;
    }

    if (emitEvent("key scalar", xsink)) {
        return -1;
    }

    state = YWS_EXPECT_VALUE;
    return 0;
}

int QoreYamlStreamWriter::writeScalarValue(const QoreStringNode* value, const char* tag,
                                            const char* anchor, yaml_scalar_style_t style,
                                            ExceptionSink* xsink) {
    TempEncodingHelper str(value, QCS_UTF8, xsink);
    if (*xsink) return -1;

    bool implicit = (tag == nullptr);

    if (!yaml_scalar_event_initialize(&event, (yaml_char_t*)anchor,
        (yaml_char_t*)(tag ? tag : YAML_STR_TAG),
        (yaml_char_t*)str->c_str(), str->strlen(),
        implicit, implicit, style)) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "failed to initialize scalar event");
        return -1;
    }

    if (anchor) {
        anchors.insert(anchor);
    }

    return emitEvent("scalar", xsink);
}

int QoreYamlStreamWriter::writeScalar(QoreValue value, const QoreStringNode* tag,
                                       const QoreStringNode* anchor, ExceptionSink* xsink) {
    if (state != YWS_IN_DOCUMENT && state != YWS_IN_MAPPING &&
        state != YWS_IN_SEQUENCE && state != YWS_EXPECT_VALUE) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "cannot write scalar in current state");
        return -1;
    }

    const char* tag_str = tag ? tag->c_str() : nullptr;
    const char* anchor_str = anchor ? anchor->c_str() : nullptr;

    // Handle type-specific formatting
    SimpleRefHolder<QoreStringNode> str_node;
    yaml_scalar_style_t style = YAML_DOUBLE_QUOTED_SCALAR_STYLE;

    switch (value.getType()) {
        case NT_BOOLEAN:
            str_node = new QoreStringNode(value.getAsBool() ? "true" : "false");
            style = YAML_PLAIN_SCALAR_STYLE;
            break;
        case NT_INT:
            str_node = new QoreStringNode();
            str_node->sprintf(QLLD, value.getAsBigInt());
            style = YAML_PLAIN_SCALAR_STYLE;
            break;
        case NT_FLOAT: {
            str_node = new QoreStringNode();
            double f = value.getAsFloat();
            if (std::isnan(f)) {
                str_node->concat(".nan");
            } else if (std::isinf(f)) {
                str_node->concat(f < 0 ? "-.inf" : ".inf");
            } else {
                yaml_format_finite_float(**str_node, f);
            }
            style = YAML_PLAIN_SCALAR_STYLE;
            break;
        }
        case NT_NUMBER: {
            str_node = new QoreStringNode();
            const QoreNumberNode* n = value.get<const QoreNumberNode>();
            n->toString(**str_node, QORE_NF_SCIENTIFIC | QORE_NF_RAW);
            if (**str_node == "inf") {
                str_node->set("@inf@n");
            } else if (**str_node == "-inf") {
                str_node->set("-@inf@n");
            } else if (**str_node == "nan") {
                str_node->set("@nan@n");
            } else {
                str_node->concat('n');
            }
            str_node->sprintf("{%d}", n->getPrec());
            if (!tag_str) {
                tag_str = QORE_YAML_NUMBER_TAG;
            }
            break;
        }
        case NT_NULL:
        case NT_NOTHING:
            str_node = new QoreStringNode("null");
            style = YAML_PLAIN_SCALAR_STYLE;
            break;
        default: {
            QoreStringValueHelper str(value, QCS_UTF8, xsink);
            if (*xsink) return -1;
            str_node = new QoreStringNode(**str);
            break;
        }
    }

    int rv = writeScalarValue(*str_node, tag_str, anchor_str, style, xsink);

    if (!rv && state == YWS_EXPECT_VALUE) {
        state = YWS_IN_MAPPING;
    }
    return rv;
}

int QoreYamlStreamWriter::writeAlias(const char* anchor, ExceptionSink* xsink) {
    if (state != YWS_IN_DOCUMENT && state != YWS_IN_MAPPING &&
        state != YWS_IN_SEQUENCE && state != YWS_EXPECT_VALUE) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "cannot write alias in current state");
        return -1;
    }

    if (anchors.find(anchor) == anchors.end()) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "unknown anchor '%s'", anchor);
        return -1;
    }

    if (!yaml_alias_event_initialize(&event, (yaml_char_t*)anchor)) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "failed to initialize alias event");
        return -1;
    }

    int rv = emitEvent("alias", xsink);
    if (!rv && state == YWS_EXPECT_VALUE) {
        state = YWS_IN_MAPPING;
    }
    return rv;
}

int QoreYamlStreamWriter::writeValueRecursive(QoreValue value, ExceptionSink* xsink) {
    switch (value.getType()) {
        case NT_HASH: {
            if (startMapping(nullptr, xsink)) return -1;
            const QoreHashNode* h = value.get<const QoreHashNode>();
            ConstHashIterator hi(h);
            unsigned iteration = 0;
            while (hi.next()) {
                // Check cancellation every 10 potentially recursive parsing steps
                if (++iteration % 10 == 0) {
                    if (qore_check_cancel(xsink, "YAML mapping writing")) {
                        return -1;
                    }
                }
                SimpleRefHolder<QoreStringNode> key(new QoreStringNode(hi.getKey()));
                if (writeKey(*key, xsink)) return -1;
                if (writeValueRecursive(hi.get(), xsink)) return -1;
            }
            return endMapping(xsink);
        }

        case NT_LIST: {
            if (startSequence(nullptr, xsink)) return -1;
            const QoreListNode* l = value.get<const QoreListNode>();
            ConstListIterator li(l);
            unsigned iteration = 0;
            while (li.next()) {
                // Check cancellation every 10 potentially recursive parsing steps
                if (++iteration % 10 == 0) {
                    if (qore_check_cancel(xsink, "YAML sequence writing")) {
                        return -1;
                    }
                }
                if (writeValueRecursive(li.getValue(), xsink)) return -1;
            }
            return endSequence(xsink);
        }

        default:
            return writeScalar(value, nullptr, nullptr, xsink);
    }
}

int QoreYamlStreamWriter::write(QoreValue value, ExceptionSink* xsink) {
    return writeValueRecursive(value, xsink);
}

int QoreYamlStreamWriter::flush(ExceptionSink* xsink) {
    if (!emitter_initialized) return 0;

    if (!yaml_emitter_flush(&emitter)) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "failed to flush emitter");
        return -1;
    }
    return 0;
}

int QoreYamlStreamWriter::close(ExceptionSink* xsink) {
    if (state == YWS_CLOSED) return 0;

    // Check for unclosed containers and raise exception
    if (!state_stack.empty() || state == YWS_IN_MAPPING || state == YWS_IN_SEQUENCE || state == YWS_EXPECT_VALUE) {
        size_t unclosed_count = state_stack.size();
        if (state == YWS_IN_MAPPING || state == YWS_IN_SEQUENCE || state == YWS_EXPECT_VALUE) {
            unclosed_count++;
        }
        xsink->raiseException(QY_STREAM_EMIT_ERR,
            "close() called with %zu unclosed container(s); must call endMapping()/endSequence() for all open containers before close()",
            unclosed_count);
        // Still try to clean up the state
        state = YWS_CLOSED;
        return -1;
    }

    // End any open document
    if (state == YWS_IN_DOCUMENT) {
        if (!yaml_document_end_event_initialize(&event, !explicit_end_doc)) {
            xsink->raiseException(QY_STREAM_EMIT_ERR, "failed to initialize document end event");
            return -1;
        }
        if (emitEvent("document end", xsink)) {
            return -1;
        }
    }

    // End stream
    if (!yaml_stream_end_event_initialize(&event)) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "failed to initialize stream end event");
        return -1;
    }

    if (emitEvent("stream end", xsink)) {
        return -1;
    }
    if (flush(xsink)) {
        return -1;
    }

    // Check for write handler errors
    if (write_handler && write_handler->hasError()) {
        xsink->raiseException(QY_STREAM_EMIT_ERR, "stream write error: %s",
            write_handler->getErrorMessage());
        state = YWS_CLOSED;
        return -1;
    }

    state = YWS_CLOSED;
    return 0;
}
