/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    yaml-module.h

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

#ifndef _QORE_YAML_MODULE_H
#define _QORE_YAML_MODULE_H

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <qore/Qore.h>
#include "yaml-scalar-util.h"
#include <qore/QoreSandboxManager.h>

#include <yaml.h>

#include <stdarg.h>

#include <map>
#include <cmath>
#include <limits>

#define QYE_NONE                0
#define QYE_CANONICAL           (1 << 0)
#define QYE_ESCAPE_UNICODE      (1 << 1)
#define QYE_EXPLICIT_START_DOC  (1 << 2)
#define QYE_EXPLICIT_END_DOC    (1 << 3)
#define QYE_BLOCK_STYLE         (1 << 4)
#define QYE_VER_1_0             (1 << 5)
#define QYE_VER_1_1             (1 << 6)
#define QYE_VER_1_2             (1 << 7)
#define QYE_EMIT_SQLNULL        (1 << 8)

#define QYE_DEFAULT (QYE_VER_1_2)

// parser options
//! resolve plain scalars with the YAML 1.2 core schema only
#define QYP_CORE_SCHEMA         (1 << 0)

#ifndef YAML_BINARY_TAG
#define YAML_BINARY_TAG "tag:yaml.org,2002:binary"
#endif

#ifndef YAML_OMAP_TAG
#define YAML_OMAP_TAG "tag:yaml.org,2002:omap"
#endif

// maximum length of a string value in an error msg
#define YAML_MAX_ERR_STR_LEN 40

DLLLOCAL extern const char* QORE_YAML_DURATION_TAG;
DLLLOCAL extern const char* QORE_YAML_NUMBER_TAG;
DLLLOCAL extern const char* QORE_YAML_SQLNULL_TAG;

DLLLOCAL extern const char* QY_EMIT_ERR;

DLLLOCAL extern const char* QY_PARSE_ERR;

DLLLOCAL extern QoreString NullStr;
DLLLOCAL extern QoreString SqlNullStr;

DLLLOCAL extern yaml_version_directive_t yaml_ver_1_0, yaml_ver_1_1, yaml_ver_1_2;

typedef std::map<int, const char*> event_map_t;
DLLLOCAL extern event_map_t event_map;

DLLLOCAL extern const char* get_event_name(yaml_event_type_t type);

class QoreYamlWriteHandler {
public:
    DLLLOCAL QoreYamlWriteHandler() {
    }

    DLLLOCAL virtual ~QoreYamlWriteHandler() {
    }

    DLLLOCAL virtual int write(unsigned char* buffer, size_t size) = 0;
};

class QoreYamlBase {
public:
    DLLLOCAL QoreYamlBase(ExceptionSink* xsink) : xsink(xsink) {
    }

protected:
    yaml_event_t event;
    ExceptionSink* xsink;

    bool valid = false;
};

class QoreYamlEmitter : public QoreYamlBase {
public:
    DLLLOCAL QoreYamlEmitter(QoreYamlWriteHandler& n_wh, int flags, int width, int indent, ExceptionSink* n_xsink);

    DLLLOCAL ~QoreYamlEmitter() {
        if (valid) {
            docEnd();
            streamEnd();
        }
        yaml_emitter_delete(&emitter);
    }

    DLLLOCAL int docStart(yaml_tag_directive_t* start = nullptr, unsigned elements = 0) {
        if (!yaml_document_start_event_initialize(&event, yaml_ver, start, start + elements, implicit_start_doc)) {
            return err("unknown error initializing yaml document start event");
        }

        return emit("doc start");
    }

    DLLLOCAL int docEnd() {
        //printd(5, "QoreYamlEmitter::docEnd() ied=%d\n", implicit_end_doc);
        if (!yaml_document_end_event_initialize(&event, implicit_end_doc)) {
            return err("unknown error initializing yaml document end event");
        }

        return emit("doc end");
    }

    DLLLOCAL int seqStart(yaml_sequence_style_t style = YAML_ANY_SEQUENCE_STYLE,
                            const char* tag = YAML_SEQ_TAG, const char* anchor = nullptr, bool implicit = true) {
        if (!yaml_sequence_start_event_initialize(&event, (yaml_char_t*)anchor, (yaml_char_t*)tag,
            implicit, style)) {
            return err("unknown error initializing yaml sequence start event");
        }

        //printd(5, "QoreYamlEmitter::seqStart(tag=%s, anchor=%s)\n", tag, anchor ? anchor : "(null)");
        return emit("seq start");
    }

