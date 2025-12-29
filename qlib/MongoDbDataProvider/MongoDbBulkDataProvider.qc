# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MongoDbBulkDataProvider class definition

/** MongoDbBulkDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! MongoDB bulk data provider - executes bulk write operations on a collection
public class MongoDbBulkDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "MongoDbBulkDataProvider",
            "supports_request": True,
            "supports_children": False,
            "constructor_options": NOTHING,
        };

        #! Request type
        const RequestType = new MongoDbBulkRequestDataType();

        #! Response type
        const ResponseType = new MongoDbBulkResponseDataType();

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
        return "bulk";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "MongoDB bulk data provider - executes bulk write operations on a collection";
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
        if (!req.operations) {
            throw "MONGODB-BULK-ERROR", "operations list is required";
        }

        int inserted_count = 0;
        int matched_count = 0;
        int modified_count = 0;
        int deleted_count = 0;
        int upserted_count = 0;
        list<auto> inserted_ids = ();
        list<auto> upserted_ids = ();

        # Process each operation
        foreach hash<auto> op in (req.operations) {
            if (op.insertOne) {
                hash<auto> result = coll.insertOne(op.insertOne.document);
                ++inserted_count;
                inserted_ids += result.insertedId;
            } else if (op.updateOne) {
                # Handle upsert manually if requested
                if (op.updateOne.upsert) {
                    *hash<auto> existing = coll.findOne(op.updateOne.filter ?? {});
                    if (!existing) {
                        # Insert new document
                        hash<auto> doc = op.updateOne.filter ?? {};
                        if (op.updateOne.update."$set") {
                            doc += op.updateOne.update."$set";
                        }
                        hash<auto> insertResult = coll.insertOne(doc);
                        ++upserted_count;
                        upserted_ids += insertResult.insertedId;
                        continue;
                    }
                }
                hash<auto> result = coll.updateOne(op.updateOne.filter ?? {}, op.updateOne.update);
                matched_count += result.matchedCount;
                modified_count += result.modifiedCount;
            } else if (op.updateMany) {
                # Handle upsert manually if requested
                if (op.updateMany.upsert) {
                    *hash<auto> existing = coll.findOne(op.updateMany.filter ?? {});
                    if (!existing) {
                        # Insert new document
                        hash<auto> doc = op.updateMany.filter ?? {};
                        if (op.updateMany.update."$set") {
                            doc += op.updateMany.update."$set";
                        }
                        hash<auto> insertResult = coll.insertOne(doc);
                        ++upserted_count;
                        upserted_ids += insertResult.insertedId;
                        continue;
                    }
                }
                hash<auto> result = coll.updateMany(op.updateMany.filter ?? {}, op.updateMany.update);
                matched_count += result.matchedCount;
                modified_count += result.modifiedCount;
            } else if (op.deleteOne) {
                hash<auto> result = coll.deleteOne(op.deleteOne.filter ?? {});
                deleted_count += result.deletedCount;
            } else if (op.deleteMany) {
                hash<auto> result = coll.deleteMany(op.deleteMany.filter ?? {});
                deleted_count += result.deletedCount;
            } else if (op.replaceOne) {
                # Implement replaceOne as deleteOne + insertOne
                hash<auto> filter = op.replaceOne.filter ?? {};
                *hash<auto> existing = coll.findOne(filter);
                if (existing) {
                    coll.deleteOne(filter);
                    hash<auto> insertResult = coll.insertOne(op.replaceOne.replacement);
                    ++matched_count;
                    ++modified_count;
                } else if (op.replaceOne.upsert) {
                    hash<auto> insertResult = coll.insertOne(op.replaceOne.replacement);
                    ++upserted_count;
                    upserted_ids += insertResult.insertedId;
                }
            } else {
                throw "MONGODB-BULK-ERROR", sprintf("unknown operation type in bulk request: %y", keys op);
            }
        }

        return {
            "inserted_count": inserted_count,
            "matched_count": matched_count,
            "modified_count": modified_count,
            "deleted_count": deleted_count,
            "upserted_count": upserted_count,
            "inserted_ids": inserted_ids,
            "upserted_ids": upserted_ids,
        };
    }
}

#! Bulk request data type
public class MongoDbBulkRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "operations": {
                "display_name": "Operations",
                "short_desc": "List of bulk write operations",
                "type": SoftListType,
                "desc": "A list of bulk write operations. Each operation is a hash with one of the following keys:\n"
                    "- insertOne: {document: {...}}\n"
                    "- updateOne: {filter: {...}, update: {...}, upsert: bool}\n"
                    "- updateMany: {filter: {...}, update: {...}, upsert: bool}\n"
                    "- deleteOne: {filter: {...}}\n"
                    "- deleteMany: {filter: {...}}\n"
                    "- replaceOne: {filter: {...}, replacement: {...}, upsert: bool}",
                "required": True,
            },
            "ordered": {
                "display_name": "Ordered",
                "short_desc": "Execute operations in order",
                "type": SoftBoolType,
                "desc": "If True (default), operations execute in order and stop on first error; "
                    "if False, operations may execute in any order and continue on errors",
                "default_value": True,
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("MongoDbBulkRequest", Fields) {
    }
}

#! Bulk response data type
public class MongoDbBulkResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "inserted_count": {
                "display_name": "Inserted Count",
                "short_desc": "Number of documents inserted",
                "type": IntType,
                "desc": "The number of documents inserted",
            },
            "matched_count": {
                "display_name": "Matched Count",
                "short_desc": "Number of documents matched for update",
                "type": IntType,
                "desc": "The number of documents matched for update operations",
            },
            "modified_count": {
                "display_name": "Modified Count",
                "short_desc": "Number of documents modified",
                "type": IntType,
                "desc": "The number of documents actually modified",
            },
            "deleted_count": {
                "display_name": "Deleted Count",
                "short_desc": "Number of documents deleted",
                "type": IntType,
                "desc": "The number of documents deleted",
            },
            "upserted_count": {
                "display_name": "Upserted Count",
                "short_desc": "Number of documents upserted",
                "type": IntType,
                "desc": "The number of documents upserted (inserted via upsert)",
            },
            "inserted_ids": {
                "display_name": "Inserted IDs",
                "short_desc": "IDs of inserted documents",
                "type": SoftListType,
                "desc": "List of _id values for inserted documents",
            },
            "upserted_ids": {
                "display_name": "Upserted IDs",
                "short_desc": "IDs of upserted documents",
                "type": SoftListType,
                "desc": "List of _id values for upserted documents",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("MongoDbBulkResponse", Fields) {
    }
}
}
