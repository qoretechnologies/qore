# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MongoDbCollectionDataProvider class definition

/** MongoDbCollectionDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! MongoDB collection data provider - provides access to documents in a collection
public class MongoDbCollectionDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "MongoDbCollectionDataProvider",
            "supports_read": True,
            "supports_create": True,
            "supports_update": True,
            "supports_upsert": True,
            "supports_delete": True,
            "supports_native_search": True,
            "supports_bulk_create": True,
            "supports_bulk_upsert": False,
            "supports_children": True,
            "constructor_options": NOTHING,
            "search_options": SearchOptions,
            "create_options": NOTHING,
            "upsert_options": UpsertOptions,
            "transaction_management": False,
            "has_record": True,
            "supports_schema": False,
            "children_can_support_apis": True,
            "children_can_support_records": False,
            "children_can_support_observers": False,
        };

        #! Search options
        const SearchOptions = {
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of documents to return",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "Maximum number of documents to return",
            },
            "skip": <DataProviderOptionInfo>{
                "display_name": "Skip",
                "short_desc": "Number of documents to skip",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "Number of documents to skip before returning results",
            },
            "sort": <DataProviderOptionInfo>{
                "display_name": "Sort",
                "short_desc": "Sort specification",
                "type": AbstractDataProviderTypeMap."*hash",
                "desc": "Sort specification hash (field: 1 for ascending, -1 for descending)",
            },
            "projection": <DataProviderOptionInfo>{
                "display_name": "Projection",
                "short_desc": "Fields to include/exclude",
                "type": AbstractDataProviderTypeMap."*hash",
                "desc": "Projection hash (field: 1 to include, 0 to exclude)",
            },
        };

        #! Upsert options
        const UpsertOptions = {
            "upsert": <DataProviderOptionInfo>{
                "display_name": "Upsert",
                "short_desc": "Create document if not found",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "If True, create a new document if no document matches the filter",
                "default_value": True,
            },
        };

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

        #! The collection name
        string coll_name;

        #! Child provider map
        const ChildMap = {
            "insert": Class::forName("MongoDbDataProvider::MongoDbInsertDataProvider"),
            "find": Class::forName("MongoDbDataProvider::MongoDbFindDataProvider"),
            "update": Class::forName("MongoDbDataProvider::MongoDbUpdateDataProvider"),
            "delete": Class::forName("MongoDbDataProvider::MongoDbDeleteDataProvider"),
            "aggregate": Class::forName("MongoDbDataProvider::MongoDbAggregateDataProvider"),
            "bulk": Class::forName("MongoDbDataProvider::MongoDbBulkDataProvider"),
        };
    }

    #! Creates the object from a MongoClient, database, and collection name
    constructor(MongoClient client, MongoDatabase db, string coll_name) {
        self.client = client;
        self.db = db;
        self.coll_name = coll_name;
        self.coll = db.getCollection(coll_name);
    }

    #! Returns the data provider name
    string getName() {
        return coll_name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("MongoDB collection data provider for collection: %s", coll_name);
    }

    #! Returns the MongoDB collection object
    MongoCollection getCollection() {
        return coll;
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return (
            <DataProviderSummaryInfo>{
                "name": "insert",
                "desc": "Insert documents into the collection",
                "type": "MongoDbInsertDataProvider",
                "supports_request": True,
            },
            <DataProviderSummaryInfo>{
                "name": "find",
                "desc": "Find documents in the collection",
                "type": "MongoDbFindDataProvider",
                "supports_request": True,
            },
            <DataProviderSummaryInfo>{
                "name": "update",
                "desc": "Update documents in the collection",
                "type": "MongoDbUpdateDataProvider",
                "supports_request": True,
            },
            <DataProviderSummaryInfo>{
                "name": "delete",
                "desc": "Delete documents from the collection",
                "type": "MongoDbDeleteDataProvider",
                "supports_request": True,
            },
            <DataProviderSummaryInfo>{
                "name": "aggregate",
                "desc": "Run aggregation pipeline on the collection",
                "type": "MongoDbAggregateDataProvider",
                "supports_request": True,
            },
            <DataProviderSummaryInfo>{
                "name": "bulk",
                "desc": "Execute bulk write operations",
                "type": "MongoDbBulkDataProvider",
                "supports_request": True,
            },
        );
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(client, db, coll);
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the record type for this provider
    /** Returns NOTHING since MongoDB is schemaless.
        This allows any fields to be used in queries.
    */
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        # MongoDB is schemaless, so return NOTHING to allow any fields
        return NOTHING;
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param where_cond the search criteria
        @param search_options the search options after processing by validateSearchOptions()

        @see requestSearchRecordsImpl()
    */
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return new MongoDbRecordIterator(coll, where_cond, search_options);
    }

    #! Creates the given record to the data provider
    /** @param rec a hash representing a single input record
        @param create_options the create options after processing by validateCreateOptions()

        @return the data written to the data provider with the \c _id field populated
    */
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        hash<auto> result = coll.insertOne(rec);
        # Return a new hash with _id to avoid type conflicts
        return {"_id": result.insertedId} + rec;
    }

    #! Updates zero or more records matching the search options
    /** @param set the hash of field data to set
        @param where_cond a hash for identifying the record(s) to be updated
        @param search_options the update options after processing by validateSearchOptions()

        @return the number of records updated
    */
    private int updateRecordsImpl(hash<auto> set, *hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> update = {"$set": set};
        hash<auto> result = coll.updateMany(where_cond ?? {}, update);
        return result.modifiedCount;
    }

    #! Upserts the given record in the data provider
    /** @param rec a hash representing a single input record
        @param upsert_options the upsert options after processing by validateUpsertOptions()

        @return see @ref db_provider_upsert_results for possible values
    */
    private string upsertRecordImpl(hash<auto> rec, *hash<auto> upsert_options) {
        # Use _id as the filter if present
        hash<auto> filter;
        if (rec._id) {
            filter = {"_id": rec._id};
        } else {
            # Without _id, use all fields as the filter
            filter = rec;
        }

        # Perform an atomic upsert to avoid race conditions
        hash<auto> result = coll.updateOne(filter, {"$set": rec}, {"upsert": True});

        # If upsertedId is present, a new document was inserted; otherwise an existing one was updated
        if (result.upsertedId) {
            return UpsertResultInserted;
        }
        return UpsertResultUpdated;
    }

    #! Deletes zero or more records
    /** @param where_cond a hash for identifying the record(s) to be deleted
        @param search_options the delete options after processing by validateSearchOptions()

        @return the number of records deleted
    */
    private int deleteRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> result = coll.deleteMany(where_cond ?? {});
        return result.deletedCount;
    }
}
}