    DLLLOCAL int seqEnd() {
        if (!yaml_sequence_end_event_initialize(&event)) {
            return err("unknown error initializing yaml sequence end event");
        }

        return emit("seq end");
    }

    DLLLOCAL int mapStart(yaml_mapping_style_t style = YAML_ANY_MAPPING_STYLE,
            const char* tag = YAML_MAP_TAG, const char* anchor = nullptr, bool implicit = true) {
        if (!yaml_mapping_start_event_initialize(&event, (yaml_char_t*)anchor, (yaml_char_t*)tag, implicit, style)) {
            return err("unknown error initializing yaml mapping start event");
        }

        return emit("map start");
    }

    DLLLOCAL int mapEnd() {
        if (!yaml_mapping_end_event_initialize(&event)) {
            return err("unknown error initializing yaml mapping end event");
        }

        return emit("map end");
    }

    DLLLOCAL int doScalarEmissionError(const QoreString& str, const char* tag) {
        // issue #3394: ensure that the string is a valid UTF-8 string before including in the exception output
        // this is just to test for valid UTF-8 data
        ExceptionSink xsink2;
        str.getUnicodePoint(-1, &xsink2);

        QoreString val(QCS_UTF8);
        if (xsink2) {
            xsink2.clear();
            val.clear();
            size_t len = QORE_MIN(str.size(), YAML_MAX_ERR_STR_LEN);
            val.concatHex(str.c_str(), len);
            if (len != str.size()) {
                val.concat("...");
            }

            return err("unknown error initializing yaml scalar output event for yaml type '%s'; value has invalid "
                "UTF-8 encoding: '<%s>'", tag, val.c_str());
        }

        size_t len = str.length();
        size_t orig_len = len;
        if (len > YAML_MAX_ERR_STR_LEN) {
            len = YAML_MAX_ERR_STR_LEN;
        }

        val.concat(str, 0, YAML_MAX_ERR_STR_LEN, &xsink2);
        assert(!xsink2);
        if (len != orig_len) {
            val.concat("...");
        }
        return err("unknown error initializing yaml scalar output event for yaml type '%s', value '%s'", tag,
            val.c_str());
    }

    DLLLOCAL int emitScalar(const QoreString& value, const char* tag, const char* anchor = nullptr,
            bool plain_implicit = true, bool quoted_implicit = true,
            yaml_scalar_style_t style = YAML_ANY_SCALAR_STYLE) {
        TempEncodingHelper str(value, QCS_UTF8, xsink);
        if (*xsink)
            return -1;

        if (!yaml_scalar_event_initialize(&event, (yaml_char_t*)anchor, (yaml_char_t*)tag,
            (yaml_char_t*)str->c_str(), str->strlen(), plain_implicit, quoted_implicit, style)) {
            // issue #3394: ensure that the string is a valid UTF-8 string before including in the exception output
            return doScalarEmissionError(**str, tag);
        }

        return emit("scalar", tag);
    }

    DLLLOCAL int emitScalar(const char* value, const char* tag, const char* anchor = nullptr,
                        bool plain_implicit = true, bool quoted_implicit = true,
                        yaml_scalar_style_t style = YAML_ANY_SCALAR_STYLE) {
        if (!yaml_scalar_event_initialize(&event, (yaml_char_t*)anchor, (yaml_char_t*)tag,
            (yaml_char_t*)value, -1, plain_implicit, quoted_implicit, style)) {
            // issue #3394: ensure that the string is a valid UTF-8 string before including in the exception output
            QoreString str(value);
            return doScalarEmissionError(str, tag);
        }

        return emit("scalar", tag);
    }

