# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestSchemaActions module definition

/*  RestSchemaConvertedUnionDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace RestSchemaActions {
#! An action-facing union with a uniform conversion to its original wire contract
/** Branch types describe the values shown to callers, such as dates. The original union validates the
    converted wire value, preserving composition, discriminators, and other schema constraints. All
    members are immutable after construction.
*/
public class RestSchemaConvertedUnionDataType inherits QoreUnionDataType {
    private {
        QoreUnionDataType wire_type;
        hash<auto> conversion;
    }

    #! Creates a converted union
    /** @param wire_type the original schema-backed union
        @param types action-facing branch types
        @param conversion the common field conversion tree
    */
    constructor(QoreUnionDataType wire_type, list<AbstractDataProviderType> types,
            hash<auto> conversion) : QoreUnionDataType(types) {
        self.wire_type = wire_type;
        self.conversion = RestSchemaConvertedUnionDataType::nameCodecs(conversion);
    }

    #! Validates the action-facing value and its wire representation
    /** @param value the supplied action value
        @return the validated action value
        @throw RUNTIME-TYPE-ERROR the action value or its wire representation violates the contract
        @throw REST-SCHEMA-CODEC-ERROR a codec cannot convert the value
    */
    auto acceptsValue(auto value) {
        if (!exists value && isOrNothingType()) {
            return;
        }
        value = QoreUnionDataType::acceptsValue(value);
        value = RestSchemaActionOperation::convertValue(value, conversion, True);
        value = wire_type.acceptsValue(value);
        return RestSchemaActionOperation::convertValue(value, conversion, False);
    }

    #! Records codec identities instead of non-serializable closures
    /** A restored type resolves named codecs from the same registry used to build the action.
        Custom codecs must be registered before the restored type is used.
    */
    private static hash<auto> nameCodecs(hash<auto> node) {
        if (node.codec && node.codec.typeCode() != NT_STRING) {
            node.codec = node.codec.name;
        }
        foreach hash<auto> field in ((node.fields ?? {}).pairIterator()) {
            node.fields{field.key} = RestSchemaConvertedUnionDataType::nameCodecs(field.value);
        }
        return node;
    }

    #! Preserves wire validation when presentation replaces branch types
    /** @param types the replacement action-facing alternatives
        @return a union retaining this conversion and its original schema constraints
    */
    QoreUnionDataType copyWithUnionTypes(list<AbstractDataProviderType> types) {
        return new RestSchemaConvertedUnionDataType(wire_type, types, conversion);
    }

    #! Returns a "soft" type equivalent to the current type
    /** @return a type with the same conversion and wire contract over soft branches */
    AbstractDataProviderType getSoftType() {
        return new RestSchemaConvertedUnionDataType(wire_type,
            AbstractDataProviderType::getSoftTypesShared(getUnionTypes()), conversion);
    }

    #! Returns an "or nothing" type equivalent to the current type
    /** An optional option keeps its conversion and its schema-backed wire contract; a union that forgets
        them validates the action value alone, against no schema at all.

        @return a type with the same conversion and wire contract whose branches accept @ref nothing
    */
    AbstractDataProviderType getOrNothingType() {
        if (isOrNothingType()) {
            return self;
        }
        return new RestSchemaConvertedUnionDataType(wire_type, (map $1.getOrNothingType(), getUnionTypes()),
            conversion);
    }

    #! Returns a mandatory (i.e. not "or nothing") type equivalent to the current type
    /** @return a type with the same conversion and wire contract over mandatory branches */
    AbstractDataProviderType getMandatoryType() {
        if (!isOrNothingType()) {
            return self;
        }
        return new RestSchemaConvertedUnionDataType(wire_type, (map $1.getMandatoryType(), getUnionTypes()),
            conversion);
    }
}
}
