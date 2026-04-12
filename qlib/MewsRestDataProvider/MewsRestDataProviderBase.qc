# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MewsRestDataProvider class definition

/** MewsRestDataProviderBase.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated doContent-Typecumentation files (the "Software"),
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

#! contains all public definitions in the MewsRestDataProvider module
public namespace MewsRestDataProvider {
#! The MewsRest data provider base class
public class MewsRestDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Max records that the Mews API will return in one request
        const MaxRecords = 1000;
    }

    private {
        #! The REST client — must be a @ref MewsRestClient or @ref MewsRestClientIo
        RestClient::AbstractRestClient rest;
        SwaggerDataProvider::SwaggerDataProvider sdp;
    }

    #! Creates the object
    constructor() {
    }

    #! Creates the object from a REST connection
    /** @param rest the Mews-flavored REST client (either @ref MewsRestClient::MewsRestClient
            or @ref MewsRestClient::MewsRestClientIo — both carry the Mews auth token
            state via the shared @ref MewsRestClient::MewsRestClientBase mixin)
        @param sdp an optional pre-built SwaggerDataProvider (defaults to one constructed
            from the Mews Swagger schema)

        @throw MEWS-CLIENT-ERROR if \a rest is not a Mews-flavored client or has no
            Swagger schema validator installed
    */
    constructor(RestClient::AbstractRestClient rest, *SwaggerDataProvider::SwaggerDataProvider sdp) {
        if (!(rest instanceof MewsRestClient::MewsRestClientBase)) {
            throw "MEWS-CLIENT-ERROR", sprintf("expected a MewsRestClient or MewsRestClientIo "
                "instance; got %y instead", rest.className());
        }
        self.rest = rest;
        if (sdp) {
            self.sdp = sdp;
        } else {
            AbstractRestSchemaValidator v = rest.getValidator();
            if (!(v instanceof SwaggerSchema)) {
                throw "MEWS-CLIENT-ERROR", sprintf("The Mews REST client object has no Swagger schema; "
                    "validator used: %y", v.className());
            }
            self.sdp = new SwaggerDataProvider(cast<SwaggerSchema>(v), rest);
            if (logger) {
                self.sdp.setLogger(logger);
            }
        }
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
        sdp.setLogger(logger);
        LoggerWrapper::setLogger(logger);
    }

    static AbstractRestClient getRestConnection(*hash<auto> options) {
        AbstractRestClient rest;
        if (options.restclient) {
            rest = options.restclient;
            if (options.url) {
                # Non-mutating URL override: build a sibling client via copyWithUrl()
                # — works for both MewsRestClient and MewsRestClientIo
                rest = rest.copyWithUrl(options.url);
            }
        } else {
            if (!options.url && !options.restclient_options) {
                throw "CONSTRUCTOR-ERROR", "no 'restclient', 'url', or 'restclient_options' option passed; "
                    "cannot create REST client to MewsRest instance without a URL";
            }

            hash<auto> opts;
            opts += options.restclient_options;
            if (options.url) {
                opts.url = options.url;
            }
            # Default to the async multiplexed MewsRestClientIo transport
            rest = new MewsRestClientIo(opts);
        }
        return rest;
    }
}
}

