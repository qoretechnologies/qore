/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreJson.cpp

    JSON (JavaScript Object Notation) parsing and serialization

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

    Note: the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2.1, and GPL 2 or later; see README-LICENSE
    for more information.

    This code was previously maintained in the external json module
    (module-json, src/ql_json.qpp), then briefly in the library (lib/QoreJson.cpp)
    so that builtin binary modules could parse and generate JSON from C++.  It is
    back in the module now that the module C++ API mechanism makes that possible
    without promoting the codec into libqore -- see design/module-cpp-api.md and
    design/json-module-migration.md.
*/

#include <qore/Qore.h>
#include "QoreJson.h"
#include <qore/QoreSandboxManager.h>

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cctype>

// RFC 4627 JSON specification
// qore only supports JSON with UTF-8

// ctype functions require an argument representable as unsigned char (or EOF); a signed char with
// the high bit set -- any UTF-8 continuation byte -- is undefined behavior, so every ctype call
// below casts to unsigned char

// returns 0 for OK
static int cmp_rest_token(const char*& p, const char* tok) {
    p++;
    while (*tok)
        if ((*(p++)) != (*(tok++)))
            return -1;
    if (!*p || *p == ',' || *p == ']' || *p == '}')
        return 0;
    if (isblank(static_cast<unsigned char>(*p)) || (*p) == '\r' || (*p) == '\n') {
        ++p;
        return 0;
    }
    return -1;
}

static void skip_whitespace(const char*& buf, int& line_number) {
    while (*buf) {
        if (isblank(static_cast<unsigned char>(*buf)) || (*buf) == '\r') {
            ++buf;
            continue;
        }
        if ((*buf) == '\n') {
            ++line_number;
            ++buf;
            continue;
        }
        break;
    }
}

