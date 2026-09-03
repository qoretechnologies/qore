# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreStringDataTypeBase class definition

/** QoreStringDataTypeBase.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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
#! describes a data type based on a string tyoe with a target encoding option
/** @note %Qore "soft" types that do not accept @ref nothing also accept @ref null; to ensure that soft types
    inheriting this class do not accept @ref null, use the \c qore.no_null option in the constructor
*/
public class QoreStringDataTypeBase inherits QoreDataType {
    private {
        #! supported options
        const SupportedOptions = QoreDataType::SupportedOptions + {
            "string.encoding": <DataProviderTypeOptionInfo>{
                "type": Type::String,
                "desc": "the output encoding when writing to the type",
            },
            # the max_size_chars option is to be enforced externally
            "string.max_size_chars": <DataProviderTypeOptionInfo>{
                "type": Type::Int,
                "desc": "the maximum length of the string in characters",
            },
        };

        #! supported "or nothing" options
        const hash<auto> SupportedOrNothingOptions = {
            # only used with "or nothing" types
            "string.empty_to_nothing": <DataProviderTypeOptionInfo>{
                "type": Type::Boolean,
                "desc": "if an empty string should be converted to no value",
            },
        };

        #! whether the wrapped type implements its own value conversion
        /** A user-defined @ref Qore::Reflection::Type "Type" subclass overrides
            @ref Qore::Reflection::Type::acceptsValue() "Type::acceptsValue()" to implement conversion semantics
            that the generic string conversion in this class does not know about; ex: the \c SoftBool*StringType
            classes used by REST data providers to render a boolean as the string \c "true" or \c "false" for a
            query argument
        */
        bool custom_type_conversion;
    }

    #! creates the object with the given options
    /** @throw TYPE-ERROR invalid type; must be a string type
    */
    constructor(Type type, *hash<auto> options, *hash<auto> tags, *hash<auto> default_value,
            *hash<NameDescInfo> attr) : QoreDataType(type, options, tags, default_value, attr) {
        if (type.getBaseTypeCode() != NT_STRING) {
            throw "TYPE-ERROR", sprintf("cannot create a %s object from type %y", self.className(),
                type.getName());
        }

        # translations are done on string types if there are options that require string processing
        if (options."string.encoding" || options."string.empty_to_nothing") {
            remove base_type_hash{NT_STRING};
        }

        custom_type_conversion = QoreStringDataTypeBase::hasCustomConversion(type);
    }

    #! Returns @ref True "True" if the given type overrides @ref Qore::Reflection::Type::acceptsValue()
    /** @param type the wrapped type to check

        @return @ref True "True" if a class other than the base @ref Qore::Reflection::Type "Type" class declares
        \c acceptsValue(), meaning that the type implements its own value conversion
    */
    private:internal static bool hasCustomConversion(Type type) {
        # only a user-defined subclass can override the method; this check keeps the common case cheap
        if (get_class_name(type) == "Type") {
            return False;
        }
        foreach AbstractMethod m in (Class::getClass(type).getMethods()) {
            if (m.getName() == "acceptsValue" && m.getClass().getName() != "Type") {
                return True;
            }
        }
        return False;
    }

    #! returns the value if the value can be assigned to the type
    /** @param value the value to assign to the type

        @return the value to be assigned; can be converted by the type

        @throw RUNTIME-TYPE-ERROR value cannot be assigned to type
    */
    auto acceptsValue(auto value) {
        # the wrapped type defines its own conversion; the generic string conversion below would silently
        # discard it and produce, for example, "1" instead of "true" for a boolean rendered as a string
        if (custom_type_conversion) {
            return QoreDataType::acceptsValue(value);
        }
        if (_flexible_type_handling && exists value && value.typeCode() != NT_STRING) {
            return SuperSoftStringDataType::getStringValue(value);
        }
        if (exists value && value !== NULL) {
            if (value.typeCode() != NT_STRING) {
                if (!accept_type_hash_simple{value.type()}) {
                    throw "RUNTIME-TYPE-ERROR", sprintf("value of type %y cannot be converted to type %y (val: %y)",
                        value.type(), getName(), value);
                }
                value = string(value, options."string.encoding");
            } else if (options."string.encoding" && (value.encoding() != options."string.encoding")) {
                value = string(value, options."string.encoding");
            }
            if (options."string.empty_to_nothing" && !value.val() && return_type_hash."nothing") {
                return;
            }
            return value;
        }
        return QoreDataType::acceptsValue(value);
    }

    #! returns supported options
    *hash<string, hash<DataProviderTypeOptionInfo>> getSupportedOptions() {
        return QoreDataType::getSupportedOptions() + SupportedOptions
            + (or_nothing_type ? SupportedOrNothingOptions : NOTHING);
    }

    #! returns a "soft" type equivalent to the current type
    /** @return a "soft" type equivalent to the current type
    */
    AbstractDataProviderType getSoftType() {
        if (soft_type) {
            return self;
        }
        return new QoreStringDataTypeBase(base_type_hash{NT_NOTHING} ? SoftStringOrNothingType : SoftStringType,
            getOptions());
    }

    #! sets the given option without any validation of the option
    private setOptionInternal(string opt, auto value) {
        if (opt == "string.encoding") {
            # make sure the encoding is recognized and is the internal target encoding
            options."string.encoding" = string("", options."string.encoding").encoding();
        } else {
            options{opt} = value;
        }
    }
}
}
