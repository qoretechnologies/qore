# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantPointsScrollDataProvider class definition

/** QdrantPointsScrollDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The Qdrant points scroll API data provider
/** This API scrolls/iterates over points in a Qdrant collection

    @note Corresponds to: POST /collections/{name}/points/scroll
*/
public class QdrantPointsScrollDataProvider inherits QdrantDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "scroll",
            "desc": "Qdrant points scroll data provider (`POST /collections/{name}/points/scroll`)",
            "type": "QdrantPointsScrollDataProvider",
            "constructor_options": QdrantDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new QdrantPointsScrollRequestDataType();

        #! Response type
        const ResponseType = new QdrantPointsScrollResponseDataType();
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
        return sprintf("Qdrant points %s provider for `%s`", ProviderInfo.name, rest.getSafeURL());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string collection = remove req.collection;
        return doQdrantPost(collectionPath(collection, "/points/scroll"), req);
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

#! Qdrant points scroll request data type
public class QdrantPointsScrollRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "collection": {
                "type": StringType,
                "display_name": "Collection",
                "short_desc": "Collection name",
                "desc": "The name of the collection to scroll points from",
                "example_value": "my_collection",
            },
            "filter": {
                "type": AutoHashOrNothingType,
                "display_name": "Filter",
                "short_desc": "Filter conditions",
                "desc": "Filter conditions to select which points to scroll; allows iterating over a subset of "
                    "points matching specific criteria",
                "example_value": {"must": ({"key": "city", "match": {"value": "London"}},)},
            },
            "limit": {
                "type": IntOrNothingType,
                "display_name": "Limit",
                "short_desc": "Maximum number of points to return",
                "desc": "The maximum number of points to return per scroll request",
                "example_value": 10,
            },
            "offset": {
                "type": AutoType,
                "display_name": "Offset",
                "short_desc": "Point ID to start from",
                "desc": "The point ID to start scrolling from; use the `next_page_offset` from the previous "
                    "response to paginate; can be an integer or a UUID string",
                "example_value": 0,
            },
            "with_payload": {
                "type": SoftBoolOrNothingType,
                "display_name": "With Payload",
                "short_desc": "Payload selector",
                "desc": "Select which payload fields to return; `true` for all, `false` for none",
                "example_value": True,
            },
            "with_vector": {
                "type": SoftBoolOrNothingType,
                "display_name": "With Vector",
                "short_desc": "Vector selector",
                "desc": "Select which vectors to return; `true` for all, `false` for none",
                "example_value": False,
            },
            "order_by": {
                "type": AutoHashOrNothingType,
                "display_name": "Order By",
                "short_desc": "Order by payload key",
                "desc": "Order the scroll results by a payload key; can be a string key name or an object with "
                    "key and direction",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Qdrant points scroll response data type
public class QdrantPointsScrollResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "points": {
                "type": AutoListType,
                "display_name": "Points",
                "short_desc": "List of scrolled point results",
                "desc": "A list of point result objects, each containing `id`, `payload`, and optionally `vector`",
                "example_value": ({"id": 1, "payload": {"text": "hello"}},),
            },
            "next_page_offset": {
                "type": AutoType,
                "display_name": "Next Page Offset",
                "short_desc": "Offset for the next page",
                "desc": "The offset to use for the next scroll request; can be an integer or a UUID string; "
                    "if not present, all points have been returned",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