// '"' has already been read and the buffer is set to this character
static int get_json_string_token(QoreString& str, const char*& buf, int& line_number,
        QoreSandboxManager* sm, ExceptionSink* xsink) {
    // increment buffer to first character of string
    buf++;
    // iteration counter for periodic interrupt checks (sandbox only); a single string token is
    // bounded only by the size of the document, so this loop needs its own cancellation point --
    // the container loops in get_json_object()/get_json_array() only check once per element
    unsigned iteration = 0;
    while (*buf) {
        if (sm && (++iteration % JSON_INTERRUPT_CHECK_INTERVAL) == 0) {
            if (qore_check_cancel(xsink, "parsing JSON string")) {
                return -1;
            }
        }
        if (*buf == '"') {
            buf++;
            return 0;
        }
        if (*buf == '\\') {
            buf++;
            if (*buf == '"' || *buf == '/' || *buf == '\\') {
                str.concat(*buf);
                ++buf;
                continue;
            }
            if (*buf == 'a') {
                str.concat('\a');
            } else if (*buf == 'b') {
                str.concat('\b');
            } else if (*buf == 'f') {
                str.concat('\f');
            } else if (*buf == 'n') {
                str.concat('\n');
            } else if (*buf == 'r') {
                str.concat('\r');
            } else if (*buf == 't') {
                str.concat('\t');
            } else if (*buf == 'v') {
                str.concat('\v');
            } else if (*buf == 'u') { // expect a unicode character specification (possibly a surrogate pair)
                ++buf;
                // check for 4 hex digits
                if (isxdigit(static_cast<unsigned char>(*buf)) && isxdigit(static_cast<unsigned char>(*(buf + 1)))
                    && isxdigit(static_cast<unsigned char>(*(buf + 2))) && isxdigit(static_cast<unsigned char>(*(buf + 3)))) {
                    char unicode[5];
                    strncpy(unicode, buf, 4);
                    unicode[4] = '\0';
                    unsigned code = strtoul(unicode, 0, 16);
                    buf += 3;
                    // combine a UTF-16 surrogate pair: astral-plane characters (> U+FFFF) are encoded in
                    // JSON as a high surrogate \uD800-DBFF followed by a low surrogate \uDC00-DFFF
                    if (code >= 0xD800 && code <= 0xDBFF) {
                        // the fixed-offset reads below are bounded by && short-circuit evaluation:
                        // *(buf + N) is only dereferenced when buf+1..buf+(N-1) all matched non-NUL
                        // characters, so at most the NUL terminator is read, never past it (same idiom
                        // as the 4-hex-digit check above for the first \u unit)
                        if (*(buf + 1) == '\\' && *(buf + 2) == 'u'
                            && isxdigit(static_cast<unsigned char>(*(buf + 3))) && isxdigit(static_cast<unsigned char>(*(buf + 4)))
                            && isxdigit(static_cast<unsigned char>(*(buf + 5))) && isxdigit(static_cast<unsigned char>(*(buf + 6)))) {
                            char low_unicode[5];
                            strncpy(low_unicode, buf + 3, 4);
                            low_unicode[4] = '\0';
                            unsigned low = strtoul(low_unicode, 0, 16);
                            if (low < 0xDC00 || low > 0xDFFF) {
                                xsink->raiseException("JSON-PARSE-ERROR", "invalid JSON string: high surrogate "
                                    "\\u%04X followed by \\u%04X, which is not a low surrogate, at line %d",
                                    code, low, line_number);
                                return -1;
                            }
                            code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                            buf += 6;
                        } else {
                            xsink->raiseException("JSON-PARSE-ERROR", "invalid JSON string: unpaired high "
                                "surrogate \\u%04X at line %d", code, line_number);
                            return -1;
                        }
                    } else if (code >= 0xDC00 && code <= 0xDFFF) {
                        xsink->raiseException("JSON-PARSE-ERROR", "invalid JSON string: unpaired low surrogate "
                            "\\u%04X at line %d", code, line_number);
                        return -1;
                    }
                    if (str.concatUnicode(code, xsink)) {
                        break;
                    }
                } else {
                    str.concat("\\u");
                }
            } else { // otherwise just concatenate the characters
                str.concat('\\');
                str.concat(*buf);
            }
            ++buf;
            continue;
        }
        if (*buf == '\n') {
            line_number++;
        }
        str.concat(*buf);
        ++buf;
    }
    xsink->raiseException("JSON-PARSE-ERROR", "premature end of input at line %d while parsing JSON string",
        line_number);
    return -1;
}

static QoreValue get_json_value(const char*& buf, int& line_number, const QoreEncoding* enc, bool& empty,
        int depth, QoreSandboxManager* sm, ExceptionSink* xsink);

