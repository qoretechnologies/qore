#!/usr/bin/env qore
%new-style
%strict-args
%require-types

%requires mongodb

ObjectId oid();
printf("ObjectId: %s\n", oid.toString());

MongoClient client("mongodb://localhost:27017");
printf("Connected: %s\n", client.ping() ? "yes" : "no");
