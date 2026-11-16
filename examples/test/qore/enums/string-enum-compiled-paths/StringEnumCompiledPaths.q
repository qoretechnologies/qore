# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of string-enum-compiled-paths.qtest: values of string and number enum types through the string and number
# fast paths of the compiled modes (qore#6254)

%modern
%enable-all-warnings

enum ES : string { X = "x", Y = "yy", N = "12" }
enum EN : number { N1 = 1.5n, N2 = 2.5n }
enum EI : int { One = 1, Two = 2 }

ES sub fs(int i) { return i ? ES::Y : ES::X; }
EN sub fn(int i) { return i ? EN::N2 : EN::N1; }

# the value of a string enum read through the pseudo-methods of strings, as a receiver
auto sub fSize(ES e) { return e.size(); }
auto sub fStrlenMethod(ES e) { return e.strlen(); }
auto sub fLengthMethod(ES e) { return e.length(); }
auto sub fStrlenFunction(ES e) { return strlen(e); }
auto sub fLengthFunction(ES e) { return length(e); }
auto sub fEmpty(ES e) { return e.empty(); }
auto sub fVal(ES e) { return e.val(); }
auto sub fSizep(ES e) { return e.sizep(); }
auto sub fStrp(ES e) { return e.strp(); }
auto sub fIntp(ES e) { return e.intp(); }
auto sub fLwr(ES e) { return e.lwr(); }
auto sub fUpr(ES e) { return e.upr(); }
auto sub fToInt(ES e) { return e.toInt(); }
auto sub fStartsWith(ES e) { return e.startsWith("y"); }
auto sub fEndsWith(ES e) { return e.endsWith("y"); }
auto sub fContains(ES e) { return e.contains("y"); }
auto sub fFind(ES e) { return e.find("y"); }
auto sub fRfind(ES e) { return e.rfind("y"); }
auto sub fSubstr(ES e) { return e.substr(1); }
auto sub fSubstrLength(ES e) { return e.substr(0, 1); }
auto sub fSizeOfCall() { return fs(1).size(); }
auto sub fSizeOfLocal(ES e) { ES c = e; return c.size(); }
auto sub fSizeOfElement(list<ES> l) { return l[0].size(); }
auto sub fSizeOfHashValue(ES e) { hash<string, ES> h = {"a": e}; return h.a.size(); }
auto sub fMapSize(ES e) { return map $1.size(), (e, e); }

# the value of a string enum as the argument of a pseudo-method of strings
auto sub fStartsWithArg(ES e) { string s = "yyz"; return s.startsWith(e); }
auto sub fEndsWithArg(ES e) { string s = "zyy"; return s.endsWith(e); }
auto sub fContainsArg(ES e) { string s = "zyyz"; return s.contains(e); }
auto sub fFindArg(ES e) { string s = "zyyz"; return s.find(e); }
auto sub fRfindArg(ES e) { string s = "zyyzyy"; return s.rfind(e); }

# the value of an int enum as the offset of substr()
auto sub fSubstrEnumOffset(EI o) { string s = "abc"; return s.substr(o); }
auto sub fSubstrEnumLength(EI o) { string s = "abc"; return s.substr(0, o); }

# the value of a string or number enum assigned to a string or number variable is converted to its base type
auto sub fStringLocal(ES e) { string s = e; return s; }
auto sub fStringLocalSize(ES e) { string s = e; return s.size(); }
auto sub fStringLocalFromCall() { string s = fs(1); return s; }
auto sub fStringLocalFromCallSize() { string s = fs(1); return s.size(); }
auto sub fStringLocalInLoop(list<ES> l) { string s = ""; foreach ES e in (l) { s = e; } return s; }
auto sub fNumberLocal(EN n) { number x = n; return x; }
auto sub fNumberLocalFromCall() { number x = fn(1); return x; }
auto sub fNumberLocalPlus(EN n) { number x = n; return x + 1n; }

# values that keep their enum type
auto sub fEnumLocal(ES e) { ES c = e; return c; }
auto sub fEnumCall() { return fs(1); }
auto sub fNumberEnumCall() { return fn(1); }
auto sub fNumberEnumLocal(EN n) { EN c = n; return c; }