// '{' has already been read and the buffer is set to this character
static QoreHashNode* get_json_object(const char*& buf, int& line_number, const QoreEncoding* enc,
        int depth, QoreSandboxManager* sm, ExceptionSink* xsink) {
    // Check sandbox-conditional depth limit
    if (sm && depth >= JSON_MAX_NESTING_DEPTH) {
        xsink->raiseException("JSON-PARSE-ERROR",
            "maximum nesting depth (%d) exceeded at line %d",
            JSON_MAX_NESTING_DEPTH, line_number);
        return nullptr;
    }
    // increment buffer to first character of object description
    buf++;
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);

    // get either string or '}'
    skip_whitespace(buf, line_number);

    if (*buf == '}') {
        buf++;
        return h.release();
    }

    // Iteration counter for periodic interrupt checks (sandbox only)
    unsigned iteration = 0;

    while (*buf) {
        // Periodic interrupt check in sandbox mode
        if (sm && (++iteration % JSON_INTERRUPT_CHECK_INTERVAL) == 0) {
            if (qore_check_cancel(xsink, "parsing JSON object")) {
                return nullptr;
            }
        }

        if (*buf != '"') {
            //printd(5, "*buf='%c'\n", *buf);
            if (h->size()) {
                xsink->raiseException("JSON-PARSE-ERROR", "unexpected text encountered at line %d while parsing JSON "
                    "object (expecting '\"' for key string)", line_number);
            } else {
                xsink->raiseException("JSON-PARSE-ERROR", "unexpected text encountered at line %d while parsing JSON "
                    "object (expecting '\" or '}'')", line_number);
            }
            break;
        }

        // get key
        QoreString str(enc);
        if (get_json_string_token(str, buf, line_number, sm, xsink))
            break;

        //printd(5, "get_json_object() key=%s\n", str.c_str());

        skip_whitespace(buf, line_number);
        if (*buf != ':') {
            //printd(5, "*buf='%c'\n", *buf);
            xsink->raiseException("JSON-PARSE-ERROR", "unexpected text encountered at line %d while parsing JSON "
                "object (expecting ':')", line_number);
            break;
        }
        buf++;
        skip_whitespace(buf, line_number);

        // get value
        bool empty = false;
        QoreValue val = get_json_value(buf, line_number, enc, empty, depth + 1, sm, xsink);
        if (*xsink) {
            break;
        }
        if (empty) {
            xsink->raiseException("JSON-PARSE-ERROR", "premature end of input at line %d while parsing JSON object "
                "(expecting JSON value)", line_number);
            break;
        }
        h->setKeyValue(&str, val, xsink);

        skip_whitespace(buf, line_number);
        if (*buf == '}') {
            buf++;
            return h.release();
        }

        if (*buf != ',') {
            xsink->raiseException("JSON-PARSE-ERROR", "unexpected text encountered at line %d while parsing JSON "
                "object (expecting ',' or '}')", line_number);
            break;
        }
        ++buf;

        skip_whitespace(buf, line_number);
    }
    return 0;
}

// '[' has already been read and the buffer is set to this character
static AbstractQoreNode* get_json_array(const char*& buf, int& line_number, const QoreEncoding* enc,
        int depth, QoreSandboxManager* sm, ExceptionSink* xsink) {
    // Check sandbox-conditional depth limit
    if (sm && depth >= JSON_MAX_NESTING_DEPTH) {
        xsink->raiseException("JSON-PARSE-ERROR",
            "maximum nesting depth (%d) exceeded at line %d",
            JSON_MAX_NESTING_DEPTH, line_number);
        return nullptr;
    }
    // increment buffer to first character of array description
    ++buf;
    ReferenceHolder<QoreListNode> l(new QoreListNode(autoTypeInfo), xsink);

    skip_whitespace(buf, line_number);
    if (*buf == ']') {
        ++buf;
        return l.release();
    }

    // Iteration counter for periodic interrupt checks (sandbox only)
    unsigned iteration = 0;

    while (*buf) {
        // Periodic interrupt check in sandbox mode
        if (sm && (++iteration % JSON_INTERRUPT_CHECK_INTERVAL) == 0) {
            if (qore_check_cancel(xsink, "parsing JSON array")) {
                return nullptr;
            }
        }
        //printd(5, "before get_json_value() buf=%s\n", buf);
        bool empty = false;
        QoreValue val = get_json_value(buf, line_number, enc, empty, depth + 1, sm, xsink);
        if (*xsink) {
            break;
        }
        if (empty) {
            xsink->raiseException("JSON-PARSE-ERROR", "premature end of input at line %d while parsing JSON array "
                "(expecting JSON value)", line_number);
            break;
        }
        //printd(5, "after get_json_value() buf=%s\n", buf);
        l->push(val, xsink);

        skip_whitespace(buf, line_number);
        if (*buf == ']') {
            buf++;
            return l.release();
        }

        if (*buf != ',') {
            //printd(5, "*buf='%c'\n", *buf);
            xsink->raiseException("JSON-PARSE-ERROR", "unexpected text encountered at line %d while parsing JSON "
                "array (expecting ',' or ']')", line_number);
            return 0;
        }
        ++buf;

        skip_whitespace(buf, line_number);
    }
    return 0;
}

