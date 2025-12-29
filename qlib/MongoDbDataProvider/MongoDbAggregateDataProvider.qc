# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MongoDbAggregateDataProvider class definition

/** MongoDbAggregateDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

%require-types
%enable-all-warnings
%new-style
%strict-args

public namespace MongoDbDataProvider {
#! MongoDB aggregate data provider - runs aggregation pipelines on a collection
public class MongoDbAggregateDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "MongoDbAggregateDataProvider",
            "supports_request": True,
            "supports_children": False,
            "constructor_options": NOTHING,
        };

        #! Request type
        const RequestType = new MongoDbAggregateRequestDataType();

        #! Response type
        const ResponseType = new MongoDbAggregateResponseDataType();

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! The MongoDB client
        MongoClient client;

        #! The database
        MongoDatabase db;

        #! The collection
        MongoCollection coll;
    }

    #! Creates the object
    constructor(MongoClient client, MongoDatabase db, MongoCollection coll) {
        self.client = client;
        self.db = db;
        self.coll = coll;
    }

    #! Returns the data provider name
    string getName() {
        return "aggregate";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "MongoDB aggregate data provider - runs aggregation pipelines on a collection";
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Makes a request and returns the response
    /** @param req the request hash
        @param request_options the request options

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (!req.pipeline) {
            throw "MONGODB-AGGREGATE-ERROR", "pipeline is required";
        }

        MongoCursor cursor = coll.aggregate(req.pipeline);
        list<hash<auto>> docs = cursor.toList();

        return {
            "results": docs,
            "count": docs.size(),
        };
    }
}

#! Aggregate request data type
public class MongoDbAggregateRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "pipeline": {
                "display_name": "Pipeline",
                "short_desc": "Aggregation pipeline stages",
                "type": SoftListType,
                "desc": "A list of aggregation pipeline stages (e.g., [{\"$match\": {...}}, {\"$group\": {...}}])",
                "required": True,
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("MongoDbAggregateRequest", Fields) {
    }
}

#! Aggregate response data type
public class MongoDbAggregateResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "results": {
                "display_name": "Results",
                "short_desc": "Aggregation results",
                "type": SoftListType,
                "desc": "List of documents resulting from the aggregation pipeline",
            },
            "count": {
                "display_name": "Count",
                "short_desc": "Number of results",
                "type": IntType,
                "desc": "The number of documents in the result",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("MongoDbAggregateResponse", Fields) {
    }
}
}
