# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright 2026 Qore Technologies, s.r.o.

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

# The cases of the DataStackGuard-<n>.qtest tests: each runs one part of them, so that each takes a short time (all of
# them took about 70 s alone in a debug build, nearly all of it in their child processes, one at a time, so that their
# time grew with the load, as call-stack-guard's did beyond the 300 s limit of run_tests.sh: qore#6856), and together
# they run every case (see addPartCase()); the parts are balanced by the time of their cases

#! checks that library operations that recurse with the nesting of data raise STACK-LIMIT-EXCEEDED
/** Each script builds deeply nested values iteratively on its main thread, and runs an operation that recurses with
    their nesting on a thread with a small stack; see StackGuardCaseTest
*/
class DataStackGuardBase inherits StackGuardCaseTest {
    public {
        #! code that builds nested values iteratively, so that building them does not recurse
        const Builders = "# the nesting of the values: far beyond what the small thread stack holds when recursing
const Depth = 100000;
# the nesting of the YAML documents: libyaml 0.2.7 rejects a document nested more than 1000 levels deep
# (YAML-PARSER-ERROR: \"exceeded maximum nesting depth\"), and its scanner reads up to 1024 characters ahead, so that
# a deeper document fails before the parser recurses at all; the document is nested as deeply as libyaml allows, and
# the thread runs with a stack too small for that (YamlStackSize)
const YamlDepth = 1000;

list sub deep_list(int n) {
    list l = ();
    for (int i = 0; i < n; ++i) {
        l = (l,);
    }
    return l;
}

hash sub deep_hash(int n) {
    hash h = {};
    for (int i = 0; i < n; ++i) {
        h = {\"a\": h};
    }
    return h;
}

class Link {
    public {
        *Link next;
    }
}

Link sub deep_object(int n) {
    Link head();
    Link l = head;
    for (int i = 0; i < n; ++i) {
        Link next();
        l.next = next;
        l = next;
    }
    return head;
}
";

        #! the stack size of the thread that parses YAML: the limit of the stack guard is 48 KiB above the start of
        #! the stack (the guard reserves 80 KiB), which the YamlDepth levels of the document exceed when parsing takes
        #! more than 48 bytes of stack per level; it takes about 190 bytes in a debug build and 270 bytes in a release
        #! build on x86_64 Linux, which reach the limit at about 220 and 160 levels
        const YamlStackSize = 128 * 1024;

        #! the cases that format nested values (part 1)
        const FormattingCases = ("list %y", "list %N", "list %n", "hash %y", "hash %N", "object %y", "object %N");

        #! the cases that compare nested values (part 2); the other cases serialize and parse them (part 3)
        const ComparisonCases = ("list ==", "list !=", "list ===", "list !==", "hash ==", "hash ===", "list inlist()",
            "list inlist_hard()");

        #! the cases, by name
        const Cases = {
            "list %y": <StackCase>{
                "setup": "auto v = deep_list(Depth);",
                "op": "sprintf(\"%y\", v)",
            },
            "list %N": <StackCase>{
                "setup": "auto v = deep_list(Depth);",
                "op": "sprintf(\"%N\", v)",
            },
            "list %n": <StackCase>{
                "setup": "auto v = deep_list(Depth);",
                "op": "sprintf(\"%n\", v)",
            },
            "hash %y": <StackCase>{
                "setup": "auto v = deep_hash(Depth);",
                "op": "sprintf(\"%y\", v)",
            },
            "hash %N": <StackCase>{
                "setup": "auto v = deep_hash(Depth);",
                "op": "sprintf(\"%N\", v)",
            },
            "object %y": <StackCase>{
                "setup": "auto v = deep_object(Depth);",
                "op": "sprintf(\"%y\", v)",
            },
            "object %N": <StackCase>{
                "setup": "auto v = deep_object(Depth);",
                "op": "sprintf(\"%N\", v)",
            },
            "list ==": <StackCase>{
                "setup": "auto v = (deep_list(Depth), deep_list(Depth));",
                "op": "v[0] == v[1]",
            },
            "list !=": <StackCase>{
                "setup": "auto v = (deep_list(Depth), deep_list(Depth));",
                "op": "v[0] != v[1]",
            },
            "list ===": <StackCase>{
                "setup": "auto v = (deep_list(Depth), deep_list(Depth));",
                "op": "v[0] === v[1]",
            },
            "list !==": <StackCase>{
                "setup": "auto v = (deep_list(Depth), deep_list(Depth));",
                "op": "v[0] !== v[1]",
            },
            "hash ==": <StackCase>{
                "setup": "auto v = (deep_hash(Depth), deep_hash(Depth));",
                "op": "v[0] == v[1]",
            },
            "hash ===": <StackCase>{
                "setup": "auto v = (deep_hash(Depth), deep_hash(Depth));",
                "op": "v[0] === v[1]",
            },
            "list inlist()": <StackCase>{
                "setup": "auto v = (deep_list(Depth), deep_list(Depth));",
                "op": "inlist(v[0], (v[1],))",
            },
            "list inlist_hard()": <StackCase>{
                "setup": "auto v = (deep_list(Depth), deep_list(Depth));",
                "op": "inlist_hard(v[0], (v[1],))",
            },
            "Serializable::serialize() list": <StackCase>{
                "setup": "auto v = deep_list(Depth);",
                "op": "Serializable::serialize(v)",
            },
            "Serializable::serialize() hash": <StackCase>{
                "setup": "auto v = deep_hash(Depth);",
                "op": "Serializable::serialize(v)",
            },
            "Serializable::serializeToData() hash": <StackCase>{
                "setup": "auto v = deep_hash(Depth);",
                "op": "Serializable::serializeToData(v)",
            },
            "Serializable::deserialize() stream": <StackCase>{
                # a serialization stream with hashes nested Depth levels deep, as untrusted input can be: the header,
                # then for each level a hash (code 0) with one member (int code 7, size 1) with key "a" (string code
                # 3, int code 7, length 1, "a"), then the innermost value 0 (int code 7, 0)
                "setup": "binary v = parse_hex_string(\"515300312e3200\");
binary level = parse_hex_string(\"00070103070161\");
for (int i = 0; i < Depth; ++i) {
    v += level;
}
v += parse_hex_string(\"0700\");",
                "op": "Serializable::deserialize(v)",
            },
            "Serializable::deserialize() data": <StackCase>{
                # serialization data with hashes nested Depth levels deep, as untrusted input can be
                "setup": "hash<auto> data = {\"_hash\": \"^auto\", \"_members\": {}};
for (int i = 0; i < Depth; ++i) {
    data = {\"_hash\": \"^auto\", \"_members\": {\"a\": data}};
}
auto v = <SerializationInfo>{\"_data\": data};",
                "op": "Serializable::deserialize(v)",
            },
            "make_yaml() list": <StackCase>{
                "requires": ("yaml",),
                "setup": "auto v = deep_list(Depth);",
                "op": "make_yaml(v)",
            },
            "make_yaml() hash": <StackCase>{
                "requires": ("yaml",),
                "setup": "auto v = deep_hash(Depth);",
                "op": "make_yaml(v)",
            },
            "YamlStreamWriter::write()": <StackCase>{
                "requires": ("yaml",),
                "setup": "auto v = deep_list(Depth);
string sub write_yaml(auto v) {
    StringOutputStream sos();
    YamlStreamWriter writer(sos);
    writer.startDocument();
    writer.write(v);
    writer.endDocument();
    writer.close();
    return sos.getData();
}",
                "op": "write_yaml(v)",
            },
            "parse_yaml()": <StackCase>{
                "requires": ("yaml",),
                "setup": "string v = strmul(\"[\", YamlDepth) + strmul(\"]\", YamlDepth);",
                "op": "parse_yaml(v)",
                "stack_size": YamlStackSize,
            },
            "MongoCollection::insertOne()": <StackCase>{
                # the document is converted to BSON before the client connects; no server is needed
                "requires": ("mongodb",),
                "setup": "auto v = deep_hash(Depth);
MongoClient client(\"mongodb://127.0.0.1:1/?serverSelectionTimeoutMS=1000\");
our MongoCollection collection = client.getDatabase(\"test\").getCollection(\"test\");",
                "op": "collection.insertOne(v)",
            },
            "YamlDocumentIterator::getValue()": <StackCase>{
                "requires": ("yaml",),
                "setup": "string v = strmul(\"[\", YamlDepth) + strmul(\"]\", YamlDepth);
auto sub iterate_yaml(string v) {
    YamlDocumentIterator i(v);
    i.next();
    return i.getValue();
}",
                "op": "iterate_yaml(v)",
                "stack_size": YamlStackSize,
            },
        };
    }

    private {
        #! the part of the cases that this test runs (see addPartCase())
        int part;
    }

    constructor(string name, int part) : StackGuardCaseTest(name, NOTHING, Builders) {
        self.part = part;
        map addPartCase(1, $1.key, \testCase(), ($1.key, $1.value)), Cases{FormattingCases}.pairIterator();
        map addPartCase(2, $1.key, \testCase(), ($1.key, $1.value)), Cases{ComparisonCases}.pairIterator();
        # serialization, YAML and BSON
        map addPartCase(3, $1.key, \testCase(), ($1.key, $1.value)),
            (Cases - (FormattingCases + ComparisonCases)).pairIterator();
        set_return_value(main());
    }

    #! adds the case \a name if it is in the part \a case_part that this test runs
    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }
}