static QoreValue get_json_value(const char*& buf, int& line_number, const QoreEncoding* enc, bool& empty,
        int depth, QoreSandboxManager* sm, ExceptionSink* xsink) {
    assert(!empty);
    // skip whitespace
    skip_whitespace(buf, line_number);
    if (!*buf) {
        empty = true;
        return QoreValue();
    }

    // can expect: 't'rue, 'f'alse, '{', '[', '"'string...", integer, '.'digits
    if (*buf == '{') {
        return get_json_object(buf, line_number, enc, depth, sm, xsink);
    }

    if (*buf == '[') {
        return get_json_array(buf, line_number, enc, depth, sm, xsink);
    }

    if (*buf == '"') {
        QoreStringNodeHolder str(new QoreStringNode(enc));
        return get_json_string_token(*(*str), buf, line_number, sm, xsink) ? 0 : str.release();
    }

    if (isdigit(static_cast<unsigned char>(*buf)) || (*buf) == '.' || (*buf) == '-') {
        // temporarily use a QoreString
        QoreString str;
        bool has_dot;
        if (*buf == '.') {
            // add a leading zero
            str.concat("0.");
            has_dot = true;
        } else {
            str.concat(*buf);
            has_dot = false;
        }
        ++buf;
        bool has_e = false;
        while (*buf) {
            if (*buf == '.') {
                if (has_dot) {
                    xsink->raiseException("JSON-PARSE-ERROR", "unexpected '.' in floating point number (too many '.' "
                        "characters)");
                    return 0;
                }
                has_dot = true;
                // if another token follows then break but do not increment buffer position
            } else if (*buf == ',' || *buf == '}' || *buf == ']') {
                break;
                // if whitespace follows then increment buffer position and break
            } else if (isblank(static_cast<unsigned char>(*buf)) || (*buf) == '\r') {
                ++buf;
                break;
                // if a newline follows then  increment buffer position and line number and break
            } else if ((*buf) == '\n') {
                ++buf;
                ++line_number;
                break;
            } else if (*buf == 'e' || *buf == 'E') {
                if (has_e) {
                    xsink->raiseException("JSON-PARSE-ERROR", "unexpected second exponent marker '%c' in number", *buf);
                    return QoreValue();
                }
                has_e = true;
                // handle optional sign after exponent marker
                str.concat(*buf);
                ++buf;
                if (*buf == '+' || *buf == '-') {
                    str.concat(*buf);
                    ++buf;
                }
                continue;
            } else if (!isdigit(static_cast<unsigned char>(*buf))) {
                xsink->raiseException("JSON-PARSE-ERROR", "unexpected character '%c' in number", *buf);
                return QoreValue();
            }
            str.concat(*buf);
            ++buf;
        }
        if (has_dot || has_e) {
            return QoreValue(q_strtod(str.c_str()));
        }
        return strtoll(str.c_str(), 0, 10);
    }

    if ((*buf) == 't') {
        if (!cmp_rest_token(buf, "rue"))
            return true;
    } else if ((*buf) == 'f') {
        if (!cmp_rest_token(buf, "alse"))
            return false;
    } else if ((*buf) == 'n') {
        if (!cmp_rest_token(buf, "ull"))
            return nothing();
    }
    //printd(5, "buf=%s\n", buf);

    xsink->raiseException("JSON-PARSE-ERROR", "invalid input at line %d; unable to parse JSON value", line_number);
    return QoreValue();
}

static int do_json_value(QoreString* str, QoreValue v, int format, int depth,
        QoreSandboxManager* sm, ExceptionSink* xsink);

