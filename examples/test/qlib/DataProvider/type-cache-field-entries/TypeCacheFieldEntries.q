# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The entries of the fields of a type in the process-wide data provider type cache are protected like the cache's
# other entries: sandboxed code cannot change them (issue #7674); see type-cache-field-entries.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires DataProvider

# a record type with a nested record type, registered by trusted code in the process-wide cache
HashDataType inner("inner");
inner.addField(new QoreDataField("x", "a nested field", AbstractDataProviderType::get(StringType)));
HashDataType rec("rec");
rec.addField(new QoreDataField("a", "a field", AbstractDataProviderType::get(StringType)));
rec.addField(new QoreDataField("b", "a field with fields", inner));
DataProvider::registerType("test-7674/rec", rec);

# the code of a tenant (a sandboxed Program)
const TenantCode = "
%modern
%requires DataProvider

string sub try_change(code c) {
    try {
        c();
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# returns the entry of a path in the process-wide cache
DataProviderTypeEntry sub entry(string path) {
    DataProviderTypeEntry e = DataProvider::getTypeRoot();
    foreach string node in (path.split(\"/\")) {
        e = e.getChild(node);
    }
    return e;
}

# the changes that a tenant can attempt on an entry
hash<string, code> sub changes() {
    return {
        \"setOrReplaceType()\": sub (DataProviderTypeEntry e) {
            e.setOrReplaceType(new HashDataType(\"tenant\"));
        },
        \"addChild()\": sub (DataProviderTypeEntry e) { e.addChild(\"tenant\"); },
        \"getCreateChild()\": sub (DataProviderTypeEntry e) { e.getCreateChild(\"tenant\"); },
        \"setType()\": sub (DataProviderTypeEntry e) { e.setType(new HashDataType(\"tenant\")); },
        \"removeChild()\": sub (DataProviderTypeEntry e) {
            *list<string> names = e.getChildNames();
            e.removeChild(names ? names[0] : \"none\");
        },
        \"lock()\": sub (DataProviderTypeEntry e) { e.lock(); },
        \"lockAll()\": sub (DataProviderTypeEntry e) { e.lockAll(); },
        \"clear()\": sub (DataProviderTypeEntry e) { e.clear(); },
    };
}

list<string> sub change_names() {
    return keys changes();
}

string sub try_one(string name, string path) {
    return try_change(sub () { changes(){name}(entry(path)); });
}

# the field entries of an entry that trusted code marks as shared after its type was set
string sub marked_later(DataProviderTypeEntry root) {
    return try_change(sub () { root.getChild(\"later\").getChild(\"b\").getChild(\"x\").setOrReplaceType(
        new HashDataType(\"tenant\")); });
}

# an entry that the tenant creates itself is its own
string sub own_entry() {
    DataProviderTypeEntry root();
    root.addChild(\"own\");
    HashDataType own(\"own\");
    own.addField(new QoreDataField(\"f\", \"a field\", AbstractDataProviderType::get(StringType)));
    root.getChild(\"own\").setType(own);
    return try_change(sub () { root.getChild(\"own\").getChild(\"f\").setOrReplaceType(new HashDataType(\"own2\")); })
        + \", \" + try_change(sub () { root.getChild(\"own\").setOrReplaceType(new HashDataType(\"own3\")); });
}
";

Program tenant(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
tenant.setSandboxManager(new SandboxManager());
tenant.parse(TenantCode, "tenant");

# the name of the type of a path in the process-wide cache, or NOTHING
*string sub type_name(string path) {
    *AbstractDataProviderType t = DataProvider::getType(path);
    return t ? t.getName() : NOTHING;
}

# the children of an entry of the process-wide cache, or NOTHING if there is no entry
*list<string> sub child_names(string path) {
    *DataProviderTypeEntry e = DataProvider::getTypeRoot();
    foreach string node in (path.split("/")) {
        e = e.getChild(node);
        if (!e) {
            return;
        }
    }
    return e.getChildNames();
}

# the state of the cache at the registered type: the types of its paths and the children of its entries
string sub state() {
    return sprintf("rec/a: %y, rec/b: %y, rec/b/x: %y, children: %y %y", type_name("test-7674/rec/a"),
        type_name("test-7674/rec/b"), type_name("test-7674/rec/b/x"), child_names("test-7674/rec"),
        child_names("test-7674/rec/b"));
}

string before = state();
printf("the cache: %s\n", before);
foreach string path in ("test-7674/rec", "test-7674/rec/a", "test-7674/rec/b", "test-7674/rec/b/x") {
    foreach string name in (tenant.callFunction("change_names")) {
        string rv = tenant.callFunction("try_one", name, path);
        string after = state();
        printf("%s %s: %s, the cache: %s\n", path, name, rv, after == before ? "unchanged" : "CHANGED: " + after);
    }
}
printf("the tenant's own entry: %s\n", tenant.callFunction("own_entry"));

# a tree whose field entries exist before trusted code marks it as shared
DataProviderTypeEntry later_root();
later_root.addChild("later");
later_root.getChild("later").setType(rec);
later_root.markShared();
printf("a field entry of a tree marked later: %s, type: %y\n", tenant.callFunction("marked_later", later_root),
    later_root.getChild("later").getChild("b").getChild("x").getType().getName());

# trusted code still changes the entries of the fields
DataProviderTypeEntry field = DataProvider::getTypeRoot().getChild("test-7674").getChild("rec").getChild("a");
AbstractDataProviderType orig = field.getType();
field.setOrReplaceType(new HashDataType("host"));
string changed = type_name("test-7674/rec/a");
field.setOrReplaceType(orig);
printf("trusted code replaces the type of a field: %y, restores it: %y\n", changed, type_name("test-7674/rec/a"));
