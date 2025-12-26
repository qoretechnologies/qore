# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestSchemaDataProvider module definition

/** RestSchemaDataProviderCommon.qc Copyright 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the RestSchemaDataProvider module
public namespace RestSchemaDataProvider {
#! The REST schema data provider common base class
public class RestSchemaDataProviderCommon inherits DataProvider::AbstractDataProvider {
    private {
        #! The REST schema (Swagger or OpenAPI 3.0)
        AbstractRestSchemaValidator schema;

        #! The REST client object for API calls
        *RestClient::RestClient rest;
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
        if (rest) {
            rest.setLogger(logger);
        }
        LoggerWrapper::setLogger(logger);
    }

    #! Returns the REST schema
    AbstractRestSchemaValidator getSchema() {
        return schema;
    }

    #! Returns the RestClient
    *RestClient::RestClient tryGetRestClient() {
        return rest;
    }

    #! Sets the RestClient
    setRestClient(RestClient::RestClient rest) {
        self.rest = rest;
    }

    #! Returns True if the schema is a Swagger schema
    bool isSwagger() {
        return schema instanceof SwaggerSchema;
    }

    #! Returns True if the schema is an OpenAPI 3.0 schema
    bool isOpenApi3() {
        return schema instanceof OpenApi3Schema;
    }
}
}