    DLLLOCAL int emitValue(const QoreString& str) {
        return emitScalar(str, YAML_STR_TAG, 0, true, true, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
    }

    DLLLOCAL int emitValue(int64 i) {
        QoreString tmp(QCS_UTF8);
        tmp.sprintf("%lld", i);
        return emitScalar(tmp, YAML_INT_TAG);
    }

    DLLLOCAL int emitValue(double f) {
        QoreString tmp(QCS_UTF8);
        if (std::isnan(f)) {
            tmp.set(".nan");
        } else if (std::isinf(f)) {
            tmp.set(f < 0 ? "-.inf" : ".inf");
        } else {
            yaml_format_finite_float(tmp, f);
        }
        return emitScalar(tmp, YAML_FLOAT_TAG);
    }

    DLLLOCAL int emitValue(const QoreNumberNode& n) {
        QoreString tmp(QCS_UTF8);
        n.toString(tmp, QORE_NF_SCIENTIFIC|QORE_NF_RAW);
        if (tmp == "inf")
            tmp.set("@inf@n");
        else if (tmp == "-inf")
            tmp.set("-@inf@n");
        else if (tmp == "nan")
            tmp.set("@nan@n");
        else
            tmp.concat('n');
        // append precision
        tmp.sprintf("{%d}", n.getPrec());
        //printd(5, "yaml emit number: %s\n", tmp.c_str());
        // issue #2343: to avoid ambiguity with single quoted strings, we always use the tag here
        return emitScalar(tmp, QORE_YAML_NUMBER_TAG, nullptr, false, false);
    }

    DLLLOCAL int emitValue(bool b) {
        QoreString tmp(QCS_UTF8);
        tmp.sprintf("%s", b ? "true" : "false");
        return emitScalar(tmp, YAML_BOOL_TAG);
    }

    DLLLOCAL int emitValue(const QoreListNode &l) {
        if (seqStart(block ? YAML_BLOCK_SEQUENCE_STYLE : YAML_FLOW_SEQUENCE_STYLE)) {
            return -1;
        }
        unsigned iteration = 0;
        ConstListIterator li(l);
        while (li.next()) {
            if (++iteration % 10 == 0) {
                if (qore_check_cancel(xsink, "YAML sequence emission")) {
                    return -1;
                }
            }
            if (emit(li.getValue())) {
                return -1;
            }
        }
        return seqEnd();
    }

    DLLLOCAL int emitValue(const QoreHashNode &h) {
        if (mapStart(block ? YAML_BLOCK_MAPPING_STYLE : YAML_FLOW_MAPPING_STYLE)) {
            return -1;
        }
        unsigned iteration = 0;
        ConstHashIterator hi(h);
        while (hi.next()) {
            if (++iteration % 10 == 0) {
                if (qore_check_cancel(xsink, "YAML mapping emission")) {
                    return -1;
                }
            }
            if (emitScalar(hi.getKey(), YAML_STR_TAG)) {
                return -1;
            }
            if (emit(hi.get())) {
                xsink->appendLastDescription(" (while serializing hash key '%s')", hi.getKey());
                return -1;
            }
        }
        return mapEnd();
    }

    DLLLOCAL int emitValue(const DateTime &d);

    DLLLOCAL int emitValue(const BinaryNode &b) {
        QoreString str(QCS_UTF8);
        str.concatBase64(&b);
        return emitScalar(str, YAML_BINARY_TAG, 0, false, false, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
    }

    DLLLOCAL int emit(const QoreValue& v);

    DLLLOCAL int emitNull() {
        return emitScalar(NullStr, YAML_NULL_TAG);
    }

    DLLLOCAL int emitSqlNull() {
        return emitScalar(SqlNullStr, QORE_YAML_SQLNULL_TAG);
    }

    DLLLOCAL void setCanonical(bool b = true) {
        yaml_emitter_set_canonical(&emitter, b);
    }

    DLLLOCAL void setUnicode(bool b = true) {
        yaml_emitter_set_unicode(&emitter, b);
    }

    DLLLOCAL bool getBlock() const {
        return block;
    }

protected:
    yaml_emitter_t emitter;
    QoreYamlWriteHandler& wh;

    bool block,
        implicit_start_doc,
        implicit_end_doc,
        emit_sqlnull;

    yaml_version_directive_t* yaml_ver = nullptr;

    DLLLOCAL int err(const char* fmt, ...) {
        QoreStringNode* desc = new QoreStringNode(QCS_UTF8);
        while (true) {
            va_list args;
            va_start(args, fmt);
            int rc = desc->vsprintf(fmt, args);
            va_end(args);
            if (!rc) {
                break;
            }
        }
        if (desc->strlen() > 255) {
            desc->terminate(255);
            desc->concat("...");
        }

        xsink->raiseException(QY_EMIT_ERR, desc);
        valid = false;
        return -1;
    }

    DLLLOCAL int emit(const char* event_str, const char* tag = nullptr) {
        if (!yaml_emitter_emit(&emitter, &event)) {
            if (tag) {
                return err("error emitting yaml %s %s event", event_str, tag);
            }
            return err("error emitting yaml %s event", event_str);
        }

        return 0;
    }

    DLLLOCAL int streamStart() {
        if (!yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING)) {
            return err("error initializing yaml stream start event");
        }

        return emit("stream start");
    }

    DLLLOCAL int streamEnd() {
        if (!yaml_stream_end_event_initialize(&event)) {
            return err("error initializing yaml stream end event");
        }

        return emit("stream end");
    }
};

class QoreYamlStringWriteHandler : public QoreYamlWriteHandler {
protected:
    QoreStringNode* str;

public:
    DLLLOCAL QoreYamlStringWriteHandler() : str(new QoreStringNode(QCS_UTF8)) {
    }

