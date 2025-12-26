# -*- mode: qore; indent-tabs-mode: nil -*-

/*  UnixTimestampMsType.qc Copyright (C) 2025 Qore Technologies s.r.o.

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
#! A type that converts dates to UNIX timestamps in milliseconds
public const UnixTimestampMsType = new UnixTimestampMsType();

#! A type that converts dates to UNIX timestamps in milliseconds
public class UnixTimestampMsType inherits UnixTimestampType {
    #! creates the field from the argument
    constructor(*hash<auto> options, *hash<auto> tags) : UnixTimestampType(options, tags) {
    }

    #! Returns the type name
    string getName() {
        return "unixtsms";
    }

    #! Returns the type's display name
    /** @return the type's display name

        @since DataProvider 3.0
    */
    string getDisplayName() {
        return "Unix Timestamp Milliseconds";
    }

    #! Returns the type's short description
    /** @return the plain-text short description for the type

        @since DataProvider 3.0
    */
    string getShortDescription() {
        return "Accepts a date/time value and returns the number of milliseconds after 1970-01-01Z";
    }

    #! Returns the description
    string getDesc() {
        return "Accepts a date/time value and returns the number of milliseconds after 1970-01-01Z";
    }

    #! returns the value if the value can be assigned to the type
    /** @param value the value to assign to the type

        @return the value to be assigned; can be converted by the type

        @throw RUNTIME-TYPE-ERROR value cannot be assigned to type
    */
    auto acceptsValue(auto value) {
        switch (value.typeCode()) {
            case NT_DATE:
                return value.getEpochMilliseconds();

            case NT_INT:
                return value;

            default:
                throw "RUNTIME-TYPE-ERROR", sprintf("Type %s does not accept values of type %y (%y)", getName(),
                    value.fullType(), value);
        }
    }

    #! Returns an "or nothing" type equivalent to the current type
    /** The base class method returns the same type; this method must be overridden in child classes to return a
        usable "or nothing" type

        @return an "or nothing" type equivalent to the current type
    */
    AbstractDataProviderType getOrNothingType() {
        return UnixTimestampMsOrNothingType;
    }
}
}
