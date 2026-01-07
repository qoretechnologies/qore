# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MongoDbDeleteDataProvider class definition

/** MongoDbDeleteDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! MongoDB delete data provider - deletes documents from a collection
public class MongoDbDeleteDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "MongoDbDeleteDataProvider",
            "supports_request": True,
            "supports_children": False,
            "constructor_options": NOTHING,
        };

        #! Request type
        const RequestType = new MongoDbDeleteRequestDataType();

        #! Response type
        const ResponseType = new MongoDbDeleteResponseDataType();

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
        return "delete";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "MongoDB delete data provider - deletes documents from a collection";
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
        hash<auto> result;
        if (req.delete_one) {
            result = coll.deleteOne(req.filter ?? {});
        } else {
            result = coll.deleteMany(req.filter ?? {});
        }

        return {
            "deleted_count": result.deletedCount,
        };
    }
}

#! Delete request data type
public class MongoDbDeleteRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "filter": {
                "display_name": "Filter",
                "short_desc": "Query filter for matching documents",
                "type": HashOrNothingType,
                "desc": "A query filter hash to match documents to delete",
            },
            "delete_one": {
                "display_name": "Delete One",
                "short_desc": "Delete only one document",
                "type": SoftBoolType,
                "desc": "If True, delete only the first matching document; otherwise delete all matching documents",
                "default_value": False,
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("MongoDbDeleteRequest", Fields) {
    }
}

#! Delete response data type
public class MongoDbDeleteResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "deleted_count": {
                "display_name": "Deleted Count",
                "short_desc": "Number of documents deleted",
                "type": IntType,
                "desc": "The number of documents that were deleted",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("MongoDbDeleteResponse", Fields) {
    }
}
}
