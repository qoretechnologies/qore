# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore HashDataType class definition

/** HashDataType.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DataProvider module
public namespace DataProvider {
#! describes a data type based on a hash
/** @note objects of this type are only compatible if their fields are compatible; non-matching fields of other types
    are automatically considered compatible; change the default "other field type" by calling
    setDefaultOtherFieldType() to change this; calling setDefaultOtherFieldType() with no argument removes
    compatibility with non-matching fields in other types.

    Additionally, adding any fields before calling setDefaultOtherFieldType() will also remove compatibility with
    unknown fields
*/
public class HashDataType inherits QoreDataType {
    private {
        #! Fields
        hash<string, AbstractDataField> fields;

        #! allow other fields
        *AbstractDataProviderType default_other_field_type =
            AbstractDataProviderType::get(AbstractDataProviderType::anyType);

        #! default other field type set mamnually?
        bool manual_default_other_field_type = False;

        #! if the type requires validation
        bool has_default_other_field_type = False;
    }

    #! creates the object and assigns the name as the type
    constructor(string name = AutoHashType.getName(), *hash<auto> options, *hash<auto> tags,
            *hash<auto> default_value, *hash<NameDescInfo> attr)
            : QoreDataType(AutoHashType, options, tags, default_value, attr) {
        self.name = name;
    }

    #! creates the object and assigns the name as the given name or the base type's name
    constructor(Type base_type, *string name, *hash<auto> options, *hash<auto> tags, *hash<auto> default_value,
            *hash<NameDescInfo> attr)
            : QoreDataType(base_type, options, tags, default_value, attr) {
        self.name = name ?? base_type.getName();
    }

    #! creates the object and assigns the name as the given name or the base type's name
    constructor(Type base_type, *string name, hash<string, AbstractDataField> fields, *hash<auto> options,
            *hash<auto> tags, *hash<auto> default_value, *hash<NameDescInfo> attr)
            : QoreDataType(base_type, options, tags, default_value, attr) {
        self.name = name ?? base_type.getName();
        self.fields = fields;
    }

    #! creates the object from the given record description and assigns the name as the type
    constructor(string name = AutoHashType.getName(), hash<string, AbstractDataField> fields, *hash<auto> options,
            *hash<auto> tags, *hash<auto> default_value, *hash<NameDescInfo> attr)
            : QoreDataType(AutoHashType, options, tags, default_value, attr) {
        self.name = name;
        self.fields = fields;
    }

    #! creates the object from the given TypedHash
    constructor(Qore::Reflection::TypedHash t, *hash<auto> options, *hash<auto> tags, *hash<auto> default_value,
            *hash<NameDescInfo> attr)
            : QoreDataType(AutoHashType, options, tags, default_value, attr) {
        name = t.getName();
        map addFieldIntern($1), t.getMembers();
    }

    #! Copy and set new fields in the copy
    HashDataType copyWithFields(hash<string, AbstractDataField> fields) {
        HashDataType rv = copy();
        rv.fields = fields;
        return rv;
    }

    #! Sets the default field type for unlisted fields
    setDefaultOtherFieldType(*AbstractDataProviderType default_other_field_type) {
        self.default_other_field_type = default_other_field_type;
        if (default_other_field_type) {
            has_default_other_field_type = default_other_field_type.hasType();
        } else {
            has_default_other_field_type = False;
        }
        manual_default_other_field_type = True;
    }

    #! Returns True if the type has a default field type for undeclared fields
    bool hasDefaultOtherFieldType() {
        return has_default_other_field_type;
    }

    #! Returns the default field type for undeclared fields, if any
    *AbstractDataProviderType getDefaultOtherFieldType() {
        return default_other_field_type;
    }

    #! Convert to an "or-nothing" type if all fields are optional
    checkConvertOrNothing() {
        if (type.isOrNothingType()) {
            return;
        }
        foreach AbstractDataField field in (fields.iterator()) {
            if (!field.getType().isOrNothingType() && !exists field.getDefaultValue()) {
                return;
            }
        }
        # convert to *hash<auto>
        type = type.getOrNothingType();
    }

    #! Returns the value if the value can be assigned to the type
    /** @param input_value the value to assign to the type

        @return the value to be assigned; can be converted by the type

        @throw RUNTIME-TYPE-ERROR value cannot be assigned to type
    */
    auto acceptsValue(auto input_value) {
        # specific check to provide a better error message in case of a missing argument
        on_error if (!exists input_value && fields && $1.err == "RUNTIME-TYPE-ERROR") {
            rethrow "MISSING-VALUE-ERROR", sprintf("no input value provided for hash with defined fields: %y",
                keys fields);
        }
        # {} + ensure that the hash value is "hash<auto>"
        auto value = {} + QoreDataType::acceptsValue(input_value);
        hash<auto> value_copy = value;
        # check fields if any are defined
        foreach AbstractDataField field in (fields.iterator()) {
            string key = field.getName();
            auto val = remove value_copy{key};
            if (!exists val) {
                if (exists (auto dv = field.getDefaultValue())) {
                    value{key} = dv;
                    continue;
                }
                if (!field.isMandatory()) {
                    continue;
                }
            }
            try {
                value{key} = field.acceptsValue(val);
            } catch (hash<ExceptionInfo> ex) {
                rethrow "RUNTIME-TYPE-ERROR", sprintf("error in type %y field %y: %s: %s", getName(), key, ex.err,
                    ex.desc);
            }
        }
        if (value_copy) {
            if (default_other_field_type) {
                if (has_default_other_field_type) {
                    foreach hash<auto> i in (value_copy.pairIterator()) {
                        try {
                            value{i.key} = default_other_field_type.acceptsValue(i.value);
                        } catch (hash<ExceptionInfo> ex) {
                            rethrow "RUNTIME-TYPE-ERROR", sprintf("error in type %y default field %y: %s: %s",
                                getName(), i.key, ex.err, ex.desc);
                        }
                    }
                } else {
                    value += value_copy;
                }
            } else if (fields) {
                throw "RUNTIME-TYPE-ERROR", sprintf("error in type %y unsupported fields in value: %y; known "
                    "fields: %y", getName(), keys value_copy, keys fields);
            }
        }
        return value;
    }

