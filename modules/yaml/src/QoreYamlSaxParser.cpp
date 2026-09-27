/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreYamlSaxParser.cpp

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

#include "QC_YamlSaxParser.h"
#include "yaml-module.h"
#include "yaml-scalar-util.h"

#include <qore/QoreEnumDecl.h>

QoreYamlSaxParser::QoreYamlSaxParser() {
}

QoreYamlSaxParser::~QoreYamlSaxParser() {
}

QoreHashNode* QoreYamlSaxParser::createEventHash(yaml_event_t& event, int depth,
                                                  ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclYamlSaxEvent, xsink), xsink);

    int type_val = 0;
    switch (event.type) {
        case YAML_STREAM_START_EVENT: type_val = YAML_SAX_STREAM_START; break;
        case YAML_STREAM_END_EVENT: type_val = YAML_SAX_STREAM_END; break;
        case YAML_DOCUMENT_START_EVENT: type_val = YAML_SAX_DOCUMENT_START; break;
        case YAML_DOCUMENT_END_EVENT: type_val = YAML_SAX_DOCUMENT_END; break;
        case YAML_ALIAS_EVENT: type_val = YAML_SAX_ALIAS; break;
        case YAML_SCALAR_EVENT: type_val = YAML_SAX_SCALAR; break;
        case YAML_SEQUENCE_START_EVENT: type_val = YAML_SAX_SEQUENCE_START; break;
        case YAML_SEQUENCE_END_EVENT: type_val = YAML_SAX_SEQUENCE_END; break;
        case YAML_MAPPING_START_EVENT: type_val = YAML_SAX_MAPPING_START; break;
        case YAML_MAPPING_END_EVENT: type_val = YAML_SAX_MAPPING_END; break;
        default: break;
    }

    const QoreEnumMember* type_member = enumYamlSaxEventType->findMemberByValue(QoreValue(type_val));
    assert(type_member);
    h->setKeyValue("type", QoreValue::makeEnum(type_member), xsink);
    h->setKeyValue("depth", depth, xsink);
    h->setKeyValue("line", (int64)event.start_mark.line + 1, xsink);
    h->setKeyValue("column", (int64)event.start_mark.column + 1, xsink);

    // Handle event-specific fields
    if (event.type == YAML_SCALAR_EVENT) {
        if (event.data.scalar.anchor)
            h->setKeyValue("anchor", new QoreStringNode((const char*)event.data.scalar.anchor), xsink);
        if (event.data.scalar.tag)
            h->setKeyValue("tag", new QoreStringNode((const char*)event.data.scalar.tag), xsink);

        // Deserialize scalar value to proper Qore type
        {
            const char* val = (const char*)event.data.scalar.value;
            size_t len = event.data.scalar.length;
            if (event.data.scalar.tag) {
                const char* tag = (const char*)event.data.scalar.tag;
                h->setKeyValue("value", yaml_parse_tagged_scalar(val, len, tag, xsink), xsink);
            } else {
                h->setKeyValue("value", yaml_parse_implicit_scalar(val, len,
                    event.data.scalar.style, false, xsink), xsink);
            }
            if (*xsink) {
                return nullptr;
            }
        }
        // Set implicit based on style: use plain_implicit for plain scalars, quoted_implicit for quoted
        // If either is true, it means the tag was inferred rather than explicitly specified
        bool is_implicit = event.data.scalar.plain_implicit || event.data.scalar.quoted_implicit;
        h->setKeyValue("implicit", is_implicit, xsink);

        const char* style_str = "plain";
        switch (event.data.scalar.style) {
            case YAML_SINGLE_QUOTED_SCALAR_STYLE: style_str = "single_quoted"; break;
            case YAML_DOUBLE_QUOTED_SCALAR_STYLE: style_str = "double_quoted"; break;
            case YAML_LITERAL_SCALAR_STYLE: style_str = "literal"; break;
            case YAML_FOLDED_SCALAR_STYLE: style_str = "folded"; break;
            default: break;
        }
        {
            SimpleRefHolder<QoreStringNode> style_val(new QoreStringNode(style_str));
            const QoreEnumMember* style_member = enumYamlScalarStyle->findMemberByValue(QoreValue(*style_val));
            assert(style_member);
            h->setKeyValue("style", QoreValue::makeEnum(style_member), xsink);
        }
    }
    else if (event.type == YAML_ALIAS_EVENT) {
        h->setKeyValue("alias", new QoreStringNode((const char*)event.data.alias.anchor), xsink);
    }
    else if (event.type == YAML_SEQUENCE_START_EVENT) {
        if (event.data.sequence_start.anchor)
            h->setKeyValue("anchor", new QoreStringNode((const char*)event.data.sequence_start.anchor), xsink);
        if (event.data.sequence_start.tag)
            h->setKeyValue("tag", new QoreStringNode((const char*)event.data.sequence_start.tag), xsink);
    }
    else if (event.type == YAML_MAPPING_START_EVENT) {
        if (event.data.mapping_start.anchor)
            h->setKeyValue("anchor", new QoreStringNode((const char*)event.data.mapping_start.anchor), xsink);
        if (event.data.mapping_start.tag)
            h->setKeyValue("tag", new QoreStringNode((const char*)event.data.mapping_start.tag), xsink);
    }

    return h.release();
}

