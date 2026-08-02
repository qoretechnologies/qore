# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file A2aJcs.qc @brief JSON Canonicalization Scheme (RFC 8785) support

/*  A2aJcs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
*/

#! the %A2aClient namespace holds all public definitions in the %A2aClient module
public namespace A2aClient {

#! The largest exponent for which ECMAScript prints a number in positional rather than exponential notation
/** @since A2aClient 1.0
*/
public const A2aJcsMaxPositionalExponent = 21;

#! The most significant digits any IEEE 754 double needs for a round trip
/** @since A2aClient 1.0
*/
public const A2aJcsMaxSignificantDigits = 17;

#! Serializes a value to its JSON Canonicalization Scheme (RFC 8785) form
/** JCS produces a byte-for-byte reproducible JSON serialization, which is what makes a detached signature over
    a JSON document verifiable by an independent implementation.  It differs from
    @ref Qore::Json::make_json() "make_json()" in three ways that matter for signatures:

    - object members are ordered by their key's UTF-16 code units, not by insertion order
    - strings use the minimal escaping RFC 8785 mandates
    - numbers use the ECMAScript \c Number::toString algorithm

    @param v the value to canonicalize; @ref nothing and @ref null both serialize as \c null

    @return the canonical JSON text

    @throw JCS-ERROR if the value cannot be canonicalized: RFC 8785 has no representation for a non-finite
    number, and no JSON representation exists for a date, binary, object, or closure

    @note Object keys are compared as UTF-16 code units, so a key containing a character outside the Basic
    Multilingual Plane sorts by its surrogate values, ahead of keys starting at \c U+E000.  This is what the
    specification requires and is not the same as ordering by code point.

    @since A2aClient 1.0
*/
public string sub a2aCanonicalizeJson(auto v) {
    return a2aJcsSerialize(v);
}

#! Serializes one value in canonical form
/** @param v the value to serialize

    @return the canonical JSON text for the value

    @since A2aClient 1.0
*/
public string sub a2aJcsSerialize(auto v) {
    switch (v.typeCode()) {
        case NT_NOTHING:
        case NT_NULL:
            return "null";

        case NT_BOOLEAN:
            return v ? "true" : "false";

        case NT_INT:
            return sprintf("%d", v);

        case NT_FLOAT:
        case NT_NUMBER:
            return a2aJcsNumber(v);

        case NT_STRING:
            return a2aJcsString(v);

        case NT_LIST: {
            list<string> items = (map a2aJcsSerialize($1), v) ?? ();
            return "[" + items.join(",") + "]";
        }

        case NT_HASH: {
            list<string> members = (map sprintf("%s:%s", a2aJcsString($1), a2aJcsSerialize(v{$1})),
                a2aJcsSortKeys(keys v)) ?? ();
            return "{" + members.join(",") + "}";
        }
    }
    throw "JCS-ERROR", sprintf("cannot canonicalize a value of type %y; RFC 8785 covers only the JSON types",
        v.type());
}

#! Sorts object keys by their UTF-16 code units as RFC 8785 requires
/** @param key_list the keys to sort

    @return the keys in canonical order

    @note The keys are compared as UTF-16BE byte sequences, which is equivalent to comparing UTF-16 code units
    and is not the same as comparing code points.

    @since A2aClient 1.0
*/
public list<string> sub a2aJcsSortKeys(softlist<string> key_list) {
    if (key_list.size() < 2) {
        return key_list;
    }
    # compare the UTF-16BE encodings of the keys; this is what "UTF-16 code unit order" means
    return sort(key_list, int sub (string l, string r) {
        string lo = binary(convert_encoding(l, "UTF-16BE")).toHex();
        string ro = binary(convert_encoding(r, "UTF-16BE")).toHex();
        return lo <=> ro;
    });
}

#! Serializes a string with the minimal escaping RFC 8785 mandates
/** Only the quotation mark, the reverse solidus, and the C0 control characters are escaped; everything else,
    including the solidus and all non-ASCII characters, is emitted literally as UTF-8.

    @param str the string to serialize

    @return the quoted, escaped string

    @since A2aClient 1.0
*/
public string sub a2aJcsString(string str) {
    string rv = "\"";
    # note: split("") does not split a string into characters in Qore, it returns the whole string;
    # length() is the character count, size() is the byte count
    int len = str.length();
    for (int i = 0; i < len; ++i) {
        string c = str.substr(i, 1);
        int cp = c.getUnicode();
        switch (cp) {
            case 0x22: rv += "\\\""; break;
            case 0x5C: rv += "\\\\"; break;
            case 0x08: rv += "\\b"; break;
            case 0x0C: rv += "\\f"; break;
            case 0x0A: rv += "\\n"; break;
            case 0x0D: rv += "\\r"; break;
            case 0x09: rv += "\\t"; break;
            default:
                if (cp < 0x20) {
                    rv += sprintf("\\u%04x", cp);
                } else {
                    rv += c;
                }
                break;
        }
    }
    return rv + "\"";
}

#! The significand of the largest finite IEEE 754 double, as digits
/** @since A2aClient 1.0
*/
public const A2aJcsMaxDoubleDigits = "17976931348623157";

#! The decimal exponent of the largest finite IEEE 754 double
/** @since A2aClient 1.0
*/
public const A2aJcsMaxDoubleExponent = 308;

#! Returns True if the given scientific-notation literal denotes a value larger than any finite double
/** Qore's @ref Qore::float() "float()" saturates at the largest finite double rather than returning an
    infinity, so a representation that is mathematically too large parses back to the largest finite double and
    appears to round trip.  Such a candidate is always too short and must be rejected.

    @param repr a literal in \c "d.ddde[+-]xx" form

    @return @ref True if the literal denotes a value larger than the largest finite double

    @since A2aClient 1.0
*/
public bool sub a2aJcsOverflowsDouble(string repr) {
    list<string> parts = repr.split("e");
    int exp10 = parts[1].toInt();
    if (exp10 < A2aJcsMaxDoubleExponent) {
        return False;
    }
    if (exp10 > A2aJcsMaxDoubleExponent) {
        return True;
    }
    string digits = parts[0];
    digits =~ s/\.//;
    digits =~ s/^-//;
    # compare significands digit by digit, padding the shorter one with trailing zeros
    string bound = A2aJcsMaxDoubleDigits;
    int width = max(digits.size(), bound.size());
    digits += strmul("0", width - digits.size());
    bound += strmul("0", width - bound.size());
    return digits > bound;
}

#! Serializes a number using the ECMAScript \c Number::toString algorithm
/** @param v the number to serialize

    @return the canonical representation

    @throw JCS-ERROR if the value is not finite; RFC 8785 has no representation for \c NaN or an infinity

    @since A2aClient 1.0
*/
public string sub a2aJcsNumber(auto v) {
    float f = float(v);
    if (f != f || f == @inf@ || f == -@inf@) {
        throw "JCS-ERROR", sprintf("%y is not finite; RFC 8785 has no representation for it", v);
    }
    # ECMAScript prints both zeros as "0"
    if (f == 0.0) {
        return "0";
    }
    string sign = f < 0.0 ? "-" : "";
    if (f < 0.0) {
        f = -f;
    }

    # find the shortest significand that round trips, as ECMAScript specifies
    string repr;
    for (int prec = 1; prec <= A2aJcsMaxSignificantDigits; ++prec) {
        # the precision is dynamic, so the format string has to be built first
        repr = sprintf(sprintf("%%.%de", prec - 1), f);
        if (float(repr) == f && !a2aJcsOverflowsDouble(repr)) {
            break;
        }
    }

    # split the "d.dddde[+-]xx" form into its digits and decimal exponent
    list<string> parts = repr.split("e");
    int exp10 = parts[1].toInt();
    string digits = parts[0];
    digits =~ s/\.//;
    # strip trailing zeros; they carry no information and ECMAScript never emits them
    digits =~ s/0+$//;
    if (!digits.val()) {
        digits = "0";
    }
    int k = digits.size();
    # n is the position of the decimal point relative to the start of the digits
    int n = exp10 + 1;

    if (k <= n && n <= A2aJcsMaxPositionalExponent) {
        return sign + digits + strmul("0", n - k);
    }
    if (0 < n && n <= A2aJcsMaxPositionalExponent) {
        return sign + digits.substr(0, n) + "." + digits.substr(n);
    }
    if (-6 < n && n <= 0) {
        return sign + "0." + strmul("0", -n) + digits;
    }
    # exponential notation
    string mantissa = k == 1 ? digits : digits.substr(0, 1) + "." + digits.substr(1);
    int e = n - 1;
    return sprintf("%s%se%s%d", sign, mantissa, e < 0 ? "-" : "+", abs(e));
}
}
