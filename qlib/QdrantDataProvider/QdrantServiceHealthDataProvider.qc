# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantServiceHealthDataProvider class definition

/** QdrantServiceHealthDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The Qdrant service health check API data provider
/** This API checks the health status of the Qdrant server by returning server info
    (title, version, commit) from the root endpoint

    @note Corresponds to: GET /
*/
public class QdrantServiceHealthDataProvider inherits QdrantDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "health",
            "desc": "Qdrant service health check data provider (`GET /`)",
            "type": "QdrantServiceHealthDataProvider",
            "constructor_options": QdrantDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = AbstractDataProviderTypeMap."nothing";

        #! Response type
        const ResponseType = new QdrantServiceHealthResponseDataType();
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", QdrantDataProvider::ConstructorOptions, options);
        rest = QdrantDataProvider::getRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(RestClient::RestClient rest) : QdrantDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Qdrant service health check provider for `%s`", rest.getSafeURL());
    }

    #! Makes a request and returns the response
    /** @param req the request (ignored)
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Use root endpoint which returns server info (title, version, commit)
        # The /healthz endpoint returns plain text
        return rest.get("/").body;
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Qdrant service health response data type
public class QdrantServiceHealthResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The health check title",
                "desc": "The title of the health check response",
                "example_value": "qdrant - vector search engine",
            },
            "version": {
                "type": StringOrNothingType,
                "display_name": "Version",
                "short_desc": "The Qdrant server version",
                "desc": "The version of the Qdrant server",
                "example_value": "1.13.5",
            },
            "commit": {
                "type": StringOrNothingType,
                "display_name": "Commit",
                "short_desc": "The Qdrant server commit hash",
                "desc": "The git commit hash of the Qdrant server build",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
