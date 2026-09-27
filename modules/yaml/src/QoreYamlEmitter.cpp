/* indent-tabs-mode: nil -*- */
/*
    yaml Qore module

    Copyright (C) 2010 - 2026 Qore Technologies, s.r.o.

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

#include "yaml-module.h"

const char* QY_EMIT_ERR = "YAML-EMITTER-ERROR";

static int qore_yaml_write_handler(QoreYamlWriteHandler* wh, unsigned char* buffer, size_t size) {
    return wh->write(buffer, size);
}

QoreYamlEmitter::QoreYamlEmitter(QoreYamlWriteHandler& wh, int flags, int width, int indent, ExceptionSink* xsink)
        : QoreYamlBase(xsink), wh(wh), block(flags & QYE_BLOCK_STYLE),
            implicit_start_doc(!(flags & QYE_EXPLICIT_START_DOC)),
            implicit_end_doc(!(flags & QYE_EXPLICIT_END_DOC)),
            emit_sqlnull(flags & QYE_EMIT_SQLNULL) {
    if (!yaml_emitter_initialize(&emitter)) {
        err("unknown error initializing yaml emitter");
        return;
    }

    if (flags & QYE_VER_1_1 && flags & QYE_VER_1_2) {
        err("cannot set both Yaml1_1 and Yaml1_2 version flags");
        return;
    }

    if (flags & QYE_CANONICAL)
        setCanonical();

    if (!(flags & QYE_ESCAPE_UNICODE))
        setUnicode();

    if (flags & QYE_VER_1_2) {
        yaml_ver = &yaml_ver_1_2;
    } else if (flags & QYE_VER_1_1) {
        yaml_ver = &yaml_ver_1_1;
    } else if (flags & QYE_VER_1_0) {
        yaml_ver = &yaml_ver_1_0;
    }

    yaml_emitter_set_output(&emitter, (yaml_write_handler_t*)qore_yaml_write_handler, &wh);

    //printd(5, "QoreYamlEmitter::QoreYamlEmitter() indent=%d width=%d\n", indent, width);
    yaml_emitter_set_indent(&emitter, indent);
    yaml_emitter_set_width(&emitter, width);

    if (!streamStart() && !docStart()) {
        valid = true;
    }
}

int QoreYamlEmitter::emit(const QoreValue& v) {
    switch (v.getType()) {
        case NT_STRING: {
            QoreStringNodeValueHelper str(v);
            return emitValue(**str);
        }

        case NT_INT:
            return emitValue(v.getAsBigInt());

        case NT_FLOAT:
            return emitValue(v.getAsFloat());

        case NT_NUMBER:
            return emitValue(*v.get<const QoreNumberNode>());

        case NT_BOOLEAN:
            return emitValue(v.getAsBool());

        case NT_LIST:
            return emitValue(*v.get<const QoreListNode>());

        case NT_HASH:
            return emitValue(*v.get<const QoreHashNode>());

        case NT_DATE:
            return emitValue(*v.get<const DateTimeNode>());

        case NT_BINARY:
            return emitValue(*v.get<const BinaryNode>());

        case NT_NULL:
            if (emit_sqlnull) {
                return emitSqlNull();
            }
            // fall down to nothing

        case NT_NOTHING:
            return emitNull();

        case NT_OBJECT:
            err("Cannot convert object of class '%s' to YAML", v.get<const QoreObject>()->getClassName());
            return -1;

        default:
            err("cannot convert Qore type '%s' to YAML", v.getFullTypeName());
            return -1;
    }
}

int QoreYamlEmitter::emitValue(const DateTime &d) {
    qore_tm info;
    d.getInfo(info);

    QoreString str(QCS_UTF8);

    if (d.isRelative()) {
        str.concat('P');
        if (d.hasValue()) {
            if (info.year)
                str.sprintf("%dY", info.year);
            if (info.month)
                str.sprintf("%dM", info.month);
            if (info.day)
                str.sprintf("%dD", info.day);

            bool has_t = false;

            if (info.hour) {
                str.sprintf("T%dH", info.hour);
                has_t = true;
            }
            if (info.minute) {
                if (!has_t) {
                    str.concat('T');
                    has_t = true;
                }
                str.sprintf("%dM", info.minute);
            }
            if (info.second) {
                if (!has_t) {
                    str.concat('T');
                    has_t = true;
                }
                str.sprintf("%dS", info.second);
            }
            if (info.us) {
                if (!has_t) {
                    str.concat('T');
                    has_t = true;
                }
                str.sprintf("%du", info.us);
            }
        } else {
            str.concat("0D");
        }

        return emitScalar(str, QORE_YAML_DURATION_TAG);
    }

    // shorthand type name
    if (emitter.canonical) {
        d.format(str, "YYYY-MM-DDTHH:mm:SS.xx");
    } else {
        d.format(str, "YYYY-MM-DD");
        if (!info.isTimeNull() || info.secsEast()) {
            // use spaces for enhanced readability
            if (!info.us) {
                d.format(str, " HH:mm:SS.");
            } else if (!(info.us % 1000)) {
                d.format(str, " HH:mm:SS.ms");
            } else {
                d.format(str, " HH:mm:SS.xx");
            }
        }
    }

    // remove trailing zeros from the microseconds component, but only if the
    // string actually has a fractional-seconds component — otherwise this would
    // strip a trailing '0' from date-only output like "2026-04-10" → "2026-04-1"
    if (strchr(str.c_str(), '.')) {
        str.trim_trailing('0');
        // remove empty decimal point if us == 0
        str.trim_trailing('.');
    }

    // if not emitting the canonical format and there is a time zone offset,
    // then add a space
    if (!emitter.canonical) {
        if (!info.isTimeNull() || info.secsEast()) {
            str.concat(' ');
            // add time zone offset (or "Z")
            d.format(str, "Z");
        }
    } else {
        d.format(str, "Z");
    }

    // issue #2343: to avoid ambiguity with single quoted strings, we always use the tag here
    return emitScalar(str, YAML_TIMESTAMP_TAG);
}