# operators on the values give plain values
auto sub fPlus(ES e) { return e + "z"; }
auto sub fIndex(ES e) { return e[0]; }
auto sub fRegex(ES e) { return e =~ /y/; }
auto sub fEquals(ES e) { return e == "yy"; }
auto sub fSprintfString(ES e) { return sprintf("<%s>", e); }
auto sub fNumberPlus(EN n) { return n + 1n; }
auto sub fNumberToString(EN n) { return n.toString(); }

sub show(string label, auto v) {
    if (v.typeCode() == NT_LIST) {
        printf("%s: %y %y\n", label, (map $1.fullType(), v), v);
    } else {
        printf("%s: %s %y\n", label, v.fullType(), v);
    }
}

sub run(string label, code c) {
    try {
        show(label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

ES e = ES::Y;
EN n = EN::N2;
run("size()", auto sub () { return fSize(e); });
run("strlen() method", auto sub () { return fStrlenMethod(e); });
run("length() method", auto sub () { return fLengthMethod(e); });
run("strlen()", auto sub () { return fStrlenFunction(e); });
run("length()", auto sub () { return fLengthFunction(e); });
run("empty()", auto sub () { return fEmpty(e); });
run("val()", auto sub () { return fVal(e); });
run("sizep()", auto sub () { return fSizep(e); });
run("strp()", auto sub () { return fStrp(e); });
run("intp()", auto sub () { return fIntp(ES::N); });
run("lwr()", auto sub () { return fLwr(e); });
run("upr()", auto sub () { return fUpr(e); });
run("toInt()", auto sub () { return fToInt(ES::N); });
run("startsWith()", auto sub () { return fStartsWith(e); });
run("endsWith()", auto sub () { return fEndsWith(e); });
run("contains()", auto sub () { return fContains(e); });
run("find()", auto sub () { return fFind(e); });
run("rfind()", auto sub () { return fRfind(e); });
run("substr()", auto sub () { return fSubstr(e); });
run("substr() with a length", auto sub () { return fSubstrLength(e); });
run("size() of a call", auto sub () { return fSizeOfCall(); });
run("size() of an enum local", auto sub () { return fSizeOfLocal(e); });
run("size() of a list element", auto sub () { return fSizeOfElement((e,)); });
run("size() of a hash value", auto sub () { return fSizeOfHashValue(e); });
run("map size()", auto sub () { return fMapSize(e); });
run("startsWith() argument", auto sub () { return fStartsWithArg(e); });
run("endsWith() argument", auto sub () { return fEndsWithArg(e); });
run("contains() argument", auto sub () { return fContainsArg(e); });
run("find() argument", auto sub () { return fFindArg(e); });
run("rfind() argument", auto sub () { return fRfindArg(e); });
run("substr() enum offset", auto sub () { return fSubstrEnumOffset(EI::One); });
run("substr() enum length", auto sub () { return fSubstrEnumLength(EI::Two); });
run("string local", auto sub () { return fStringLocal(e); });
run("string local size()", auto sub () { return fStringLocalSize(e); });
run("string local from a call", auto sub () { return fStringLocalFromCall(); });
run("string local from a call size()", auto sub () { return fStringLocalFromCallSize(); });
run("string local in a loop", auto sub () { return fStringLocalInLoop((ES::X, ES::Y)); });
run("number local", auto sub () { return fNumberLocal(n); });
run("number local from a call", auto sub () { return fNumberLocalFromCall(); });
run("number local + 1", auto sub () { return fNumberLocalPlus(n); });
run("enum local", auto sub () { return fEnumLocal(e); });
run("enum call", auto sub () { return fEnumCall(); });
run("number enum call", auto sub () { return fNumberEnumCall(); });
run("number enum local", auto sub () { return fNumberEnumLocal(n); });
run("+", auto sub () { return fPlus(e); });
run("[0]", auto sub () { return fIndex(e); });
run("regex", auto sub () { return fRegex(e); });
run("==", auto sub () { return fEquals(e); });
run("sprintf()", auto sub () { return fSprintfString(e); });
run("number + 1", auto sub () { return fNumberPlus(n); });
run("number toString()", auto sub () { return fNumberToString(n); });