static int do_json_list(ExceptionSink* xsink, QoreString* str, const QoreListNode* l, int format,
        int depth, QoreSandboxManager* sm, unsigned offset = 0) {
    assert(l);
    str->concat("[");
    ConstListIterator li(l);
    QoreString tmp(str->getEncoding());
    // Iteration counter for periodic interrupt checks (sandbox only)
    unsigned iteration = 0;
    while (li.next()) {
        if (li.index() < offset) {
            continue;
        }
        // Periodic interrupt check in sandbox mode
        if (sm && (++iteration % JSON_INTERRUPT_CHECK_INTERVAL) == 0) {
            if (qore_check_cancel(xsink, "serializing JSON list")) {
                return -1;
            }
        }
        tmp.clear();
        if (do_json_value(&tmp, li.getValue(), format < 0 ? format : format + 2, depth + 1, sm, xsink)) {
            return -1;
        }
        if (format >= 0) {
            str->concat('\n');
            str->addch(' ', format + 2);
        }
        str->sprintf("%s", tmp.c_str());
        if (!li.last()) {
            str->concat(",");
        }
    }
    if (!l->empty() && format >= 0) {
        str->concat('\n');
        str->addch(' ', format);
    }
    str->concat("]");
    return 0;
}

static int do_json_string_intern(ExceptionSink* xsink, QoreString* str, const char* utf8_str, int format,
        QoreSandboxManager* sm) {
    str->concat('"');
    qore_size_t i = str->size();
    str->concatEscape(utf8_str, '"', '\\');

    // iteration counter for periodic interrupt checks (sandbox only); the scan below is bounded
    // only by the length of the string being serialized, and each iteration does encoding-aware
    // character-length work, so it needs its own cancellation point
    unsigned iteration = 0;

    // http://tools.ietf.org/html/rfc4627
    // encode all control characters in the string concatenated
    while (i < str->size()) {
        if (sm && (++iteration % JSON_INTERRUPT_CHECK_INTERVAL) == 0) {
            if (qore_check_cancel(xsink, "serializing JSON string")) {
                return -1;
            }
        }
        // see if we have a single-byte character; do not try to look at bytes within multi-byte characters
        qore_size_t cl = q_get_char_len(str->getEncoding(), str->c_str() + i, str->size() - i, xsink);
        if (*xsink)
            return -1;
        if (cl > 1) {
            i += cl;
            continue;
        }

        unsigned char c = (*str)[i];
        if (c < 32) {
            switch (c) {
                case 7: str->replace(i, 1, "\\a"); ++i; break; // BEL
                case 8: str->replace(i, 1, "\\b"); ++i; break; // BS
                case 9: str->replace(i, 1, "\\t"); ++i; break; // HT
                case 10: str->replace(i, 1, "\\n"); ++i; break; // LF
                case 11: str->replace(i, 1, "\\v"); ++i; break; // VT
                case 12: str->replace(i, 1, "\\f"); ++i; break; // FF
                case 13: str->replace(i, 1, "\\r"); ++i; break; // CR
                default: {
                    // c < 32 here, so the escape is always exactly "\uXXXX" (6 chars + NUL)
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<int>(c));
                    str->replace(i, 1, buf); i += 5;
                    break;
                }
            }
        }
        ++i;
    }

    str->concat('"');
    return 0;
}

static int do_json_string(ExceptionSink* xsink, QoreString* str, const QoreString* vstr, int format,
        QoreSandboxManager* sm) {
   TempEncodingHelper t(vstr, str->getEncoding(), xsink);
   if (*xsink)
      return -1;

   return do_json_string_intern(xsink, str, t->c_str(), format, sm);
}

