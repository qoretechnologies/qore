# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElasticSearchDataProvider class definition

/** ElasticSearchDataProviderBase.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the ElasticSearchDataProvider module
public namespace ElasticSearchDataProvider {
#! The ElasticSearch data provider base class
public class ElasticSearchDataProviderBase inherits DataProvider::AbstractDataProvider {
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

    #! Creates the object from a REST connection
    constructor(RestClient::AbstractRestClient rest) {
        self.rest = rest;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
        LoggerWrapper::setLogger(logger);
    }
}
}

