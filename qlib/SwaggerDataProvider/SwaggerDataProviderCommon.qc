# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SwaggerDataProvider module definition

/** SwaggerDataProviderCommon.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SwaggerDataProvider module
public namespace SwaggerDataProvider {
#! The Swagger data provider common base class
public class SwaggerDataProviderCommon inherits DataProvider::AbstractDataProvider {
    private {
        #! The Swagger schema
        SwaggerSchema schema;

        #! The REST client object for API calls
        *RestClient::AbstractRestClient rest;
    }

    #! Creates the object
    constructor() {
    }

    #! Creates the object
    constructor(RestClient::AbstractRestClient rest) {
        self.rest = rest;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        if (rest) {
            rest.setLogger(logger);
        }
        LoggerWrapper::setLogger(logger);
    }

    #! Returns the Swagger schema
    SwaggerSchema getSchema() {
        return schema;
    }

    #! Returns the REST client
    *RestClient::AbstractRestClient tryGetRestClient() {
        return rest;
    }

    #! Sets the REST client
    setRestClient(RestClient::AbstractRestClient rest) {
        self.rest = rest;
    }
}
}