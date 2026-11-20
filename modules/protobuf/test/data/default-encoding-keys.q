#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

/*  default-encoding-keys.q Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License

    The child script of default-encoding-keys.qtest, run with a default character encoding given by the test; it has
    only ASCII characters, so that it means the same in any default encoding, and prints each result as
    "<name>=<hex bytes>".

    usage: default-encoding-keys.q
*/

%modern
%requires protobuf

# "k" and an e acute in UTF-8
string key = "k" + binary_to_string(<c3a9>, "UTF-8");

sub out(string name, data value) {
    printf("%s=%s\n", name, make_hex_string(binary(value)));
}

out("default", get_default_encoding());

Protobuf::ProtobufSchema schema("syntax = \"proto2\";\n"
    "package test;\n"
    "message M { map<string, string> m = 1; }\n");

# the hash key is in the default encoding; the map key on the wire is UTF-8
hash<auto> m = {};
m{key} = "v";
out("map-key", (keys m)[0]);
out("encoded", schema.encode("test.M", {"m": m}));

# a UTF-8 map key decoded is converted to the default encoding, so the key is found
hash<auto> h = schema.decode("test.M", <0a080a036bc3a9120176>);
out("decoded-key", (keys h.m)[0]);
out("decoded-found", exists h.m{key} ? "yes" : "no");
out("decoded-value", h.m{key} ?? "");

h = schema.fromJson("test.M", "{\"m\":{\"" + key + "\":\"v\"}}");
out("json-key", (keys h.m)[0]);
out("json-found", exists h.m{key} ? "yes" : "no");

string json = schema.toJson("test.M", {"m": m});
out("to-json", json);
out("to-json-encoding", json.encoding());

# a map key that the default encoding cannot hold (the euro sign in UTF-8) cannot be a hash key in it
string err = "none";
try {
    schema.decode("test.M", <0a090a046be282ac120176>);
} catch (hash<ExceptionInfo> ex) {
    err = ex.err;
}
out("euro-key-error", err);
