# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantDataProviderBase class definition

/** QdrantDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the QdrantDataProvider module
public namespace QdrantDataProvider {
#! The Qdrant data provider base class
public class QdrantDataProviderBase inherits DataProvider::AbstractDataProvider {
    private {
        #! The REST client for Qdrant API access
        RestClientIo::RestClientIo rest;
    }

    #! Creates the object
    constructor() {
    }

    #! Creates the object from a REST connection
    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
        LoggerWrapper::setLogger(logger);
    }

    #! Returns the REST client
    RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Makes a GET request to the Qdrant API and returns the result
    private auto doQdrantGet(string path) {
        return rest.restGet(path).body.result;
    }

    #! Makes a POST request to the Qdrant API and returns the result
    private auto doQdrantPost(string path, *hash<auto> body) {
        return rest.restPost(path, body).body.result;
    }

    #! Makes a PUT request to the Qdrant API and returns the result
    private auto doQdrantPut(string path, *hash<auto> body) {
        return rest.restPut(path, body).body.result;
    }

    #! Makes a PATCH request to the Qdrant API and returns the result
    private auto doQdrantPatch(string path, *hash<auto> body) {
        return rest.restPatch(path, body).body.result;
    }

    #! Makes a DELETE request to the Qdrant API and returns the result
    private auto doQdrantDelete(string path, *hash<auto> body) {
        return rest.restDel(path, body).body.result;
    }

    #! Returns a collection path with an optional suffix
    private string collectionPath(string name, *string suffix) {
        return sprintf("/collections/%s%s", encode_url(name, True), suffix ?? "");
    }

    #! Formats a query parameter value for use in a URL
    /** Booleans are converted to "true"/"false" strings as required by the Qdrant API
    */
    static string formatQueryValue(auto val) {
        if (val.typeCode() == NT_BOOLEAN) {
            return val ? "true" : "false";
        }
        return string(val);
    }
}
}
