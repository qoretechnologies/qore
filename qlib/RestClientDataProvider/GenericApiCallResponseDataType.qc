# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestClientDataProvider module definition

/** GenericApiCallResponseDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the RestClientDataProvider module
public namespace RestClientDataProvider {
#! Constant type for the framework-injected generic "Make an API call" response
public const GenericApiCallResponseDataType = new GenericApiCallResponseDataType();

#! Response type used by @ref GenericApiCallProvider — the framework-injected generic call provider
/** Lean by design: only the three fields any caller actually needs (`status`, `headers`, `body`),
    plus an optional `debug` sub-hash that's populated when the caller sets `include_debug` on the
    request.

    Older REST-call providers in this codebase use
    @ref RestClientCallResponseDataType which echoes the full request back (`request-headers`,
    `request-uri`, …), duplicates `response-headers` as `response-headers-raw`, and includes
    several @ref NOTHING placeholder fields. That response shape is heavy, leaks bearer tokens via
    the un-redacted request-headers echo, and forces every consumer to dig past the duplication.
    This type keeps only what's useful.

    @since RestClientDataProvider 2.4
*/
public class GenericApiCallResponseDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "status": {
                "display_name": "HTTP Status Code",
                "type": IntType,
                "short_desc": "The HTTP response status code",
                "desc": "The HTTP response status code (e.g. 200, 404, 500). When the request raises "
                    "an error and `error_passthru` is not set, the action throws and this hash is not "
                    "returned at all; with `error_passthru = True` the status reflects the actual "
                    "non-2xx code.",
            },
            "headers": {
                "display_name": "Response Headers",
                "type": AutoHashOrNothingType,
                "short_desc": "Response headers, lowercased keys",
                "desc": "All HTTP response headers with lowercased keys for case-insensitive lookup. "
                    "Set-Cookie values from the server are included verbatim.",
            },
            "body": {
                "display_name": "Response Body",
                "type": AutoType,
                "short_desc": "Deserialized response body",
                "desc": "The response body, deserialized when the server returned a recognized "
                    "Content-Type (JSON, XML, YAML). Raw bytes/strings are returned as-is for "
                    "unknown content types.",
            },
            "debug": {
                "display_name": "Debug Info",
                "type": AutoHashOrNothingType,
                "short_desc": "Optional request/timing debug info — set include_debug=True to populate",
                "desc": "Populated only when the request was made with `include_debug = True`. "
                    "Contains the request method/URL/headers (with sensitive headers like "
                    "Authorization redacted), request and response body sizes, and the round-trip "
                    "duration in milliseconds.",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
