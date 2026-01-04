# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestClientDataProvider class definition

/** RestClientDataProviderBase.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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
#! The REST client data provider class, provides API classes as children
public class RestClientDataProviderBase inherits DataProvider::AbstractDataProvider {
    private {
        #! The REST client object
        RestClient::RestClient rest;
    }

    #! Creates the object
    constructor() {
    }

    #! Creates the object
    constructor(RestClient::RestClient rest) {
        self.rest = rest;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
        LoggerWrapper::setLogger(logger);
    }

    #! Makes a REST request with debug info collection
    /** @param meth the HTTP method (GET, POST, PUT, DELETE, etc.)
        @param body the request body (can be NOTHING for GET, DELETE, etc.)
        @param path the request path
        @param hdr optional request headers

        @return the response hash

        @since RestClientDataProvider 2.2
    */
    private hash<auto> makeRequestWithDebug(string meth, auto body, string path, *hash<auto> hdr) {
        date start_time = now_us();
        hash<auto> info;
        *string error_msg;
        hash<auto> resp;

        try {
            resp = rest.doRequest(meth, path, body, \info, NOTHING, hdr);
        } catch (hash<ExceptionInfo> ex) {
            error_msg = sprintf("%s: %s", ex.err, ex.desc);
            # Collect debug info even on error if collector is registered
            if (debugCollectionEnabled()) {
                collectRestDebugInfo(meth, path, body, hdr, info, NOTHING, start_time, error_msg);
            }
            rethrow;
        }

        # Collect debug info if collector is registered
        if (debugCollectionEnabled()) {
            collectRestDebugInfo(meth, path, body, hdr, info, resp, start_time);
        }

        return {
            "request-headers": info.headers,
            "request-serialization": info."request-serialization",
            "request-uri": info."request-uri",
            "response-body": resp.body,
            "response-headers": (map {$1.key.lwr(): $1.value}, info."response-headers-raw".pairIterator()),
            "response-headers-raw": info."response-headers-raw",
            "response-serialization": info."response-serialization",
            "response-uri": info."response-uri",
            "status-code": info."response-code",
            "status-message": info."response-headers".status_message,
        };
    }

    #! Collects REST request/response debug info
    private collectRestDebugInfo(string meth, string path, auto body, *hash<auto> req_hdr,
            hash<auto> info, *hash<auto> resp, date start_time, *string error_msg) {
        date end_time = now_us();
        int duration_ms = (end_time - start_time).durationMilliseconds();

        # Build full URL
        string url = rest.getURL();
        if (path) {
            if (!url.endsWith("/") && !path.startsWith("/")) {
                url += "/";
            }
            url += path;
        }

        # Determine protocol
        string protocol = "HTTP/1.1";
        if (info."response-headers"."x-http2-status") {
            protocol = "HTTP/2";
        }

        hash<DataProviderDebugInfo> debug_info = <DataProviderDebugInfo>{
            "operation": DPDI_OP_RESPONSE,
            "start_time": start_time,
            "end_time": end_time,
            "duration_ms": duration_ms,
            "url": url,
            "method": meth,
            "protocol": protocol,
            "status_code": info."response-code",
            "status_message": info."response-headers".status_message,
            "request_headers": info.headers ?? req_hdr,
            "response_headers": info."response-headers-raw",
            "request_body": body,
            "response_body": resp.body,
            "request_content_type": info.headers."Content-Type" ?? req_hdr."Content-Type",
            "response_content_type": info."response-headers"."content-type",
            "request_size": body.typeCode() == NT_BINARY ? body.size() :
                (body.typeCode() == NT_STRING ? body.length() : NOTHING),
            "response_size": resp.body.typeCode() == NT_BINARY ? resp.body.size() :
                (resp.body.typeCode() == NT_STRING ? resp.body.length() : NOTHING),
            "http2": protocol == "HTTP/2",
            "tls": url =~ /^https:/i,
            "error": error_msg,
        };

        collectDebugInfo(debug_info);
    }

    #! Returns the request path for the given request
    static string getRequestPath(hash<auto> req) {
        if (req.query_args) {
            if (req.path =~ /\?/) {
                req.path += "&";
            } else {
                req.path += "?";
            }
            req.path += foldl $1 + "&" + $2,
                (map sprintf("%s=%s", $1.key, RestClientDataProviderBase::getUriValue($1.value)),
                    req.query_args.pairIterator());
        }
        return req.path;
    }

    #! Returns the value for the given query argument
    static string getUriValue(auto v) {
        switch (v.typeCode()) {
            case NT_BOOLEAN:
                return v ? "true" : "false";
            case NT_LIST:
                return foldl $1 + "," + $2, (map RestClientDataProviderBase::getUriValue($1), v);
            case NT_STRING:
                return v;
            default:
                return v.toString();
        }
    }
}
}