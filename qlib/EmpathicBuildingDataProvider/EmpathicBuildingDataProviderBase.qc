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
        #! Default headers of a synchronous REST client that are not copied by @ref getAsyncClient()
        const IgnoredHeaders = {
            "accept": True,
            "user-agent": True,
            "content-type": True,
        };
    }

    private {
        #! The REST client for API calls
        RestClientIo::RestClientIo rest;
    }

    #! Creates the object
    constructor() {
    }

    #! Creates the object
    /** @param rest the REST client; a synchronous @ref RestClient::RestClient "RestClient" is converted with
        @ref getAsyncClient()
        @param logger an optional logger
    */
    constructor(RestClient::AbstractRestClient rest, *Logger::LoggerInterface logger) {
        self.rest = EmpathicBuildingDataProviderBase::getAsyncClient(rest);
        if (logger) {
            setLogger(logger);
        }
    }

    #! Returns the REST client for API calls
    /** @return the REST client for API calls

        @since EmpathicBuildingDataProvider 1.2
    */
    RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Returns an asynchronous REST client for the given REST client
    /** @param rest the REST client

        @return \a rest if it is a @ref RestClientIo::RestClientIo "RestClientIo" object, otherwise an asynchronous
        client created with the constructor options, connection path, and default headers of \a rest: an
        @ref EmpathicBuildingRestClientIo for an @ref EmpathicBuildingRestClient, otherwise a
        @ref RestClientIo::RestClientIo "RestClientIo"

        @throw EMPATHICBUILDING-CLIENT-ERROR \a rest is neither a @ref RestClientIo::RestClientIo "RestClientIo" nor a
        @ref RestClient::RestClient "RestClient" object

        @since EmpathicBuildingDataProvider 1.2
    */
    static RestClientIo::RestClientIo getAsyncClient(RestClient::AbstractRestClient rest) {
        if (rest instanceof RestClientIo::RestClientIo) {
            return cast<RestClientIo::RestClientIo>(rest);
        }
        if (!(rest instanceof RestClient::RestClient)) {
            throw "EMPATHICBUILDING-CLIENT-ERROR", sprintf("cannot use a REST client of class %y; expecting a "
                "\"RestClientIo\" or \"RestClient\" object", rest.className());
        }
        RestClient::RestClient sync_rest = cast<RestClient::RestClient>(rest);
        # the options can have values of different types, so the hash must not take a narrower type
        hash<auto> opts = {};
        opts += sync_rest.getConstructorOptions();
        # the synchronous client has already logged in if possible; its token is used
        *string token = sync_rest.getToken();
        if (token.val()) {
            opts.token = token;
            opts.token_type = sync_rest.getTokenType();
            opts.skip_login = True;
        }
        # the current default headers, including any added after the client was created, for example a gateway
        # token; they are given as an option so that copies of the client made with copyWithUrl() keep them; the
        # async client sends header names in lower case
        hash<auto> headers = {};
        foreach hash<auto> i in (sync_rest.getDefaultHeaders().pairIterator()) {
            string key = i.key.lwr();
            if (!IgnoredHeaders{key}) {
                headers{key} = i.value;
            }
        }
        if (headers) {
            opts.headers = headers;
        }
        RestClientIo::RestClientIo rv = rest instanceof EmpathicBuildingRestClient
            ? new EmpathicBuildingRestClientIo(opts)
            : new RestClientIo::RestClientIo(opts);
        rv.setConnectionPath(sync_rest.getConnectionPath());
        return rv;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
        LoggerWrapper::setLogger(logger);
    }

    #! Update any connection used by the data provider
    /** @param conn the new AbstractConnection object
        @param connobj the new connection object itself (ex: an AbstractRestClient for a "rest" connection); a
        synchronous @ref RestClient::RestClient "RestClient" is converted with @ref getAsyncClient()
    */
    updateConnectionObjects(object conn, object connobj) {
        if (!(connobj instanceof RestClient::AbstractRestClient)) {
            throw "UPDATE-CONNECTION-ERROR", sprintf("Data provider %y requires an \"AbstractRestClient\" "
                "object; got %y instead", self.className(), connobj.className());
        }
        rest = EmpathicBuildingDataProviderBase::getAsyncClient(cast<RestClient::AbstractRestClient>(connobj));
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