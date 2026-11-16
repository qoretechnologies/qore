# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of string-enum-compiled-paths.qtest: values of string and number enum types through the string and number
# fast paths of the compiled modes (qore#6254)

%modern
%enable-all-warnings

enum ES : string { X = "x", Y = "yy", N = "12" }
enum EN : number { N1 = 1.5n, N2 = 2.5n }
enum EI : int { One = 1, Two = 2 }
enum EL : string { Long = "a string value longer than an inline short string" }
enum EU : string { U = "äöü" }

hashdecl HS {
    string s;
}

class C {
    string get(ES e) { return e; }
    static string getStatic(ES e) { return e; }
    ES getEnum(ES e) { return e; }
    *ES getOptionalEnum(ES e) { return e; }
}

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

# the value of a string or number enum returned by a function, a method or a closure declared with another type is
# converted to the type's base type
string sub fReturnString(ES e) { return e; }
*string sub fReturnOptionalString(ES e) { return e; }
softstring sub fReturnSoftString(ES e) { return e; }
number sub fReturnNumber(EN n) { return n; }
string sub fReturnStringMixed(int i, ES e) { if (i) { return e; } return "plain"; }
auto sub fReturnStringSize(ES e) { return fReturnString(e).size(); }
auto sub fReturnStringStrlen(ES e) { return strlen(fReturnString(e)); }
auto sub fReturnStringLocal(ES e) { string s = fReturnString(e); return s; }
auto sub fReturnStringMixedSize(ES e) { return fReturnStringMixed(1, e).size(); }
auto sub fMethodReturn(ES e) { C c(); return c.get(e); }
auto sub fStaticMethodReturn(ES e) { return C::getStatic(e); }
auto sub fClosureReturn(ES e) { code c = string sub (ES x) { return x; }; return c(e); }

softint sub fReturnSoftInt(ES e) { return e; }
int sub fReturnInt(EI i) { return i; }
number sub fReturnIntAsNumber(EI i) { return i; }

# the value of an enum returned by a function, a method or a closure declared with its own enum type keeps it
ES sub fReturnEnum(ES e) { return e; }
*ES sub fReturnOptionalEnum(ES e) { return e; }
auto sub fMethodReturnEnum(ES e) { C c(); return c.getEnum(e); }
auto sub fMethodReturnOptionalEnum(ES e) { C c(); return c.getOptionalEnum(e); }
auto sub fClosureReturnEnum(ES e) { code c = ES sub (ES x) { return x; }; return c(e); }

# the value of a string enum in a string-typed hash value or hashdecl member (where it is kept as the enum's member)
# through the string paths
auto sub fHashValueSize(hash<string, string> h) { return h.a.size(); }
auto sub fHashValueEmpty(hash<string, string> h) { return h.a.empty(); }
auto sub fHashValueVal(hash<string, string> h) { return h.a.val(); }
auto sub fHashLocalSize(ES e) { hash<string, string> h = {"a": e}; return h.a.size(); }
auto sub fHashLocalEmpty(ES e) { hash<string, string> h = {"a": e}; return h.a.empty(); }
auto sub fHashDeclMemberSize(hash<HS> h) { return h.s.size(); }
auto sub fHashDeclMemberEmpty(hash<HS> h) { return h.s.empty(); }
auto sub fHashDeclMemberVal(hash<HS> h) { return h.s.val(); }
auto sub fHashDeclMemberLength(hash<HS> h) { return h.s.length(); }

# the value of a string enum in a string-typed hashdecl member concatenated with other strings, then measured
string sub fConcat2(string a, string b) { return a + b; }
string sub fConcat3(string a, string b, string c) { return a + b + c; }
int sub fConcatLength(string a, string b) { return fConcat2(a, b).length(); }
int sub fConcatLocalLength(string a, string b) { string r = fConcat2(a, b); return r.length(); }
int sub fConcatSize(string a, string b, string c) { return fConcat3(a, b, c).size(); }
int sub fConcatFind(string a, string b, string c) { return fConcat3(a, b, c).find("r"); }
# the member is passed as a string argument, which keeps the enum's member as the hashdecl member does
list<int> sub fConcatMember(ES e) {
    hash<HS> h = <HS>{"s": e};
    return (fConcatLength(h.s, "q"), fConcatLocalLength(h.s, "q"), fConcatSize("q", h.s, "r"),
        fConcatFind("q", h.s, "r"));
}
bool sub fConcatStartsWith(string a, string b, string c, string p) { return fConcat3(a, b, c).startsWith(p); }
bool sub fConcatContains(string a, string b, string c, string p) { return fConcat3(a, b, c).contains(p); }
int sub fConcatFindParam(string a, string b, string c, string p) { return fConcat3(a, b, c).find(p); }
string sub fAppend(string a, string b) { string r = a; r += b; return r; }
# a search of a concatenation with a pattern that is a parameter, and an append of the member
list<auto> sub fConcatSearchMember(ES e, string p) {
    hash<HS> h = <HS>{"s": e};
    return (fConcatStartsWith(h.s, "q", "r", p), fConcatContains("q", h.s, "r", p), fConcatFindParam("q", h.s, "r", p),
        fAppend("q", h.s), fAppend(h.s, "q"));
}
list<int> sub fConcatMultiByteMember(EU u) {
    hash<HS> h = <HS>{"s": u};
    return (fConcatLength(h.s, "q"), fConcatLocalLength(h.s, "q"), fConcatSize("q", h.s, "r"),
        fConcatFind("q", h.s, "r"));
}