    #! Returns True if this type can be assigned from values of the argument type
    /** @note objects of this type are only compatible if their fields are compatible; if either side is a hash
        without declared fields, then they are compatible
    */
    bool isAssignableFrom(AbstractDataProviderType t) {
        *hash<string, AbstractDataField> other_fields = t.getFields();
        if (fields && other_fields) {
            foreach AbstractDataField field in (fields.iterator()) {
                string key = field.getName();
                *AbstractDataField other_field = remove other_fields{key};
                if (!other_field) {
                    if (!field.isMandatory()) {
                        continue;
                    } else {
                        return False;
                    }
                }
                if (!field.getType().isAssignableFrom(other_field.getType())) {
                    return False;
                }
            }
            if (other_fields) {
                if (!default_other_field_type) {
                    return False;
                }
                if (has_default_other_field_type) {
                    return True;
                }
                foreach AbstractDataField other_field in (other_fields.iterator()) {
                    if (!default_other_field_type.isAssignableFrom(other_field.getType())) {
                        return False;
                    }
                }
            }
        }
        return QoreDataType::isAssignableFrom(t);
    }

    #! Adds a set of fields from a hash
    /** @param new_fields a hash keyed by field name where values are hashes that can be processed by
        @ref QoreDataField::constructor(hash<auto>)

        @since DataProvider 3.0
    */
    HashDataType addQoreFields(hash<auto> new_fields) {
        if (!new_fields) {
            return self;
        }
        foreach hash<auto> i in (new_fields.pairIterator()) {
            if (fields{i.key}) {
                throw "ADD-QORE-FIELDS-ERROR", sprintf("field %y already exists in hash %y", i.key, name);
            }
            if (i.value.typeCode() != NT_HASH) {
                throw "ADD-QORE-FIELDS-ERROR", sprintf("field %y to be added has a value of type %y; expecting "
                    "\"hash\"", i.key, i.value.fullType());
            }
            if (!exists i.value.name) {
                i.value.name = i.key;
            }
            on_error rethrow $1.err, sprintf("%s (while adding field %y to type %y from hash: %y)", $1.desc, i.key,
                name, i.value);
            QoreDataField field(i.value);
            addFieldIntern(field);

            if (exists (auto fv = field.getDefaultValue())) {
                default_value{i.key} = fv;
            }
        }
        return self;
    }

    #! Adds a field to the type
    HashDataType addField(AbstractDataField field) {
        addFieldIntern(field);
        return self;
    }

    #! Adds a field to the type from a typed hash member
    HashDataType addField(Qore::Reflection::TypedHashMember mem) {
        addFieldIntern(mem);
        return self;
    }

    #! Returns the given field, if present, or @ref nothing if not
    *AbstractDataField getField(string name) {
        return fields{name};
    }

    #! Returns the fields of the data structure; if any
    *hash<string, AbstractDataField> getFields() {
        return fields;
    }

    #! Returns a "soft" type equivalent to the current type
    /** @return a "soft" type equivalent to the current type
    */
    AbstractDataProviderType getSoftType() {
        if (soft_type) {
            return self;
        }
        HashDataType rv(type, name, options, tags, default_value);
        if (manual_default_other_field_type && default_other_field_type) {
            rv.setDefaultOtherFieldType(default_other_field_type.getSoftType());
        }
        map rv.addField($1.getSoftType()), getFields().iterator();
        rv.soft_type = True;
        return rv;
    }

    #! returns a description of the type as a hash
    hash<DataTypeInfo> getInfo(*bool simple) {
        hash<DataTypeInfo> rv = QoreDataType::getInfo(simple);
        if (!fields && default_other_field_type) {
            rv.default_field_type_info = default_other_field_type.getInfo(simple);
        }
        return rv;
    }

    #! Returns an "or nothing" type equivalent to the current type as well as all fields
    /** @return an "or nothing" type equivalent to the current type as well as all fields
    */
    AbstractDataProviderType getDeepOrNothingType() {
        if (or_nothing_type) {
            return self;
        }
        # recursively return "or nothing" typed fields in any case
        HashDataType rv = QoreDataType::getOrNothingType();
        map rv.fields{$1.key} = $1.value.getOrNothingType(), fields.pairIterator();
        return rv;
    }

    #! Post process added fields
    /** do not allow unnamed fields if any fields are added manually and no default other field type has been set
        manually

        @since DataProvider 3.0
    */
    private postProcessAddedField(AbstractDataField field) {
        if (default_other_field_type && !manual_default_other_field_type) {
            remove default_other_field_type;
        }
        if (exists (auto val = field.getDefaultValue())) {
            if (type == AutoHashType) {
                type = AutoHashOrNothingType;
            }
            default_value{field.getName()} = val;
        }
    }

    #! adds a field to the type
    private addFieldIntern(AbstractDataField field) {
        fields{field.getName()} = field;
        postProcessAddedField(field);
    }

    #! adds a field to the type from a typed hash member
    private addFieldIntern(Qore::Reflection::TypedHashMember mem) {
        fields{mem.getName()} = new QoreDataField(mem);
        postProcessAddedField(fields{mem.getName()});
    }
}
}
