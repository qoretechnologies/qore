# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A class that a tenant declares and that inherits a data type or field class of the library cannot use the private
# state and methods of the library's classes, on the types and fields that every Program of the process shares or on
# its own objects; it can implement the type it declares with the public API (qore#6843); see
# shared-type-subclass.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires DataProvider

# the types and fields that every Program shares: created by trusted code
HashDataType shared_hash("shared");
shared_hash.addField(new QoreDataField("a", "a", AbstractDataProviderTypeMap."string"));
QoreDataField shared_field("f", "f", AbstractDataProviderTypeMap."string");
ListDataType shared_list("shared_list", AbstractDataProviderTypeMap."string");

const Opts = PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES | PO_STRONG_ENCAPSULATION;

#! returns a new tenant Program with the code parsed, or the parse error
*Program sub tenant(string label, string code) {
    Program p(Opts);
    p.setSandboxManager(new SandboxManager());
    p.loadModule("DataProvider");
    try {
        p.parse(code, label);
        return p;
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: parse: %s: %s\n", label, ex.err, ex.desc);
    }
}

const ClassHead = "class T inherits DataProvider::HashDataType { constructor() : DataProvider::HashDataType(\"t\") {} ";

# the static forms: refused at parse time
const ParseCases = (
    "fields": ClassHead + "static f(DataProvider::HashDataType h) { h.fields.x = NOTHING; } }",
    "shared_object": ClassHead + "static f(DataProvider::HashDataType h) { h.shared_object = False; } }",
    "self-fields": ClassHead + "f() { self.fields = {}; } }",
    "addFieldIntern": ClassHead + "static f(DataProvider::HashDataType h, DataProvider::AbstractDataField fld) {
        h.addFieldIntern(fld); } }",
    "self-addFieldIntern": ClassHead + "f(DataProvider::AbstractDataField fld) { addFieldIntern(fld); } }",
    "setOptionIntern": ClassHead + "static f(DataProvider::HashDataType h) { h.setOptionIntern(\"x\", 1); } }",
    "field-name": "class F inherits DataProvider::QoreDataField {
        constructor() : DataProvider::QoreDataField(\"t\", \"t\", DataProvider::AbstractDataProviderTypeMap.\"string\")
        {} static f(DataProvider::QoreDataField fld) { fld.name = \"x\"; } }",
    "setElementTypeIntern": "class L inherits DataProvider::ListDataType {
        constructor() : DataProvider::ListDataType(\"l\", DataProvider::AbstractDataProviderTypeMap.\"string\") {}
        static f(DataProvider::ListDataType l) {
            l.setElementTypeIntern(DataProvider::AbstractDataProviderTypeMap.\"int\"); } }",
);

foreach hash<auto> i in (ParseCases.pairIterator()) {
    if (tenant(i.key, i.value)) {
        printf("%s: parse: OK\n", i.key);
    }
}

# the dynamic forms, refused at run time, and a type that the tenant implements with the public API
const TenantCode = "
class T inherits DataProvider::HashDataType {
    constructor() : DataProvider::HashDataType(\"t\") {
        # the public API of the base class
        addField(new DataProvider::QoreDataField(\"own\", \"own\", DataProvider::AbstractDataProviderTypeMap.\"string\"));
    }

    static auto fields(auto h) {
        h.fields.x = new DataProvider::QoreDataField(\"x\", \"x\", DataProvider::AbstractDataProviderTypeMap.\"string\");
        return keys h.getFields();
    }
    static auto sharedFlag(auto h) { h.shared_object = False; return \"cleared\"; }
    static auto computed(auto h) { string n = \"fie\" + \"lds\"; return keys h.(n); }
    static auto addFieldIntern(auto h) {
        h.addFieldIntern(new DataProvider::QoreDataField(\"y\", \"y\", DataProvider::AbstractDataProviderTypeMap.\"string\"));
        return keys h.getFields();
    }
    static auto setOptionIntern(auto h) { h.setOptionIntern(\"x\", 1); return \"set\"; }

    auto selfFields() { auto o = self; return keys o.fields; }
    auto ownFields() { return keys getFields(); }
}

class F inherits DataProvider::QoreDataField {
    constructor() : DataProvider::QoreDataField(\"t\", \"t\", DataProvider::AbstractDataProviderTypeMap.\"string\") {}
    static auto fieldName(auto fld) { fld.name = \"renamed\"; return fld.getName(); }
}

class L inherits DataProvider::ListDataType {
    constructor() : DataProvider::ListDataType(\"l\", DataProvider::AbstractDataProviderTypeMap.\"string\") {}
    static auto listElement(auto l) {
        l.setElementTypeIntern(DataProvider::AbstractDataProviderTypeMap.\"int\");
        return l.getElementType().getName();
    }
}

auto sub run(string cls, string m, auto arg) {
    return call_static_method(cls, m, arg);
}

auto sub runSelf(string m) {
    return call_object_method(new T(), m);
}
";

*Program p = tenant("tenant", TenantCode);
if (!p) {
    exit(1);
}

sub show(string label, code c) {
    try {
        printf("%s: OK %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

foreach string m in (("fields", "sharedFlag", "computed", "addFieldIntern", "setOptionIntern")) {
    show(m, auto sub () { return p.callFunction("run", "T", m, shared_hash); });
}
show("fieldName", auto sub () { return p.callFunction("run", "F", "fieldName", shared_field); });
show("listElement", auto sub () { return p.callFunction("run", "L", "listElement", shared_list); });
foreach string m in (("selfFields", "ownFields")) {
    show(m, auto sub () { return p.callFunction("runSelf", m); });
}

printf("shared after: %y %y %y\n", keys shared_hash.getFields(), shared_field.getName(),
    shared_list.getElementType().getName());