# a string enum value that is not an inline short string
auto sub fLongSize(EL e) { return e.size(); }
auto sub fLongEmpty(EL e) { return e.empty(); }
auto sub fLongLocal(EL e) { string s = e; return s; }
auto sub fLongHashValueSize(hash<string, string> h) { return h.a.size(); }

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
run("string return", auto sub () { return fReturnString(e); });
run("*string return", auto sub () { return fReturnOptionalString(e); });
run("softstring return", auto sub () { return fReturnSoftString(e); });
run("number return", auto sub () { return fReturnNumber(n); });
run("string return of one of two values", auto sub () { return fReturnStringMixed(1, e); });
run("size() of a string return", auto sub () { return fReturnStringSize(e); });
run("strlen() of a string return", auto sub () { return fReturnStringStrlen(e); });
run("string local from a string return", auto sub () { return fReturnStringLocal(e); });
run("size() of a string return of one of two values", auto sub () { return fReturnStringMixedSize(e); });
run("method string return", auto sub () { return fMethodReturn(e); });
run("static method string return", auto sub () { return fStaticMethodReturn(e); });
run("closure string return", auto sub () { return fClosureReturn(e); });
run("hash value size()", auto sub () { return fHashValueSize({"a": e}); });
run("hash value empty()", auto sub () { return fHashValueEmpty({"a": e}); });
run("hash value val()", auto sub () { return fHashValueVal({"a": e}); });
run("hash local value size()", auto sub () { return fHashLocalSize(e); });
run("hash local value empty()", auto sub () { return fHashLocalEmpty(e); });
run("hashdecl member size()", auto sub () { return fHashDeclMemberSize(<HS>{"s": e}); });
run("hashdecl member empty()", auto sub () { return fHashDeclMemberEmpty(<HS>{"s": e}); });
run("hashdecl member val()", auto sub () { return fHashDeclMemberVal(<HS>{"s": e}); });
run("hashdecl member length()", auto sub () { return fHashDeclMemberLength(<HS>{"s": e}); });
run("hashdecl member long value size()", auto sub () { return fHashDeclMemberSize(<HS>{"s": EL::Long}); });
run("softint return", auto sub () { return fReturnSoftInt(ES::N); });
run("int return of an int enum", auto sub () { return fReturnInt(EI::Two); });
run("number return of an int enum", auto sub () { return fReturnIntAsNumber(EI::Two); });
run("enum return", auto sub () { return fReturnEnum(e); });
run("optional enum return", auto sub () { return fReturnOptionalEnum(e); });
run("method enum return", auto sub () { return fMethodReturnEnum(e); });
run("method optional enum return", auto sub () { return fMethodReturnOptionalEnum(e); });
run("closure enum return", auto sub () { return fClosureReturnEnum(e); });
run("concatenated hashdecl member length(), size(), find()", auto sub () { return fConcatMember(e); });
run("concatenated hashdecl member search with a parameter, append",
    auto sub () { return fConcatSearchMember(e, "y"); });
run("concatenated hashdecl member search with another parameter, append",
    auto sub () { return fConcatSearchMember(e, "q"); });
run("concatenated multi-byte hashdecl member length(), size(), find()",
    auto sub () { return fConcatMultiByteMember(EU::U); });
run("long value size()", auto sub () { return fLongSize(EL::Long); });
run("long value empty()", auto sub () { return fLongEmpty(EL::Long); });
run("long value string local", auto sub () { return fLongLocal(EL::Long); });
run("long value hash value size()", auto sub () { return fLongHashValueSize({"a": EL::Long}); });
