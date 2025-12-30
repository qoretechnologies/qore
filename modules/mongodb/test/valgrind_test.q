#!/usr/bin/env qore
# Comprehensive memory test for MongoDB module
%new-style
%strict-args
%require-types

%requires mongodb

# Test ObjectId creation and destruction
printf("Testing ObjectId...\n");
for (int i = 0; i < 100; i++) {
    ObjectId oid();
    string s = oid.toString();
    ObjectId oid2(s);
}
printf("ObjectId test passed\n");

# Test client connection
printf("Testing connection...\n");
string url = ENV.MONGODB_URL ?? "mongodb://localhost:27017";
MongoClient client(url);
if (!client.ping()) {
    printf("Could not connect to MongoDB\n");
    exit(1);
}
printf("Connection test passed\n");

# Test database operations
printf("Testing database operations...\n");
MongoDatabase db = client.getDatabase("valgrind_test_db");
list<string> colls = db.listCollectionNames();
printf("Database operations test passed\n");

# Test collection CRUD
printf("Testing CRUD operations...\n");
MongoCollection coll = db.createCollection("valgrind_test_coll");

# Insert
for (int i = 0; i < 50; i++) {
    coll.insertOne({"index": i, "data": "test " + string(i)});
}

# Find
MongoCursor cursor = coll.find();
list<auto> docs = cursor.toList();

# Update
for (int i = 0; i < 25; i++) {
    coll.updateOne({"index": i}, {"$set": {"updated": True}});
}

# Delete
for (int i = 0; i < 25; i++) {
    coll.deleteOne({"index": i});
}

# Clean up
coll.drop();
db.drop();
printf("CRUD test passed\n");

# Test aggregation
printf("Testing aggregation...\n");
coll = db.createCollection("agg_test");
for (int i = 0; i < 20; i++) {
    coll.insertOne({"category": ("A", "B", "C")[i % 3], "value": i * 10});
}
cursor = coll.aggregate((
    {"$group": {"_id": "$category", "total": {"$sum": "$value"}}},
    {"$sort": {"total": -1}},
));
docs = cursor.toList();
coll.drop();
db.drop();
printf("Aggregation test passed\n");

printf("All memory tests passed!\n");
