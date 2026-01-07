# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MongoDbUpdateDataProvider class definition

/** MongoDbUpdateDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! MongoDB update data provider - updates documents in a collection
public class MongoDbUpdateDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "MongoDbUpdateDataProvider",
            "supports_request": True,
            "supports_children": False,
            "constructor_options": NOTHING,
        };

        #! Request type
        const RequestType = new MongoDbUpdateRequestDataType();

        #! Response type
        const ResponseType = new MongoDbUpdateResponseDataType();

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
        return "update";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "MongoDB update data provider - updates documents in a collection";
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
        # Build options hash with upsert if requested
        *hash<auto> opts;
        if (req.upsert) {
            opts = {"upsert": True};
        }

        hash<auto> result;
        if (req.update_one) {
            result = coll.updateOne(req.filter ?? {}, req.update, opts);
        } else {
            result = coll.updateMany(req.filter ?? {}, req.update, opts);
        }

        return {
            "matched_count": result.matchedCount,
            "modified_count": result.modifiedCount,
            "upserted_id": result.upsertedId,
        };
    }
}

#! Update request data type
public class MongoDbUpdateRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "filter": {
                "display_name": "Filter",
                "short_desc": "Query filter for matching documents",
                "type": HashOrNothingType,
                "desc": "A query filter hash to match documents to update",
            },
            "update": {
                "display_name": "Update",
                "short_desc": "Update operations to apply",
                "type": HashType,
                "desc": "The update operations to apply (e.g., {\"$set\": {\"field\": \"value\"}})",
                "required": True,
            },
            "update_one": {
                "display_name": "Update One",
                "short_desc": "Update only one document",
                "type": SoftBoolType,
                "desc": "If True, update only the first matching document; otherwise update all matching documents",
                "default_value": False,
            },
            "upsert": {
                "display_name": "Upsert",
                "short_desc": "Insert if not found",
                "type": SoftBoolType,
                "desc": "If True, insert a new document if no documents match the filter",
                "default_value": False,
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("MongoDbUpdateRequest", Fields) {
    }
}

#! Update response data type
public class MongoDbUpdateResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "matched_count": {
                "display_name": "Matched Count",
                "short_desc": "Number of documents matched",
                "type": IntType,
                "desc": "The number of documents that matched the filter",
            },
            "modified_count": {
                "display_name": "Modified Count",
                "short_desc": "Number of documents modified",
                "type": IntType,
                "desc": "The number of documents that were actually modified",
            },
            "upserted_id": {
                "display_name": "Upserted ID",
                "short_desc": "ID of upserted document",
                "type": AutoType,
                "desc": "The _id of the upserted document (if upsert occurred)",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("MongoDbUpdateResponse", Fields) {
    }
}
}
