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
        RestClientIo::RestClientIo rest;
    }

    #! Creates the object
    constructor() {
    }

    #! Creates the object
    constructor(RestClientIo::RestClientIo rest) {
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
        *string error_msg;
        hash<RestClient::RestResponseInfo> resp;

        try {
            resp = rest.restDoRequest(meth, path, body, hdr);
        } catch (hash<ExceptionInfo> ex) {
            error_msg = sprintf("%s: %s", ex.err, ex.desc);
            # Collect debug info even on error if collector is registered
            if (debugCollectionEnabled()) {
                # On REST-RESPONSE-ERROR the arg1 is a RestResponseInfo hash; reuse it
                *hash<RestClient::RestResponseInfo> err_resp;
                if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.typeCode() == NT_HASH) {
                    err_resp = cast<hash<RestClient::RestResponseInfo>>(ex.arg);
                }
                collectRestDebugInfo(meth, path, body, hdr, err_resp, start_time, error_msg);
            }
            rethrow;
        }

        # Collect debug info if collector is registered
        if (debugCollectionEnabled()) {
            collectRestDebugInfo(meth, path, body, hdr, resp, start_time);
        }

        return RestClientDataProviderBase::buildResponseHash(resp);
    }

    #! Converts a @ref RestClient::RestResponseInfo hash to the REST data provider response shape
    static hash<auto> buildResponseHash(hash<RestClient::RestResponseInfo> resp) {
        return {
            "request-headers": resp.request_headers,
            "request-serialization": NOTHING,
            "request-uri": resp.request_path,
            "response-body": resp.body,
            "response-headers": resp.headers
                ? (map {$1.key.lwr(): $1.value}, resp.headers.pairIterator())
                : NOTHING,
            "response-headers-raw": resp.headers,
            "response-serialization": NOTHING,
            "response-uri": resp.request_path,
            "status-code": resp.status_code,
            "status-message": resp.headers.status_message,
        };
    }

    #! Collects REST request/response debug info
    private collectRestDebugInfo(string meth, string path, auto body, *hash<auto> req_hdr,
            *hash<RestClient::RestResponseInfo> resp, date start_time, *string error_msg) {
        date end_time = now_us();
        int duration_ms = (end_time - start_time).durationMilliseconds();

        # Build full URL
        string url = rest.getUrl();
        if (path) {
            if (!url.endsWith("/") && !path.startsWith("/")) {
                url += "/";
            }
            url += path;
        }

        # Determine protocol from RestResponseInfo (h1/h2/h3 → HTTP/1.1/HTTP/2/HTTP/3)
        string protocol = "HTTP/1.1";
        switch (resp.protocol) {
            case "h2":
                protocol = "HTTP/2";
                break;
            case "h3":
                protocol = "HTTP/3";
                break;
        }

        *hash<auto> req_headers = resp.request_headers ?? req_hdr;
        hash<DataProviderDebugInfo> debug_info = <DataProviderDebugInfo>{
            "operation": DPDI_OP_RESPONSE,
            "start_time": start_time,
            "end_time": end_time,
            "duration_ms": duration_ms,
            "url": url,
            "method": meth,
            "protocol": protocol,
            "status_code": resp.status_code,
            "status_message": resp.headers.status_message,
            "request_headers": req_headers,
            "response_headers": resp.headers,
            "request_body": body,
            "response_body": resp.body,
            "request_content_type": req_headers."Content-Type" ?? req_headers."content-type",
            "response_content_type": resp.content_type,
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