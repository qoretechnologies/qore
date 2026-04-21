# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DataProviderErrorUtil.qc defines defensive helpers for REST API error enrichment

/*  DataProviderErrorUtil.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Main DataProvider namespace
public namespace DataProvider {
#! Defensive utilities for extracting human-readable reasons from REST API error response bodies
/** Data providers whose upstream APIs wrap errors in a JSON envelope typically want to surface
    the API's own reason text in the exception \c desc so callers can see *why* a 4xx or 5xx was
    returned instead of only the HTTP status line.

    Each API's envelope is different (top-level \c error scalar, nested \c meta.message, OpenAPI
    \c description + \c errors[], etc.), but the defensive plumbing is the same: guard against a
    missing body, non-hash body, or missing/wrong-typed keys. These helpers centralise that
    plumbing so providers can compose their error formatting without re-implementing the
    guards in every catch block.

    All methods in this class are tolerant of any input: they never throw on malformed or
    unexpected data and return \c NOTHING instead. Providers can therefore call them
    unconditionally and safely fall back to \c ex.desc when nothing was extracted.
*/
public class DataProviderErrorUtil {
    #! Returns the first non-empty string value found by walking dotted paths in the given value
    /** @param data the root value to probe (typically \c ex.arg.body); may be of any type
        @param paths dotted key paths to try in order, e.g. \c ("meta.message", "error", "description")

        @return the first resolved value that is a non-empty string; \c NOTHING if no path
        resolves to a non-empty string, or if \a data is not a hash
    */
    static *string firstStringAt(auto data, softlist<string> paths) {
        if (data.typeCode() != NT_HASH) {
            return NOTHING;
        }
        foreach string path in (paths) {
            auto v = data;
            foreach string key in (path.split(".")) {
                if (v.typeCode() != NT_HASH) {
                    v = NOTHING;
                    break;
                }
                v = v{key};
            }
            if (v.typeCode() == NT_STRING && v.val()) {
                return v;
            }
        }
        return NOTHING;
    }

    #! Returns the first value of any type found by walking dotted paths in the given value
    /** Like @ref firstStringAt() but does not require a string result. Useful for probing a
        numeric \c code field alongside a string \c message field.

        @param data the root value to probe; may be of any type
        @param paths dotted key paths to try in order

        @return the first resolved value that \c exists (not \c NOTHING); \c NOTHING if no path
        resolves, or if \a data is not a hash
    */
    static auto firstValueAt(auto data, softlist<string> paths) {
        if (data.typeCode() != NT_HASH) {
            return NOTHING;
        }
        foreach string path in (paths) {
            auto v = data;
            bool resolved = True;
            foreach string key in (path.split(".")) {
                if (v.typeCode() != NT_HASH) {
                    resolved = False;
                    break;
                }
                v = v{key};
            }
            if (resolved && exists v) {
                return v;
            }
        }
        return NOTHING;
    }

    #! Formats a list of field-level error entries as a bracketed \c "[part1; part2; ...]" string
    /** Each hash entry becomes \c "<field> <code>: <message>", drawing from the first present key
        in each of the three hint lists. Non-hash entries are stringified via \c %y. If no
        non-empty part can be produced, returns \c NOTHING.

        @param errors the value to format (typically \c body.errors); \c NOTHING is returned
        unless this is a non-empty list
        @param field_keys candidate keys to probe for the field name (default \c ("field",))
        @param code_keys candidate keys to probe for the error code (default \c ("error_code", "code"))
        @param message_keys candidate keys to probe for the human message (default \c ("message", "msg"))

        @return the formatted bracketed string, or \c NOTHING if no parts could be produced
    */
    static *string formatFieldErrors(auto errors,
                                     softlist<string> field_keys = ("field",),
                                     softlist<string> code_keys = ("error_code", "code"),
                                     softlist<string> message_keys = ("message", "msg")) {
        if (errors.typeCode() != NT_LIST || !errors) {
            return NOTHING;
        }
        list<string> parts = ();
        foreach auto e in (errors) {
            if (e.typeCode() != NT_HASH) {
                push parts, sprintf("%y", e);
                continue;
            }
            string part = DataProviderErrorUtil::firstStringAt(e, field_keys) ?? "";
            *string code = DataProviderErrorUtil::firstStringAt(e, code_keys);
            *string msg = DataProviderErrorUtil::firstStringAt(e, message_keys);
            if (code.val()) {
                part = part.val() ? part + " " + code : code;
            }
            if (msg.val()) {
                part = part.val() ? part + ": " + msg : msg;
            }
            if (part.val()) {
                push parts, part;
            }
        }
        return parts ? "[" + join("; ", parts) + "]" : NOTHING;
    }
}
}  # public namespace DataProvider