static int do_json_value(QoreString* str, QoreValue v, int format, int depth,
        QoreSandboxManager* sm, ExceptionSink* xsink) {
    if (v.isNullOrNothing()) {
        str->concat("null");
        return 0;
    }

    qore_type_t vtype = v.getType();

    if (vtype == NT_LIST) {
        // Check sandbox-conditional depth limit (containers only)
        if (sm && depth >= JSON_MAX_NESTING_DEPTH) {
            xsink->raiseException("JSON-SERIALIZATION-ERROR",
                "maximum nesting depth (%d) exceeded during serialization",
                JSON_MAX_NESTING_DEPTH);
            return -1;
        }
        return do_json_list(xsink, str, v.get<const QoreListNode>(), format, depth, sm);
    }

    if (vtype == NT_HASH) {
        // Check sandbox-conditional depth limit (containers only)
        if (sm && depth >= JSON_MAX_NESTING_DEPTH) {
            xsink->raiseException("JSON-SERIALIZATION-ERROR",
                "maximum nesting depth (%d) exceeded during serialization",
                JSON_MAX_NESTING_DEPTH);
            return -1;
        }
        const QoreHashNode* h = v.get<const QoreHashNode>();
        str->concat("{");
        ConstHashIterator hi(h);
        QoreString tmp(str->getEncoding());
        // Iteration counter for periodic interrupt checks (sandbox only)
        unsigned iteration = 0;
        while (hi.next()) {
            // Periodic interrupt check in sandbox mode
            if (sm && (++iteration % JSON_INTERRUPT_CHECK_INTERVAL) == 0) {
                if (qore_check_cancel(xsink, "serializing JSON object")) {
                    return -1;
                }
            }
            tmp.clear();
            if (do_json_value(&tmp, hi.get(), format == -1 ? format : format + 2, depth + 1, sm, xsink)) {
                return -1;
            }
            if (format != -1) {
                str->concat('\n');
                str->addch(' ', format + 2);
            }
            QoreString tmpkey(hi.getKey());
            if (do_json_string(xsink, str, &tmpkey, format, sm)) {
                return -1;
            }
            str->concat(':');
            if (format >= 0) {
                str->concat(' ');
            }
            str->concat(tmp.c_str(), tmp.size());
            //str->sprintf("\"%s\":%s", hi.getKey(), tmp.c_str());
            if (!hi.last()) {
                str->concat(",");
            }
        }
        if (!h->empty() && format >= 0) {
            str->concat('\n');
            str->addch(' ', format);
        }
        str->concat("}");
        return 0;
    }

    if (vtype == NT_STRING) {
        QoreStringNodeValueHelper s(v);
        return do_json_string(xsink, str, *s, format, sm);
    }

    if (vtype == NT_INT) {
        str->sprintf("%lld", v.getAsBigInt());
        return 0;
    }

    if (vtype == NT_FLOAT) {
        double f = v.getAsFloat();
        // check for nan, +/-inf and serialize as null
        if (std::isnan(f) || std::isinf(f)) {
            str->concat("null");
        } else {
            str->sprintf("%.25g", f);
            // apply noise reduction algorithm
            qore_apply_rounding_heuristic(*str, 6, 8);
        }
        return 0;
    }

    if (vtype == NT_NUMBER) {
        const QoreNumberNode* n = v.get<const QoreNumberNode>();
        if (!n->ordinary()) {
            str->concat("null");
        } else {
            n->getStringRepresentation(*str);
        }
        return 0;
    }

    if (vtype == NT_BOOLEAN) {
        str->concat(v.getAsBool() ? "true" : "false");
        return 0;
    }

    if (vtype == NT_DATE) {
        const DateTimeNode* date = v.get<const DateTimeNode>();
        // ensure that all date/time values are reported in the current time zone
        // for simplicity's sake (particularly because json does not have a native date/time format)
        qore_tm info;
        date->getInfo(currentTZ(), info);

        // this will be serialized as a string
        str->concat('"');
        if (date->isRelative()) {
            str->concat('P');
            if (date->hasValue()) {
                if (info.year) {
                    str->sprintf("%dY", info.year);
                }
                if (info.month) {
                    str->sprintf("%dM", info.month);
                }
                if (info.day) {
                    str->sprintf("%dD", info.day);
                }

                bool has_t = false;
                if (info.hour) {
                    str->sprintf("T%dH", info.hour);
                    has_t = true;
                }
                if (info.minute) {
                    if (!has_t) {
                        str->concat('T');
                        has_t = true;
                    }
                    str->sprintf("%dM", info.minute);
                }
                if (info.second) {
                    if (!has_t) {
                        str->concat('T');
                        has_t = true;
                    }
                    str->sprintf("%dS", info.second);
                }
                if (info.us) {
                    if (!has_t) {
                        str->concat('T');
                        has_t = true;
                    }
                    str->sprintf("%du", info.us);
                }
            } else {
                str->concat("0D");
            }
        } else {
#ifndef SECS_PER_MINUTE
#define SECS_PER_MINUTE          60
#endif
#ifndef SECS_PER_HOUR
#define SECS_PER_HOUR            (SECS_PER_MINUTE * 60)
#endif
            // issue #2655 include a 'T' between the date and time as per ISO-8601
            str->sprintf("%04d-%02d-%02dT%02d:%02d:%02d.%06d", info.year, info.month, info.day, info.hour,
                info.minute, info.second, info.us);
            if (!info.utc_secs_east) {
                str->concat('Z');
            } else {
                // issue #2655 do not include a space before the UTC offset
                str->concat(info.utc_secs_east < 0 ? '-' : '+');
                if (info.utc_secs_east < 0) {
                    info.utc_secs_east = -info.utc_secs_east;
                }
                int h = info.utc_secs_east / SECS_PER_HOUR;
                // the remaining seconds after hours
                int r = info.utc_secs_east % SECS_PER_HOUR;
                // minutes
                int m = r / SECS_PER_MINUTE;
                // we have already output the hour sign above
                str->sprintf("%02d:%02d", h < 0 ? -h : h, m);
                // see if there are any seconds
                int s = info.utc_secs_east - h * SECS_PER_HOUR - m * SECS_PER_MINUTE;
                if (s) {
                    str->sprintf(":%02d", s);
                }
            }
        }
        str->concat('"');
        return 0;
    }

    if (vtype == NT_BINARY) {
        str->concat('\"');
        str->concatBase64(v.get<const BinaryNode>());
        str->concat('\"');
        return 0;
    }

    xsink->raiseException("JSON-SERIALIZATION-ERROR", "don't know how to serialize type '%s'", v.getFullTypeName());
    return -1;
}

