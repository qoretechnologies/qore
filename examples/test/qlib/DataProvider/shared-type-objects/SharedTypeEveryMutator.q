# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# Every method that changes a data type or field refuses sandboxed code for one that every Program of the process
# shares, which keeps its state, and changes one of the tenant's own (qore#6843); see shared-type-every-mutator.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires DataProvider

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern
%requires DataProvider

# the objects that the changes change, with the given creator's code
hash<auto> sub make_objects() {
    DataProvider::HashDataType h(\"h\");
    h.addField(new DataProvider::QoreDataField(\"a\", \"a\", Reflection::StringOrNothingType));
    h.addField(new DataProvider::QoreDataField(\"b\", \"b\", Reflection::StringOrNothingType));
    h.setPath(\"orig/path\");
    return {
        \"hash\": h,
        \"list\": new DataProvider::ListDataType(Reflection::StringType),
        \"union\": new DataProvider::QoreUnionDataType((DataProvider::AbstractDataProviderType::get(
            Reflection::StringType), DataProvider::AbstractDataProviderType::get(Reflection::IntType))),
        \"field\": new DataProvider::QoreDataField(\"f\", \"f\", new DataProvider::QoreDataType(Reflection::StringType)),
        \"list_field\": new DataProvider::QoreDataField(\"lf\", \"lf\",
            new DataProvider::ListDataType(Reflection::StringType)),
    };
}

# the state of the given objects that the changes change
string sub snapshot(hash<auto> o) {
    DataProvider::HashDataType h = o.hash;
    DataProvider::QoreDataField f = o.field;
    list<string> l = (
        sprintf(\"path: %y\", h.getPath()),
        sprintf(\"schema identity: %y\", h.getSchemaIdentity()),
        sprintf(\"tags: %y\", h.getTags()),
        sprintf(\"options: %y\", h.getOptions()),
        sprintf(\"fields: %y\", (map sprintf(\"%s:%s\", $1.key, $1.value.getType().getName()),
            h.getFields().pairIterator())),
        sprintf(\"default other field type: %y\", h.hasDefaultOtherFieldType()
            ? h.getDefaultOtherFieldType().getName() : NOTHING),
        sprintf(\"required groups: %y\", h.getRequiredGroups()),
        sprintf(\"or nothing: %y\", h.isOrNothingType()),
        sprintf(\"list element type: %y\", o.list.getElementType().getName()),
        sprintf(\"union: %y\", o.union.getName()),
        sprintf(\"field type: %y\", f.getType().getName()),
        sprintf(\"field options: %y\", f.getOptions()),
        sprintf(\"field allowed values: %y\", f.getAllowedValues()),
        sprintf(\"field default value: %y\", f.getDefaultValue()),
        sprintf(\"field remote default value: %y\", f.getRemoteDefaultValue()),
        sprintf(\"field attributes: %y\", f.getAttributes()),
        sprintf(\"field description: %y\", f.getDescription()),
        sprintf(\"list field element allowed values: %y\", o.list_field.getElementAllowedValues()),
    );
    return join(\"\\n\", l);
}

# the changes, by method
hash<string, code> sub changes() {
    return {
        \"AbstractDataProviderType::setPath()\": sub (hash<auto> o) { o.hash.setPath(\"x\"); },
        \"AbstractDataProviderType::clearPath()\": sub (hash<auto> o) { o.hash.clearPath(); },
        \"AbstractDataProviderType::setSchemaIdentity()\": sub (hash<auto> o) {
            o.hash.setSchemaIdentity(\"#/injected\");
        },
        \"AbstractDataProviderType::setDynamicTag()\": sub (hash<auto> o) { o.hash.setDynamicTag(); },
        \"AbstractDataProviderType::setOption()\": sub (hash<auto> o) { o.hash.setOption(\"qore.no_null\", True); },
        \"AbstractDataProviderType::setOptions()\": sub (hash<auto> o) {
            o.hash.setOptions({\"qore.no_null\": True});
        },
        \"HashDataType::addField(AbstractDataField)\": sub (hash<auto> o) {
            o.hash.addField(new DataProvider::QoreDataField(\"c\", \"c\", Reflection::StringOrNothingType));
        },
        # for the tenant's own type, the field's default value is the initializer of a member of a typed hash of the
        # trusted Program, which sandboxed code cannot evaluate through reflection (REFLECTION-ACCESS-ERROR)
        \"HashDataType::addField(TypedHashMember)\": sub (hash<auto> o) {
            o.hash.addField(o.member);
        },
        \"HashDataType::addQoreFields()\": sub (hash<auto> o) {
            o.hash.addQoreFields({\"c\": {\"type\": Reflection::StringOrNothingType}});
        },
        \"HashDataType::setFields()\": sub (hash<auto> o) {
            o.hash.setFields({\"c\": new DataProvider::QoreDataField(\"c\", \"c\", Reflection::StringOrNothingType)});
        },
        \"HashDataType::setFieldList()\": sub (hash<auto> o) {
            o.hash.setFieldList((new DataProvider::QoreDataField(\"c\", \"c\", Reflection::StringOrNothingType),));
        },
        \"HashDataType::replaceField()\": sub (hash<auto> o) {
            o.hash.replaceField(new DataProvider::QoreDataField(\"a\", \"a\", Reflection::IntOrNothingType));
        },
        \"HashDataType::addOrReplaceField()\": sub (hash<auto> o) {
            o.hash.addOrReplaceField(new DataProvider::QoreDataField(\"b\", \"b\", Reflection::IntOrNothingType));
        },
        \"HashDataType::setDefaultOtherFieldType()\": sub (hash<auto> o) {
            o.hash.setDefaultOtherFieldType(DataProvider::AbstractDataProviderType::get(Reflection::StringType));
        },
        \"HashDataType::setRequiredGroups()\": sub (hash<auto> o) { o.hash.setRequiredGroups(((\"a\", \"b\"),)); },
        \"HashDataType::checkConvertOrNothing()\": sub (hash<auto> o) { o.hash.checkConvertOrNothing(); },
        \"ListDataType::updateElementType()\": sub (hash<auto> o) {
            o.list.updateElementType(DataProvider::AbstractDataProviderType::get(Reflection::IntType));
        },
        \"QoreUnionDataType::setUnionTypes()\": sub (hash<auto> o) {
            o.union.setUnionTypes((DataProvider::AbstractDataProviderType::get(Reflection::IntType),
                DataProvider::AbstractDataProviderType::get(Reflection::FloatType)));
        },
        \"AbstractDataField::setOption()\": sub (hash<auto> o) { o.field.setOption(\"qore.no_null\", True); },
        \"AbstractDataField::setOptions()\": sub (hash<auto> o) { o.field.setOptions({\"qore.no_null\": True}); },
        \"AbstractDataField::setAllowedValues()\": sub (hash<auto> o) { o.field.setAllowedValues((\"x\", \"y\")); },
        \"AbstractDataField::setElementAllowedValues()\": sub (hash<auto> o) {
            o.list_field.setElementAllowedValues((\"x\", \"y\"));
        },
        \"AbstractDataField::setDefaultValue()\": sub (hash<auto> o) { o.field.setDefaultValue(\"injected\"); },
        \"AbstractDataField::setRemoteDefaultValue()\": sub (hash<auto> o) {
            o.field.setRemoteDefaultValue(\"injected\");
        },
        \"AbstractDataField::setAttributes()\": sub (hash<auto> o) { o.field.setAttributes({\"injected\": True}); },
        \"AbstractDataField::replaceAttributes()\": sub (hash<auto> o) {
            o.field.replaceAttributes({\"injected\": True});
        },
        \"QoreDataField::appendDescription()\": sub (hash<auto> o) { o.field.appendDescription(\" \", \"injected\"); },
        \"QoreDataField::updateFieldType()\": sub (hash<auto> o) {
            o.field.updateFieldType(DataProvider::AbstractDataProviderType::get(Reflection::IntType));
        },
    };
}

# the names of the changes
list<string> sub change_names() {
    return keys changes();
}

# the result of the given change of the given objects, or its exception
string sub try_change(string name, hash<auto> o) {
    try {
        changes(){name}(o);
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# the given change of objects of the tenant's own: its result, and whether it changed their state; the member of a
# typed hash comes from trusted code, as sandboxed code cannot use reflection
string sub try_own_change(string name, Reflection::TypedHashMember member) {
    hash<auto> o = make_objects() + {\"member\": member};
    string before = snapshot(o);
    string rv = try_change(name, o);
    return sprintf(\"%s, %s\", rv, snapshot(o) == before ? \"state kept\" : \"state changed\");
}
";

Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
p.setSandboxManager(new SandboxManager());
p.parse(TenantCode, "tenant");

# the member of a typed hash that the change by HashDataType::addField(TypedHashMember) adds
hashdecl AddedMemberDecl {
    string added_member;
}
TypedHashMember member = (new TypedHash("AddedMemberDecl")).getMember("added_member");

# trusted code (no sandbox) creates objects that every Program of the process can share, with the same code as the
# tenant's
Program trusted(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
trusted.parse(TenantCode, "trusted");

foreach string name in (p.callFunction("change_names")) {
    hash<auto> shared = trusted.callFunction("make_objects") + {"member": member};
    string before = trusted.callFunction("snapshot", shared);
    string rv = p.callFunction("try_change", name, shared);
    string after = trusted.callFunction("snapshot", shared);
    printf("%s: shared: %s, %s; own: %s\n", name, rv, after == before ? "state kept" : "state changed",
        p.callFunction("try_own_change", name, member));
}
