# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore HashDeclDataType class definition

/** HashDeclDataType.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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

# assume local scope for variables, do not use "$" signs
%modern
# require type definitions everywhere
#! strict argument handling
# enable all warnings

%requires reflection

#! contains all public definitions in the DataProvider module
public namespace DataProvider {
#! describes a data type based on a hashdecl
public class HashDeclDataType inherits QoreDataType {
    private {
        TypedHash hd;
    }

    #! creates the object from the TypedHash
    constructor(TypedHash hd, *hash<auto> options, *hash<auto> tags, *hash<auto> default_value,
            *hash<NameDescInfo> attr) : QoreDataType(hd.getType(), options, tags, default_value, attr) {
        self.hd = hd;
    }

    #! creates the object from a TypedHash Type
    private constructor(Type type, *hash<auto> options, *hash<auto> tags, *hash<auto> default_value,
            *hash<NameDescInfo> attr) : QoreDataType(type, options, tags, default_value, attr) {
        hd = type.getTypedHash();
    }

    #! Returns the value if the value can be assigned to the type
    /** @param value the value to assign to the type

        @return the value to be assigned; can be converted by the type

        @throw RUNTIME-TYPE-ERROR value cannot be assigned to type
    */
    auto acceptsValue(auto value) {
        return hd.doCast(value);
    }

    #! Returns True if this type can be assigned from values of the argument type
    /** @note objects of this type are only compatible if their fields are compatible; if either side is a hash
        without declared fields, then they are compatible
    */
    bool isAssignableFrom(AbstractDataProviderType t) {
        *hash<string, AbstractDataField> other_fields = t.getFields();
        *hash<string, AbstractDataField> self_fields = getFields();
        if (self_fields && other_fields) {
            foreach AbstractDataField field in (self_fields.iterator()) {
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
                return False;
            }
        }
        return QoreDataType::isAssignableFrom(t);
    }

    #! Returns the fields of the data structure; if any
    *hash<string, AbstractDataField> getFields() {
        string name = getName();
        return map {
            $1.getName(): new QoreDataField($1.getName(), sprintf("hashdecl %y member %y", name, $1.getName()),
                soft_type ? AbstractDataProviderType::get($1.getType()).getSoftType() : $1.getType(), $1.getDefaultValue()),
        }, hd.getMembers();
    }

    #! Returns the given field, if present, or @ref nothing if not
    *AbstractDataField getField(string name) {
        *TypedHashMember member;
        try {
            member = hd.getMember(name);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "UNKNOWN-MEMBER") {
                return;
            }
            rethrow;
        }
        return new QoreDataField(name, NOTHING, member.getType(), member.getDefaultValue());
    }

    #! Returns the TypedHash backing the type
    TypedHash getTypedHash() {
        return hd;
    }

    #! Returns a "soft" type equivalent to the current type
    /** @return a "soft" type equivalent to the current type
    */
    AbstractDataProviderType getSoftType() {
        HashDataType rv(getName(), options);
        map rv.addField($1.getSoftType()), getFields().iterator();
        return rv;
    }

    #! Returns an "or nothing" type equivalent to the current type
    /** @return an "or nothing" type equivalent to the current type
    */
    AbstractDataProviderType getOrNothingType() {
        HashDataType rv("*" + getName(), options);
        map rv.addField($1.getOrNothingType()), getFields().iterator();
        return rv;
    }

    #! Returns a mandatory (i.e. not "or nothing") type equivalent to the current type
    /** The base class method returns the same type; this method must be overridden in child classes to return a
        usable mandatory type

        @return a mandatory (i.e. not "or nothing") type equivalent to the current type
    */
    AbstractDataProviderType getMandatoryType() {
        string name = getName();
        if (name =~ /^\*/) {
            name = name[1..];
        }
        HashDataType rv(name, options);
        map rv.addField($1.getMandatoryType()), getFields().iterator();
        return rv;
    }
}
}
