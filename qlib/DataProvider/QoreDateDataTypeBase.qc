# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreDateDataTypeBase class definition

/** QoreDateDataTypeBase.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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
#! describes a data type based on a date type with validation for string parsing and time zone support
/** @note %Qore "soft" types that do not accept @ref nothing also accept @ref null; to ensure that soft types
    inheriting this class do not accept @ref null, use the \c qore.no_null option in the constructor
*/
public class QoreDateDataTypeBase inherits QoreDataType {
    private {
        #! supported options
        const hash<string, hash<DataProviderTypeOptionInfo>> SupportedOptions = {
            "date.output_timezone": <DataProviderTypeOptionInfo>{
                "type": "object<TimeZone>",
                "desc": "the time zone to use when writing to the type",
            },
        };

        #! supported soft options
        const hash<string, hash<DataProviderTypeOptionInfo>> SupportedSoftOptions = {
            "date.format": <DataProviderTypeOptionInfo>{
                "type": Type::String,
                "desc": "the format string for converting strings to dates",
            },
            "date.input_timezone": <DataProviderTypeOptionInfo>{
                "type": "object<TimeZone>",
                "desc": "the time zone for external data when writing to the type and converting strings to dates",
            },
        };
    }

    #! creates the object
    /** @throw TYPE-ERROR invalid type; must be a date type
    */
    private constructor(Type type, *hash<auto> options, *hash<auto> tags) : QoreDataType(type, options, tags) {
        if (type.getBaseTypeCode() != NT_DATE) {
            throw "TYPE-ERROR", sprintf("cannot create a %s object from type %y", self.className(),
                type.getName());
        }
        # translations are performed on date types if there is an output_timezone
        if (options.output_timezone) {
            remove base_type_hash{NT_DATE};
        }
        # set the input timezone option unconditionally
        if (!options."date.input_timezone") {
            self.options."date.input_timezone" = TimeZone::get();
        }
    }

    #! returns the value if the value can be assigned to the type
    /** @param value the value to assign to the type

        @return the value to be assigned; can be converted by the type

        @throw RUNTIME-TYPE-ERROR value cannot be assigned to type
    */
    auto acceptsValue(auto value) {
        if (exists value && value !== NULL) {
            # try to convert from the source type
            if (value.typeCode() !== NT_DATE) {
                # only convert if the type can be converted
                if (!soft_type || !accept_type_hash_simple{value.type()}) {
                    throw "RUNTIME-TYPE-ERROR", sprintf("type %y cannot be assigned from a value of type %y",
                        getName(), value.type());
                }
                value = options."date.input_timezone".date(value, options."date.format");
            }

            # convert to the output TimeZone if necessary
            if (options."date.output_timezone") {
                value = options."date.output_timezone".date(value);
            }
            return value;
        }
        return QoreDataType::acceptsValue(value);
    }

    #! returns supported options
    *hash<string, hash<DataProviderTypeOptionInfo>> getSupportedOptions() {
        return QoreDataType::getSupportedOptions() + SupportedOptions + (soft_type ? SupportedSoftOptions : NOTHING);
    }

    #! returns a "soft" type equivalent to the current type
    /** @return a "soft" type equivalent to the current type
    */
    AbstractDataProviderType getSoftType() {
        if (soft_type) {
            return self;
        }
        return new QoreDateDataTypeBase(base_type_hash{NT_NOTHING} ? SoftDateOrNothingType : SoftDateType, getOptions());
    }
}
}
