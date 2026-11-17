# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FixedLengthBase.qc Qore user module for working with files with fixed lines length

/*  FixedLengthBase Copyright 2015 - 2024 Qore Technologies, s.r.o.

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

#! The FixedLengthUtil namespace contains all the definitions in the FixedLengthUtil module
public namespace FixedLengthUtil {
#! Base class for fixed-length readers and writers
public class FixedLengthBase {
    private {
        #! The record specifications
        hash<auto> m_specs;

        #! Global input or output options
        *hash<auto> m_opts; # encoding, eol, ignore_empty, timezone, etc

        #! Translates from global options to data provider options
        const hash<string, string> GlobalOptionMap = {
            "date_format": "date.format",
            "number_format": "number.format",
            "timezone": "date.input_timezone",
            "encoding": "string.encoding",
        };
    }

    #! Creates the object from the record specifications
    constructor(hash<auto> m_specs, *hash<auto> m_opts) {
        self.m_specs = m_specs;
        self.m_opts = m_opts;
    }

    #! Returns the description of the record type, if any
    *hash<string, AbstractDataField> getRecordType() {
        return {
            "type": new QoreDataField("type", NOTHING, StringType),
            "record": new QoreDataField("record", NOTHING, AutoHashType),
        };
        /*
        *hash<string, AbstractDataField> rv;
        # global options
        *hash<auto> global_options = map {$1.value: m_opts{$1.key}}, GlobalOptionMap.pairIterator(),
            exists m_opts{$1.key};
        foreach hash<auto> i in (m_specs.pairIterator()) {
            HashDataType field();
            foreach hash<auto> ki in (i.value.pairIterator()) {
                *hash<auto> type_options = global_options;
                if (ki.value.format) {
                    switch (ki.type) {
                        case "float":
                        case "int":
                        case "number":
                            type_options."number.format" = ki.value.format;
                            break;
                        case "date":
                            type_options."date.format" = ki.value.format;
                            break;
                    }
                }
                if (ki.value.timezone) {
                    type_options."date.input_timezone" = ki.value.timezone;
                }
                Type base_type(OptimalQoreSoftDataTypeMap{$1.value.type} ?? "string");
                QoreDataField record_field(ki.key, NOTHING,
                    AbstractDataProviderType::get(base_type.getOrNothingType(), type_options));
                field.addField(record_field);
            }
        }
        return rv;
        */
    }

    #! Returns True if the string value of an \c "int", \c "float" or \c "number" field is a number of its type
    /** @param type the field type: \c "int", \c "float" or \c "number"
        @param value the string value of the field
        @param fmt the number format of the field, if any: the thousands separator, optionally followed by the decimal
        separator and the number of decimals (ex: \c ".," or \c ".,2")

        @return True if the value is a number of the field's type in the given format, or has only spaces (an empty
        field, read as zero)

        @since FixedLengthUtil 1.7
    */
    private static bool isNumericValue(string type, string value, *string fmt) {
        if (value =~ /^\s*$/) {
            return True;
        }
        switch (type) {
            case "int":
                # with a number format, only the thousands separator is removed: a value with the decimal separator is
                # not an integer (parse_int() would ignore the decimals)
                return fmt ? is_int(replace(value, fmt[0], ""), True) : is_int(value);
            case "float":
                return is_float(fmt ? FixedLengthBase::removeNumberSeparators(value, fmt) : value);
            case "number":
                return is_number(fmt ? FixedLengthBase::removeNumberSeparators(value, fmt) : value);
        }
        return True;
    }

    #! Returns the value without the separators of the given number format, so that its digits can be checked
    /** @param val the value
        @param fmt a number format: the thousands separator, optionally followed by the decimal separator and the
        number of decimals (ex: \c ".," or \c ".,2"); the format can have only the thousands separator

        @since FixedLengthUtil 1.7
    */
    private static string removeNumberSeparators(string val, string fmt) {
        for (int i = 0; i < 2 && i < fmt.length(); ++i) {
            val = replace(val, fmt[i], "");
        }
        return val;
    }
}
}
