# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreStringFormatDataType class definition

/** QoreStringFormatDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! A string-backed data type carrying a semantic format (email, uri, uuid, etc.)
/** These types are string-backed: @ref getBaseTypeName() returns \c "string", so the wire/storage
    representation is unchanged from a plain string.  The type's identity (@ref getName() and the
    \c qore.external_name tag) carries the semantic format so that user interfaces can render an
    appropriate input control (email field, IP field, etc.).

    When a format regex is supplied the type acts as a server-side backstop: @ref acceptsValue() validates
    string input against the regex and throws \c RUNTIME-TYPE-ERROR on failure.  Validation is applied only
    on input/coercion paths (never on records returned from a data provider), and the regexes are
    deliberately permissive.

    @since %DataProvider 3.6
*/
public class QoreStringFormatDataType inherits QoreStringDataTypeBase {
    private {
        #! An optional regex used to validate string input
        *string format_regex;
    }

    #! Creates the type
    /** @param base_type the base string type (@ref StringType or @ref StringOrNothingType)
        @param type_name the semantic type name (also used as the \c qore.external_name tag)
        @param display_name the type's display name
        @param short_desc the type's short description
        @param format_regex an optional permissive regex used to validate string input
        @param options optional type options
    */
    constructor(Type base_type, string type_name, string display_name, string short_desc,
            *string format_regex, *hash<auto> options)
            : QoreStringDataTypeBase(base_type, options,
                {"qore.external_name": type_name} + (format_regex ? {"qore.validation_regex": format_regex} : {}),
                NOTHING,
                <NameDescInfo>{
                    "name": type_name,
                    "display_name": display_name,
                    "short_desc": short_desc,
                    "desc": short_desc,
                }) {
        self.format_regex = format_regex;
    }

    #! Returns the value if it can be assigned to the type, validating any format regex
    /** @param value the value to assign to the type

        @return the value to be assigned; can be converted by the type

        @throw RUNTIME-TYPE-ERROR value cannot be assigned to the type or does not match the format
    */
    auto acceptsValue(auto value) {
        value = QoreStringDataTypeBase::acceptsValue(value);
        if (format_regex.val() && exists value && value.typeCode() == NT_STRING && value.val()
                && !regex(value, format_regex)) {
            throw "RUNTIME-TYPE-ERROR", sprintf("value %y is not a valid %y value", value, getName());
        }
        return value;
    }

    #! Returns the validation regex for the format, if any
    /** @return the permissive regex used to validate string input, or @ref nothing if the type does not
        validate its input
    */
    *string getValidationRegex() {
        return format_regex;
    }
}

#! An email address (string-backed)
public const EmailType = new QoreStringFormatDataType(StringType, "email", "Email",
    "An email address", '^[^@\s]+@[^@\s]+$');
#! An email address or no value
public const EmailOrNothingType = EmailType.getOrNothingType();

#! A URI (string-backed)
public const UriType = new QoreStringFormatDataType(StringType, "uri", "URI",
    "A uniform resource identifier", '^[A-Za-z][A-Za-z0-9+.-]*:');
#! A URI or no value
public const UriOrNothingType = UriType.getOrNothingType();

#! A UUID (string-backed)
public const UuidType = new QoreStringFormatDataType(StringType, "uuid", "UUID",
    "A universally-unique identifier",
    '^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$');
#! A UUID or no value
public const UuidOrNothingType = UuidType.getOrNothingType();

#! A hostname (string-backed)
public const HostnameType = new QoreStringFormatDataType(StringType, "hostname", "Hostname",
    "A host or domain name", '^[A-Za-z0-9.\-]+$');
#! A hostname or no value
public const HostnameOrNothingType = HostnameType.getOrNothingType();

#! An IPv4 address (string-backed)
public const Ipv4Type = new QoreStringFormatDataType(StringType, "ipv4", "IPv4 Address",
    "An IPv4 address",
    '^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)(\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)){3}$');
#! An IPv4 address or no value
public const Ipv4OrNothingType = Ipv4Type.getOrNothingType();

#! An IPv6 address (string-backed)
public const Ipv6Type = new QoreStringFormatDataType(StringType, "ipv6", "IPv6 Address",
    "An IPv6 address", '^([0-9A-Fa-f]{0,4}:){2,7}[0-9A-Fa-f]{0,4}$');
#! An IPv6 address or no value
public const Ipv6OrNothingType = Ipv6Type.getOrNothingType();

#! A telephone number (string-backed)
public const PhoneType = new QoreStringFormatDataType(StringType, "phone", "Phone Number",
    "A telephone number", '^\+?[0-9 ().\-]{3,}$');
#! A telephone number or no value
public const PhoneOrNothingType = PhoneType.getOrNothingType();
}
