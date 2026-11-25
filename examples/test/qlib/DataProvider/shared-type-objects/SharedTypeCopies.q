# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A copy of a data type or field belongs to the code that makes it: a tenant can change its copies of a type or a
# field that every Program of the process shares, and library code that copies a shared type and changes the copy for
# a tenant works; the shared originals are unchanged (qore#6843); see shared-type-copies.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires DataProvider

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern
%requires DataProvider

# the result of a change, or its exception
string sub try_change(code c) {
    try {
        c();
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

DataProvider::QoreDataField sub new_field(string name) {
    return new DataProvider::QoreDataField(name, name, Reflection::StringOrNothingType);
}

string sub add_field(DataProvider::HashDataType type) {
    return try_change(sub () { type.addField(new_field(\"added\")); });
}

string sub change_copy(DataProvider::HashDataType shared) {
    return try_change(sub () { shared.copy().addField(new_field(\"copy\")); });
}

string sub change_copy_with_tags(DataProvider::HashDataType shared) {
    return try_change(sub () {
        cast<DataProvider::HashDataType>(shared.copyWithTags({\"t\": True})).addField(new_field(\"tags\"));
    });
}

string sub change_copy_with_fields(DataProvider::HashDataType shared) {
    return try_change(sub () {
        shared.copyWithFields(shared.getFields()).addField(new_field(\"fields\"));
    });
}

string sub change_or_nothing_type(DataProvider::QoreDataType shared) {
    return try_change(sub () { shared.getOrNothingType().setPath(\"tenant/copy\"); });
}

string sub change_field_copy(DataProvider::QoreDataField shared) {
    return try_change(sub () { shared.copy().setDefaultValue(\"copy\"); });
}

string sub change_renamed_field(DataProvider::QoreDataField shared) {
    return try_change(sub () { shared.getRenamedField(\"renamed\").setDefaultValue(\"renamed\"); });
}

# a pipeline step that removes a field of a nested hash type copies the nested type and the field that holds it
string sub remove_nested_field(DataProvider::HashDataType shared) {
    try {
        DataProvider::QoreRemoveFieldsProcessor proc({\"fields\": \"nested.x\"});
        *DataProvider::AbstractDataProviderType out = proc.getPipelineProcessorOutputTypeWithInputType(shared);
        return sprintf(\"nested fields: %s\", join(\" \", keys out.getFields().nested.getType().getFields()));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
";

string sub field_names(AbstractDataProviderType type) {
    return join(" ", keys type.getFields());
}

Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
p.setSandboxManager(new SandboxManager());
p.parse(TenantCode, "tenant");

# a type and a field that code no sandbox governs creates, which every Program of the process can share
HashDataType inner("inner");
inner.addField(new QoreDataField("x", "x", StringOrNothingType));
inner.addField(new QoreDataField("y", "y", StringOrNothingType));
HashDataType shared("shared");
QoreDataField shared_field("a", "a", StringOrNothingType);
shared.addField(shared_field);
shared.addField(new QoreDataField("nested", "nested", inner));
QoreDataType shared_simple(StringType);

printf("the tenant changes its copy of a shared type: %s\n", p.callFunction("change_copy", shared));
printf("the tenant changes its copy of a shared type with other tags: %s\n",
    p.callFunction("change_copy_with_tags", shared));
printf("the tenant changes its copy of a shared type with other fields: %s\n",
    p.callFunction("change_copy_with_fields", shared));
printf("the tenant changes its \"or nothing\" copy of a shared type: %s\n",
    p.callFunction("change_or_nothing_type", shared_simple));
printf("the tenant changes its copy of a shared field: %s\n", p.callFunction("change_field_copy", shared_field));
printf("the tenant changes its renamed copy of a shared field: %s\n",
    p.callFunction("change_renamed_field", shared_field));
printf("a pipeline step of the tenant removes a nested field of a shared type: %s\n",
    p.callFunction("remove_nested_field", shared));

# the shared originals are unchanged
printf("the shared type: %s\n", field_names(shared));
printf("its nested type: %s\n", field_names(inner));
printf("the shared field's default value: %y\n", shared_field.getDefaultValue());
printf("the shared simple type's options: %y\n", shared_simple.getOptions());

# a copy that code no sandbox governs makes is shared, as a type that such code creates
HashDataType trusted_copy = shared.copy();
printf("the tenant changes a copy that trusted code made: %s\n", p.callFunction("add_field", trusted_copy));
