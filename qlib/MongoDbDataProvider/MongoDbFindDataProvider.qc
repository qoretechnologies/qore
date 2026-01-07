# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MongoDbFindDataProvider class definition

/** MongoDbFindDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

%modern

public namespace MongoDbDataProvider {
#! MongoDB find data provider - finds documents in a collection
public class MongoDbFindDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "MongoDbFindDataProvider",
            "supports_request": True,
            "supports_children": False,
            "constructor_options": NOTHING,
        };

        #! Request type
        const RequestType = new MongoDbFindRequestDataType();

        #! Response type
        const ResponseType = new MongoDbFindResponseDataType();

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
        return "find";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "MongoDB find data provider - finds documents in a collection";
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
        hash<auto> opts;
        if (req.limit) {
            opts.limit = req.limit;
        }
        if (req.skip) {
            opts.skip = req.skip;
        }
        if (req.sort) {
            opts.sort = req.sort;
        }
        if (req.projection) {
            opts.projection = req.projection;
        }

        MongoCursor cursor = coll.find(req.filter ?? {}, opts);
        list<hash<auto>> docs = cursor.toList();

        return {
            "documents": docs,
            "count": docs.size(),
        };
    }
}

#! Find request data type
public class MongoDbFindRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "filter": {
                "display_name": "Filter",
                "short_desc": "Query filter for matching documents",
                "type": HashOrNothingType,
                "desc": "A query filter hash to match documents (empty or NOTHING for all documents)",
            },
            "limit": {
                "display_name": "Limit",
                "short_desc": "Maximum number of documents to return",
                "type": SoftIntOrNothingType,
                "desc": "Maximum number of documents to return",
            },
            "skip": {
                "display_name": "Skip",
                "short_desc": "Number of documents to skip",
                "type": SoftIntOrNothingType,
                "desc": "Number of documents to skip before returning results",
            },
            "sort": {
                "display_name": "Sort",
                "short_desc": "Sort specification",
                "type": HashOrNothingType,
                "desc": "Sort specification hash (field: 1 for ascending, -1 for descending)",
            },
            "projection": {
                "display_name": "Projection",
                "short_desc": "Fields to include/exclude",
                "type": HashOrNothingType,
                "desc": "Projection hash (field: 1 to include, 0 to exclude)",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("MongoDbFindRequest", Fields) {
    }
}

#! Find response data type
public class MongoDbFindResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "documents": {
                "display_name": "Documents",
                "short_desc": "List of matching documents",
                "type": SoftListType,
                "desc": "List of documents matching the query filter",
            },
            "count": {
                "display_name": "Count",
                "short_desc": "Number of documents returned",
                "type": IntType,
                "desc": "The number of documents returned",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("MongoDbFindResponse", Fields) {
    }
}
}