QoreValue parse_json(const QoreString* str, ExceptionSink* xsink) {
    int line_number = 1;
    const char* buf = str->c_str();
    const unsigned char* data = reinterpret_cast<const unsigned char*>(buf);
    if (str->size() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) { // Skip UTF-8 BOM.
        buf = reinterpret_cast<const char*>(data+3);
    } else if (str->size() >= 2) { // Skip Unicode BOM.
        if ((data[0] == 0xFE && data[1] == 0xFF) || (data[0] == 0xFF && data[1] == 0xFE)) {
            buf = reinterpret_cast<const char*>(data+2);
        }
    }

    // Get sandbox manager for depth checks
    QoreSandboxManagerHelper smh;

    // ignore empty values; return NOTHIN in this case
    bool empty = false;
    ValueHolder rv(get_json_value(buf, line_number, str->getEncoding(), empty, 0, smh.get(), xsink), xsink);
    if (rv && *buf) {
        // check for excess text after JSON data
        skip_whitespace(buf, line_number);
        if (*buf) {
            xsink->raiseException("JSON-PARSE-ERROR", "extra text after JSON data on line %d", line_number);
            return QoreValue();
        }
    }
    return rv.release();
}

QoreStringNode* make_json(QoreValue data, int format, const QoreEncoding* enc, ExceptionSink* xsink) {
    QoreStringNodeHolder str(new QoreStringNode(enc ? enc : QCS_UTF8));
    QoreSandboxManagerHelper smh;
    if (do_json_value(*str, data, format & JGF_ADD_FORMATTING ? 0 : -1, 0, smh.get(), xsink)) {
        return nullptr;
    }
    return str.release();
}

int json_serialize_value(QoreString& str, QoreValue data, int indent, ExceptionSink* xsink) {
    QoreSandboxManagerHelper smh;
    return do_json_value(&str, data, indent, 0, smh.get(), xsink);
}

int json_serialize_list(QoreString& str, const QoreListNode* l, int indent, ExceptionSink* xsink,
        unsigned offset) {
    assert(l);
    QoreSandboxManagerHelper smh;
    return do_json_list(xsink, &str, l, indent, 0, smh.get(), offset);
}
