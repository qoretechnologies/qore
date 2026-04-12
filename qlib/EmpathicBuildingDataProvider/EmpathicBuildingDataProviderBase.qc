# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EmpathicBuildingDataProvider module definition

/** EmpathicBuildingDataProviderBase.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the EmpathicBuildingDataProvider module
public namespace EmpathicBuildingDataProvider {
#! The EmpathicBuilding data provider class
public class EmpathicBuildingDataProviderBase inherits DataProvider::AbstractDataProvider {
    private {
        #! The REST client — any @ref RestClient::AbstractRestClient "AbstractRestClient" subclass
        /** May be a sync @ref RestClient::RestClient "RestClient" (HTTP/1.x) or an async
            @ref RestClientIo::RestClientIo "RestClientIo" (HTTP/1.1/2/3 multiplexed).
        */
        RestClient::AbstractRestClient rest;
    }

    #! Creates the object
    constructor() {
    }

    #! Creates the object
    constructor(RestClient::AbstractRestClient rest, *Logger::LoggerInterface logger) {
        self.rest = rest;
        if (logger) {
            setLogger(logger);
        }
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
        LoggerWrapper::setLogger(logger);
    }

    #! Update any connection used by the data provider
    /** @param conn the new AbstractConnection object
        @param connobj the new connection object itself (ex: an AbstractRestClient for a "rest" connection)
    */
    updateConnectionObjects(object conn, object connobj) {
        if (!(connobj instanceof RestClient::AbstractRestClient)) {
            throw "UPDATE-CONNECTION-ERROR", sprintf("Data provider %y requires an \"AbstractRestClient\" "
                "object; got %y instead", self.className(), connobj.className());
        }
        rest = cast<RestClient::AbstractRestClient>(connobj);
        rest.setLogger(logger);
    }

    #! Converts a Haltian ms epoch offset to a date/time value
    static date getTimestamp(int epoch_ms) {
        int epoch = epoch_ms / 1000;
        int ms = epoch_ms- epoch * 1000;
        return localtime(epoch) + milliseconds(ms);
    }

    #! Makes a REST call and returns the response
    /** Connection-level retries (socket errors, stale pooled connections, protocol fallback)
        are handled by @ref HttpClientIo::HttpClientConnectionManager "HttpClientConnectionManager"
        with up to 3 automatic retries — no application-level retry loop is needed.
    */
    private hash<RestClient::RestResponseInfo> doRestCommand(string method, string path, auto body) {
        debug("REST %y %y (%y) body: %y", method, rest.getUrl(), path, body);
        return rest.restDoRequest(method, path, body);
    }

    #! Map a @ref RestClient::RestResponseInfo error arg to the legacy ex arg shape
    /** The async @ref RestClientIo::RestClientIo "RestClientIo" transport throws
        @ref RestClient::REST-RESPONSE-ERROR with a @ref RestClient::RestResponseInfo hashdecl
        in \c ex.arg; translate to the legacy \c "request-uri" / \c "response-code" /
        \c "response-body" shape so callers do not need to change.
    */
    private static hash<auto> mapRestErrorInfo(hash<ExceptionInfo> ex) {
        if (ex.arg.typeCode() != NT_HASH) {
            return {};
        }
        return {
            "request-uri": ex.arg.request_path,
            "response-code": ex.arg.status_code,
            "response-body": ex.arg.body,
        };
    }
}
}