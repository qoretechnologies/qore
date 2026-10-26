#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

%modern

#! the value of a cast, which the cast also holds as the temporary that it converts, used after its scope ends
/** Each case evaluates a cast in a scope of its own whose temporaries are released before the value is used: the
    operand of a change to a member, the value of a switch statement, and the list of a foreach statement.  A cast
    whose operand already has its type returns that operand, which it holds as a temporary, so the value must outlive
    the release of the temporaries: each document is destroyed only when the value that holds it goes out of scope,
    after the case prints what it sees.
*/

our list<string> Log = ();

class Doc {
    private {
        string name;
    }

    constructor(string name) {
        self.name = name;
    }

    destructor() {
        Log += "~" + name;
    }

    string getName() {
        return name;
    }
}

hashdecl M {
    string text;
    Doc doc;
}

hashdecl R {
    *list<hash<M>> msgs;
    *hash<M> one;
    *hash<string, Doc> docs;
}

#! returns a hash<M> with its own document, as a value of an unknown type, so the cast is made at run time
auto sub make_m(string name) {
    return <M>{"text": name, "doc": new Doc(name)};
}

#! returns a hash<string, Doc> with its own documents, as a value of an unknown type
auto sub make_docs(list<string> names) {
    hash<string, Doc> rv = map {$1: new Doc($1)}, names;
    return rv;
}

string sub run_list() {
    hash<R> rv();
    rv.msgs = cast<list<hash<M>>>(map cast<hash<M>>({"text": $1, "doc": new Doc($1)}), ("a", "b"));
    string seen = sprintf("list: %y %y", Log, (map $1.doc.getName(), rv.msgs));
    return seen;
}

string sub run_hashdecl() {
    hash<R> rv();
    rv.one = cast<hash<M>>(make_m("c"));
    string seen = sprintf("hashdecl: %y %y", Log, rv.one.doc.getName());
    return seen;
}

string sub run_complex_hash() {
    hash<R> rv();
    rv.docs = cast<hash<string, Doc>>(make_docs(("d", "e")));
    string seen = sprintf("complex hash: %y %y", Log, (map $1.getName(), rv.docs.iterator()));
    return seen;
}

string sub run_push() {
    hash<R> rv();
    rv.msgs = ();
    push rv.msgs, cast<hash<M>>(make_m("f"));
    string seen = sprintf("push: %y %y", Log, (map $1.doc.getName(), rv.msgs));
    return seen;
}

string sub run_switch() {
    string seen;
    switch (cast<list<hash<M>>>(map cast<hash<M>>({"text": $1, "doc": new Doc($1)}), ("g",))) {
        default:
            seen = sprintf("switch: %y", Log);
    }
    return seen;
}

string sub run_foreach() {
    list<string> names = ();
    *list<string> during;
    foreach hash<M> m in (cast<list<hash<M>>>(map cast<hash<M>>({"text": $1, "doc": new Doc($1)}), ("h", "i"))) {
        if (!exists during) {
            during = Log;
        }
        names += m.doc.getName();
    }
    return sprintf("foreach: %y %y", during, names);
}

#! returns a hash<R> with a member that holds its own document, as a value of an unknown type
auto sub make_holder(string name) {
    return <R>{"one": <M>{"text": name, "doc": new Doc(name)}};
}

string sub run_member_chain() {
    hash<R> rv();
    rv.one = cast<hash<R>>(make_holder("j")).one;
    string seen = sprintf("member chain: %y %y", Log, rv.one.doc.getName());
    return seen;
}

foreach code c in ((\run_list(), \run_hashdecl(), \run_complex_hash(), \run_push(), \run_switch(), \run_foreach(),
        \run_member_chain())) {
    Log = ();
    string seen = c();
    printf("%s; after: %y\n", seen, Log);
}
