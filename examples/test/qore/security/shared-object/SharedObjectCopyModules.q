# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The classes of the standard modules that guard their state with SandboxManager::isSharedObject(self) declare a
# copy() method that resets it: a tenant copies their shared objects, and its copy is not shared and has none of the
# state that the class guards for the object copied (qore#7667); see shared-object-copy.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires DataProvider
%requires OpenApi3
%requires Swagger
%requires reflection
%requires json

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern

# the copy, or the exception
auto sub copy_it(object o) {
    try {
        return o.copy();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# the result of a change, or its exception
string sub try_change(code c) {
    try {
        c();
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub add_field(HashDataType t) {
    return try_change(sub () { t.addField(new QoreDataField(\"tenant\", NOTHING, StringType)); });
}

string sub set_default(AbstractDataField f) {
    return try_change(sub () { f.setDefaultValue(\"tenant\"); });
}
";

Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
p.setSandboxManager(new SandboxManager());
p.loadModule("DataProvider");
p.parse(TenantCode, "tenant");

# the value of a member of an object, also a private one
auto sub member(object o, string cls, string name) {
    return cast<Reflection::NormalMember>(Reflection::Class::forName(cls).getMember(name)).getValue(o);
}

# the tenant's copy of the object
object sub tenant_copy(object o) {
    auto c = p.callFunction("copy_it", o);
    if (c.typeCode() != NT_OBJECT) {
        throw "COPY-REFUSED", sprintf("%s: %s", o.className(), c);
    }
    printf("%s: the tenant copies a shared one (shared: %y): its copy is shared: %y\n", o.className(),
        SandboxManager::isSharedObject(o), SandboxManager::isSharedObject(c));
    return c;
}

# DataProvider: AbstractDataProviderType and AbstractDataField
{
    HashDataType t();
    t.addField(new QoreDataField("a", NOTHING, StringType));
    t.freeze();
    HashDataType c = tenant_copy(t);
    printf("  the tenant adds a field: to the shared type: %s; to its copy: %s\n", p.callFunction("add_field", t),
        p.callFunction("add_field", c));
    printf("  the type's guarded state: frozen: %y\n", member(c, "AbstractDataProviderType", "frozen"));

    QoreDataField f("f", NOTHING, StringType);
    f.freeze();
    QoreDataField fc = tenant_copy(f);
    printf("  the tenant sets a default: of the shared field: %s; of its copy: %s\n",
        p.callFunction("set_default", f), p.callFunction("set_default", fc));
    printf("  the field's guarded state: shared_object: %y\n", member(fc, "AbstractDataField", "shared_object"));
}

# the REST schemas: AbstractRestSchemaValidator and AbstractRestSchemaObject, with the classes of OpenApi3 and Swagger
sub check_document(object doc, object so) {
    # the document and the schema object build their types
    so.getDataType();
    object dc = tenant_copy(doc);
    printf("  the document's type build lock is its own: %y, its type cache is empty: %y (the document's: %y)\n",
        member(dc, "AbstractRestSchemaValidator", "type_build_lock")
            != member(doc, "AbstractRestSchemaValidator", "type_build_lock"),
        !member(dc, doc.className(), "typemap"), !member(doc, doc.className(), "typemap"));
    object sc = tenant_copy(so);
    printf("  the schema object's guarded state: shared_object: %y\n",
        member(sc, "AbstractRestSchemaObject", "shared_object"));
}

{
    OpenApi3Schema doc = OpenApi3Loader::fromString(make_json({
        "openapi": "3.0.0", "info": {"title": "t", "version": "1"}, "paths": {},
        "components": {"schemas": {
            "Thing": {"type": "object", "properties": {"name": {"type": "string"}}},
        }},
    }), True);
    check_document(doc, doc.components.schemas.Thing);
}

{
    SwaggerSchema doc = SwaggerLoader::fromString(make_json({
        "swagger": "2.0", "info": {"title": "t", "version": "1"}, "paths": {},
        "definitions": {
            "Thing": {"type": "object", "properties": {"name": {"type": "string"}}},
        },
    }), True);
    check_document(doc, doc.definitions.Thing);
}