    DLLLOCAL ~QoreYamlStringWriteHandler() {
        if (str) {
            str->deref();
        }
    }

    DLLLOCAL QoreStringNode* take() {
        QoreStringNode* rv = str;
        str = nullptr;
        return rv;
    }

    DLLLOCAL int write(unsigned char* buffer, size_t size) {
        assert(str);
        str->concat((const char*)buffer, size);
        return 1;
    }
};

class QoreYamlParser : public QoreYamlBase {
public:
    //! Creates the parser
    /** @param str the YAML text
        @param xsink exception sink
        @param core_schema if true, plain scalars are resolved with the YAML 1.2 core schema only; see
        yaml_parse_core_schema_scalar()
    */
    DLLLOCAL QoreYamlParser(const QoreString& str, ExceptionSink* xsink, bool core_schema = false)
            : QoreYamlBase(xsink), discard(false), core_schema(core_schema) {
        yaml_parser_initialize(&parser);
        yaml_parser_set_input_string(&parser, (const unsigned char*)str.c_str(), str.strlen());
        yaml_parser_set_encoding(&parser, YAML_UTF8_ENCODING);
        valid = true;
    }

    DLLLOCAL QoreValue parse();

    DLLLOCAL ~QoreYamlParser() {
        discardEvent();
        yaml_parser_delete(&parser);

        for (auto& i : alias_map) {
            i.second.discard(nullptr);
        }
    }

protected:
    yaml_parser_t parser;
    bool discard;
    //! true if plain scalars are resolved with the YAML 1.2 core schema only
    bool core_schema;

    typedef std::map<std::string, QoreValue> alias_map_t;
    alias_map_t alias_map;

    DLLLOCAL void discardEvent() {
        if (discard) {
            yaml_event_delete(&event);
            discard = false;
        }
    }

    DLLLOCAL int getEvent() {
        discardEvent();

        if (!yaml_parser_parse(&parser, &event)) {
            valid = false;
            xsink->raiseException(QY_PARSE_ERR, "YAML parser error at line %d column %d: %s",
                (int)parser.problem_mark.line + 1, (int)parser.problem_mark.column + 1,
                parser.problem ? parser.problem : "unknown error");
            return -1;
        }
        //printd(5, "QoreYamlParser::getEvent() got %s event (%d)\n", get_event_name(event.type), event.type);

        discard = true;
        return 0;
    }

    DLLLOCAL int checkEvent(yaml_event_type_t type) {
        if (event.type != type) {
            xsink->raiseException(QY_PARSE_ERR, "expecting '%s' event; got '%s' event instead", get_event_name(type),
                get_event_name(event.type));
            return -1;
        }
        return 0;
    }

    DLLLOCAL int getCheckEvent(yaml_event_type_t type) {
        if (getEvent())
            return -1;

        return checkEvent(type);
    }

    DLLLOCAL QoreListNode* parseSeq();
    DLLLOCAL QoreHashNode* parseMap();
    DLLLOCAL QoreValue parseAlias();
    DLLLOCAL QoreValue parseScalar(bool favor_string = false);
    DLLLOCAL QoreValue parseNode(bool favor_string = false);
};

// Include new class headers
#include "QC_YamlSaxParser.h"
#include "QC_YamlDocumentIterator.h"
#include "QC_YamlStreamWriter.h"

#endif