bool QoreYamlSaxParser::emitEvent(ParseState& state, yaml_event_t& event) {
    ReferenceHolder<QoreHashNode> event_hash(createEventHash(event, state.depth, state.xsink),
                                              state.xsink);
    if (*state.xsink) return false;

    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), state.xsink);
    args->push(event_hash.release(), state.xsink);

    ValueHolder rv(state.callback->execValue(*args, state.xsink), state.xsink);
    return !*state.xsink;
}

bool QoreYamlSaxParser::processEvents(ParseState& state) {
    yaml_event_t event;
    bool done = false;

    unsigned iteration = 0;
    while (!done) {
        // Check cancellation every 10 potentially recursive parsing steps
        if (++iteration % 10 == 0) {
            if (qore_check_cancel(state.xsink, "YAML SAX parsing")) {
                return false;
            }
        }

        if (!yaml_parser_parse(&state.parser, &event)) {
            state.xsink->raiseException(QY_SAX_PARSE_ERR,
                "YAML parse error at line %d column %d: %s",
                (int)state.parser.problem_mark.line + 1,
                (int)state.parser.problem_mark.column + 1,
                state.parser.problem ? state.parser.problem : "unknown error");
            return false;
        }

        // Adjust depth based on event type
        switch (event.type) {
            case YAML_SEQUENCE_START_EVENT:
            case YAML_MAPPING_START_EVENT:
                if (!emitEvent(state, event)) {
                    yaml_event_delete(&event);
                    return false;
                }
                state.depth++;
                break;

            case YAML_SEQUENCE_END_EVENT:
            case YAML_MAPPING_END_EVENT:
                state.depth--;
                if (!emitEvent(state, event)) {
                    yaml_event_delete(&event);
                    return false;
                }
                break;

            case YAML_STREAM_END_EVENT:
                done = true;
                // fall through
            default:
                if (!emitEvent(state, event)) {
                    yaml_event_delete(&event);
                    return false;
                }
                break;
        }

        yaml_event_delete(&event);
    }

    return true;
}

int QoreYamlSaxParser::parse(const QoreStringNode* yaml_str,
                             ResolvedCallReferenceNode* callback,
                             ExceptionSink* xsink) {
    assert(yaml_str != nullptr);
    assert(callback != nullptr);

    TempEncodingHelper str(yaml_str, QCS_UTF8, xsink);
    if (*xsink) return -1;

    ParseState state;
    state.depth = 0;
    state.callback = callback;
    state.xsink = xsink;

    yaml_parser_initialize(&state.parser);
    yaml_parser_set_input_string(&state.parser,
        (const unsigned char*)str->c_str(), str->strlen());
    yaml_parser_set_encoding(&state.parser, YAML_UTF8_ENCODING);

    bool result = processEvents(state);
    yaml_parser_delete(&state.parser);

    return result ? 0 : -1;
}

int QoreYamlSaxParser::parseStream(QoreObject* stream,
                                   ResolvedCallReferenceNode* callback,
                                   const QoreEncoding* encoding,
                                   ExceptionSink* xsink) {
    assert(stream != nullptr);
    assert(callback != nullptr);

    // Use shared streaming read handler
    YamlStreamReadHandler read_handler(stream, encoding);

    ParseState state;
    state.depth = 0;
    state.callback = callback;
    state.xsink = xsink;

    yaml_parser_initialize(&state.parser);
    read_handler.setupParser(&state.parser);
    yaml_parser_set_encoding(&state.parser, YAML_UTF8_ENCODING);

    bool result = processEvents(state);

    // Check for read handler errors
    if (read_handler.hasError() && !*xsink) {
        xsink->raiseException(QY_SAX_STREAM_ERR, "stream read error: %s",
            read_handler.getErrorMessage().c_str());
        result = false;
    }

    yaml_parser_delete(&state.parser);

    return result ? 0 : -1;
}
