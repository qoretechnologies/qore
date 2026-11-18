# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# applies the trim operator to lvalues holding strings that are not valid in their encoding, so that it raises
# INVALID-ENCODING, and prints for each lvalue the exception and how many references to the string the operator left
# behind (from dbg_node_info(); "n/a" without library debugging), which trim-exception-cleanup.qtest checks
%modern

class Holder {
    public {
        string s;
    }

    constructor(string s) {
        self.s = s;
    }

    #! trims the member through "self"
    string trimSelf() {
        int before = refs(s);
        *string err;
        try {
            trim s;
        } catch (hash<ExceptionInfo> ex) {
            err = ex.err;
        }
        return result("self member", err, before, refs(s));
    }
}

#! returns a new string that is not valid UTF-8 ("h\xe9llo" with two trailing spaces), held only by the caller
string sub new_invalid() {
    return binary_to_string(<68e96c6c6f2020>, "UTF-8");
}

#! the reference count of the node holding \a v, or -1 without library debugging
int sub refs(auto v) {
    if (!Option::HAVE_LIBRARY_DEBUGGING) {
        return -1;
    }
    # the description shows the string, which is not valid UTF-8, so it is read bytewise
    string info = force_encoding(call_function("dbg_node_info", v, True), "ISO-8859-1");
    *list<*string> r = (info =~ x/refs: ([0-9]+)/);
    if (!r[0]) {
        throw "REFS-ERROR", sprintf("no reference count in %y", info);
    }
    return r[0].toInt();
}

#! the line for one lvalue: its label, the exception raised, and the references to the string left behind
/** the reference counts are read in the same way before and after the operator, as reading them through another
    call can add a reference
*/
string sub result(string label, *string err, int before, int after) {
    return sprintf("%s: %s %s", label, err ?? "no exception", before < 0 ? "n/a" : sprintf("%d", after - before));
}

string sub check_local() {
    string s = new_invalid();
    int before = refs(s);
    *string err;
    try {
        trim s;
    } catch (hash<ExceptionInfo> ex) {
        err = ex.err;
    }
    return result("local", err, before, refs(s));
}

string sub check_hash_member() {
    hash<auto> h = {"s": new_invalid()};
    int before = refs(h.s);
    *string err;
    try {
        trim h.s;
    } catch (hash<ExceptionInfo> ex) {
        err = ex.err;
    }
    return result("hash member", err, before, refs(h.s));
}

string sub check_list_element() {
    list<string> l = (new_invalid(),);
    int before = refs(l[0]);
    *string err;
    try {
        trim l[0];
    } catch (hash<ExceptionInfo> ex) {
        err = ex.err;
    }
    return result("list element", err, before, refs(l[0]));
}

string sub check_object_member() {
    Holder o(new_invalid());
    int before = refs(o.s);
    *string err;
    try {
        trim o.s;
    } catch (hash<ExceptionInfo> ex) {
        err = ex.err;
    }
    return result("object member", err, before, refs(o.s));
}

string sub trim_reference(reference<string> r) {
    *string err;
    try {
        trim r;
    } catch (hash<ExceptionInfo> ex) {
        err = ex.err;
    }
    return err ?? "no exception";
}

string sub check_reference() {
    string s = new_invalid();
    int before = refs(s);
    string err = trim_reference(\s);
    return result("reference", err, before, refs(s));
}

#! the loop of SmtpClient::getResponseDesc(): trims a loop variable sharing its string with the list, so the
#! operator trims a copy; the copy is not reachable afterwards, only a leak check (valgrind) sees it left behind, but
#! the strings of the list must keep their references
string sub check_foreach() {
    list<string> l = (new_invalid(), new_invalid());
    int before = refs(l[0]);
    *string err;
    try {
        foreach string line in (l) {
            trim line;
        }
    } catch (hash<ExceptionInfo> ex) {
        err = ex.err;
    }
    return result("foreach variable", err, before, refs(l[0]));
}

map printf("%s\n", $1), (
    check_local(),
    check_hash_member(),
    check_list_element(),
    check_object_member(),
    new Holder(new_invalid()).trimSelf(),
    check_reference(),
    check_foreach(),
);
