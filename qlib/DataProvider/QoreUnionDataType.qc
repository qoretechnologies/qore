# -*- mode: qore; indent-tabs-mode: nil -*-
#! QoreUnionDataType class definition

/*  QoreUnionDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! A data provider type accepting any one of several alternative types.
public class QoreUnionDataType inherits AbstractDataProviderType {
    private {
        #! Union type alternatives in declaration order
        list<AbstractDataProviderType> types = ();

        #! Normalized type name
        string name;
    }

    #! creates the type
    /** @param typenames list of type names making up the union, in matching order
        @throw TYPE-ERROR fewer than two alternatives were provided
        @since DataProvider 3.5.1
    */
    constructor(list<string> typenames) {
        if (typenames.size() < 2) {
            throw "TYPE-ERROR", sprintf("union types require at least two alternatives; got %y", typenames);
        }
        foreach string typename in (typenames) {
            types += AbstractDataProviderType::get(typename);
        }
        name = typenames.join("|");
    }

    #! creates the type
    /** @param types list of data provider type alternatives, in matching order
        @throw TYPE-ERROR fewer than two alternatives were provided
        @since DataProvider 3.5.1
    */
    constructor(list<AbstractDataProviderType> types) {
        if (types.size() < 2) {
            throw "TYPE-ERROR", sprintf("union types require at least two alternatives; got %y", types);
        }
        self.types = types;
        name = (map $1.getName(), types).join("|");
    }

    #! Returns the union alternatives
    /** @return the union alternatives in declaration order
        @since DataProvider 3.5.1
    */
    list<AbstractDataProviderType> getUnionTypes() {
        return types;
    }

    #! Returns the type name
    /** @return the normalized union type name
        @since DataProvider 3.5.1
    */
    string getName() {
        return name;
    }

    #! Returns the type's display name
    /** @return the display name generated from the union type name
        @since DataProvider 3.5.1
    */
    string getDisplayName() {
        return AbstractDataField::getDisplayName(getName());
    }

    #! Returns the type's short description
    /** @return a short description listing the accepted alternatives
        @since DataProvider 3.5.1
    */
    string getShortDescription() {
        return sprintf("Accepts one of: %s", (map $1.getName(), types).join(", "));
    }

    #! Returns the description
    /** @return a description listing the accepted alternatives
        @since DataProvider 3.5.1
    */
    string getDesc() {
        return sprintf("Union data type accepting one of: %s", (map $1.getName(), types).join(", "));
    }

    #! Returns a description of the type as a hash
    /** @param simple if @ref True, return simplified type names where supported by the alternatives
        @return a hash with a \c union_types key preserving alternative type metadata
        @since DataProvider 3.5.1
    */
    hash<DataTypeInfo> getInfo(*bool simple) {
        hash<DataTypeInfo> rv = AbstractDataProviderType::getInfo(simple);
        rv.union_types = map $1.getInfo(simple), types;
        return rv;
    }

    #! Returns the base type for the type, if any
    /** @return @ref NOTHING because a union has no single native Qore type
        @since DataProvider 3.5.1
    */
    *Type getValueType() {
        # A union has no single native Qore Type.
    }

    #! Returns the subtype (for lists or hashes) if there is only one
    /** @return the shared element type when every alternative has the same element type, otherwise @ref NOTHING
        @since DataProvider 3.5.1
    */
    *AbstractDataProviderType getElementType() {
        *AbstractDataProviderType rv;
        foreach AbstractDataProviderType type in (types) {
            *AbstractDataProviderType etype = type.getElementType();
            if (!etype) {
                return;
            }
            if (!rv) {
                rv = etype;
            } else if (rv.getName() != etype.getName()) {
                return;
            }
        }
        return rv;
    }

    #! Returns the fields of the data structure; if any
    /** @return @ref NOTHING because a union cannot expose one reliable field set
        @since DataProvider 3.5.1
    */
    *hash<string, AbstractDataField> getFields() {
        # A union cannot expose a single reliable field set.
    }

    #! Returns the base type name for the type; must be a standard Qore base type name
    /** @return the common non-null base type name, or \c all when alternatives have different base types
        @since DataProvider 3.5.1
    */
    string getBaseTypeName() {
        *string rv;
        foreach AbstractDataProviderType type in (types) {
            string base_type = type.getBaseTypeName();
            if (base_type == "nothing" || base_type == "null") {
                continue;
            }
            if (!rv) {
                rv = base_type;
            } else if (rv != base_type) {
                return "all";
            }
        }
        return rv ?? "all";
    }

    #! Returns the base type code for the type
    /** @return the common non-null base type code, or \c NT_ALL when alternatives have different base types
        @since DataProvider 3.5.1
    */
    int getBaseTypeCode() {
        *int rv;
        foreach AbstractDataProviderType type in (types) {
            foreach hash<auto> i in (type.getDirectTypeHash().pairIterator()) {
                int code = i.key.toInt();
                if (code == NT_NOTHING || code == NT_NULL) {
                    continue;
                }
                if (!exists rv) {
                    rv = code;
                } else if (rv != code) {
                    return NT_ALL;
                }
            }
        }
        return rv ?? NT_ALL;
    }

    #! Returns a hash of native base type code keys where no translations are performed; keys are type codes
    /** @return a merged hash of native base type codes accepted directly by any alternative
        @since DataProvider 3.5.1
    */
    *hash<string, bool> getDirectTypeHash() {
        hash<string, bool> rv;
        foreach AbstractDataProviderType type in (types) {
            rv += type.getDirectTypeHash();
        }
        return rv;
    }

    #! Returns True if this type is a list
    /** @return @ref True when all non-null alternatives share the list base type
        @since DataProvider 3.5.1
    */
    bool isList() {
        return getBaseTypeCode() == NT_LIST;
    }

    #! Returns True if the type must have a value
    /** @return @ref True if the union does not accept @ref nothing
        @since DataProvider 3.5.1
    */
    bool isMandatory() {
        return !isOrNothingType();
    }

    #! Returns True if the type also accepts @ref nothing
    /** @return @ref True if any alternative accepts @ref nothing
        @since DataProvider 3.5.1

        @note Each alternative is asked directly rather than looking for
        @ref Qore::NT_NOTHING "NT_NOTHING" in @ref getDirectTypeHash().  That hash reports the base type
        code of an alternative's *value* — \c "*string" contributes @ref Qore::NT_STRING "NT_STRING" and
        nothing else — so it can never show that an alternative is optional, and a union of optional
        alternatives reported itself as mandatory.
    */
    bool isOrNothingType() {
        foreach AbstractDataProviderType type in (types) {
            if (type.isOrNothingType()) {
                return True;
            }
        }
        return False;
    }

    #! Returns a hash of types accepted by this type; keys are type names
    /** @param simple if @ref True, return simplified type names where supported by the alternatives
        @return the merged accepted type-name hash for all alternatives
        @since DataProvider 3.5.1
    */
    hash<string, bool> getAcceptTypeHash(*bool simple) {
        hash<string, bool> rv;
        foreach AbstractDataProviderType type in (types) {
            rv += type.getAcceptTypeHash(simple);
        }
        return rv;
    }

    #! Returns a hash of types returned by this type; keys are type names
    /** @param simple if @ref True, return simplified type names where supported by the alternatives
        @return the merged returned type-name hash for all alternatives
        @since DataProvider 3.5.1
    */
    hash<string, bool> getReturnTypeHash(*bool simple) {
        hash<string, bool> rv;
        foreach AbstractDataProviderType type in (types) {
            rv += type.getReturnTypeHash(simple);
        }
        return rv;
    }

    #! Returns the value if the value can be assigned to any alternative type
    /** @param value value to validate
        @return the value accepted by the first matching alternative
        @throw RUNTIME-TYPE-ERROR no alternative accepts \a value
        @since DataProvider 3.5.1
    */
    auto acceptsValue(auto value) {
        *hash<ExceptionInfo> last_exception;
        foreach AbstractDataProviderType type in (types) {
            try {
                return type.acceptsValue(value);
            } catch (hash<ExceptionInfo> ex) {
                last_exception = ex;
            }
        }

        string value_type = exists value ? value.type() : "nothing";
        throw "RUNTIME-TYPE-ERROR", sprintf("value of type %y does not match union type %y%s",
            value_type, getName(), last_exception ? sprintf("; last error: %s", last_exception.desc) : "");
    }

    #! Returns a "soft" type equivalent to the current type
    /** @return a union whose alternatives are the soft equivalents of this union's alternatives
        @since DataProvider 3.5.1
    */
    AbstractDataProviderType getSoftType() {
        return new QoreUnionDataType(map $1.getSoftType(), types);
    }

    #! Returns an "or nothing" type equivalent to the current type
    /** @return a union whose alternatives accept @ref nothing
        @since DataProvider 3.5.1
    */
    AbstractDataProviderType getOrNothingType() {
        if (isOrNothingType()) {
            return self;
        }
        return new QoreUnionDataType(map $1.getOrNothingType(), types);
    }

    #! Returns a mandatory (i.e. not "or nothing") type equivalent to the current type
    /** @return a union whose alternatives are mandatory
        @since DataProvider 3.5.1
    */
    AbstractDataProviderType getMandatoryType() {
        if (!isOrNothingType()) {
            return self;
        }
        return new QoreUnionDataType(map $1.getMandatoryType(), types);
    }
}
}
