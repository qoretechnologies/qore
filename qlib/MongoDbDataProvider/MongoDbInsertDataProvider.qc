# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MongoDbInsertDataProvider class definition

/** MongoDbInsertDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! MongoDB insert data provider - inserts documents into a collection
public class MongoDbInsertDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "MongoDbInsertDataProvider",
            "supports_request": True,
            "supports_children": False,
            "constructor_options": NOTHING,
        };

        #! Request type
        const RequestType = new MongoDbInsertRequestDataType();

        #! Response type
        const ResponseType = new MongoDbInsertResponseDataType();

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
        return "insert";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "MongoDB insert data provider - inserts documents into a collection";
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
        if (req.documents) {
            # Insert multiple documents
            hash<auto> result = coll.insertMany(req.documents);
            return {
                "inserted_count": result.insertedCount,
                # insertMany doesn't return individual IDs in libmongoc
                "inserted_ids": (),
            };
        } else if (req.document) {
            # Insert single document
            hash<auto> result = coll.insertOne(req.document);
            return {
                "inserted_count": 1,
                "inserted_ids": (result.insertedId,),
            };
        }
        throw "MONGODB-INSERT-ERROR", "either 'document' or 'documents' must be provided";
    }
}

#! Insert request data type
public class MongoDbInsertRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "document": {
                "display_name": "Document",
                "short_desc": "Single document to insert",
                "type": HashOrNothingType,
                "desc": "A single document (hash) to insert into the collection",
            },
            "documents": {
                "display_name": "Documents",
                "short_desc": "Multiple documents to insert",
                "type": SoftListOrNothingType,
                "desc": "A list of documents (hashes) to insert into the collection",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("MongoDbInsertRequest", Fields) {
    }
}

#! Insert response data type
public class MongoDbInsertResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "inserted_count": {
                "display_name": "Inserted Count",
                "short_desc": "Number of documents inserted",
                "type": IntType,
                "desc": "The number of documents successfully inserted",
            },
            "inserted_ids": {
                "display_name": "Inserted IDs",
                "short_desc": "IDs of inserted documents",
                "type": SoftListType,
                "desc": "List of _id values for the inserted documents",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("MongoDbInsertResponse", Fields) {
    }
}
}
