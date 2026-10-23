# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DataTypeCatalogue.qc the plain-words names of the data types

/*  DataTypeCatalogue.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The plain-words names of a data type, as a user interface shows them to a person
/** @see @ref DataProvider::DataTypeCatalogue "DataTypeCatalogue"

    @since %DataProvider 3.11
*/
public hashdecl DataTypeCatalogueInfo {
    #! The name of the type that the entry names: a base type name, such as \c int, or the name of a type with a
    #! presentation of its own, such as \c rgbcolor
    string name;

    #! The name that labels the type in pickers, forms, lists and completion details, such as \c "Whole number"
    string display_name;

    #! The name of the type inside a sentence, with its article, such as \c "a whole number"
    /** A capitalized label reads badly in running text, and a sentence that adds its own article to a label cannot
        be translated: "The expression gives a whole number, and this field takes key-value pairs (a hash)".
    */
    string sentence_form;

    #! A short description of the type in plain text
    string short_desc;

    #! A description of the type in plain text
    string desc;
}

#! The catalogue of the data types as a user interface names them to a person
/** Each type has two names: its @ref DataProvider::DataTypeCatalogueInfo::display_name "display_name" labels it in
    pickers, forms, lists and DPQL completion details (\c "Whole number"), and its
    @ref DataProvider::DataTypeCatalogueInfo::sentence_form "sentence_form" names it inside a sentence
    (\c "a whole number").

    The catalogue names the base data types (\c any, \c string, \c int, \c float, \c number, \c bool, \c date,
    \c binary, \c data, \c hash, \c list, \c timeout and \c nothing) and the types of this module with a
    presentation of their own (\c rgbcolor, \c file and the string formats \c email, \c uri, \c uuid, \c hostname,
    \c ipv4, \c ipv6 and \c phone).  An optional, soft or complex type of a type that the catalogue names
    (\c *string, \c softint, \c hash<auto>, \c list<string>) is named by the entry of that type, as are the aliases
    \c integer, \c boolean, \c auto and \c null and the types that hold a value of a base type in another form
    (\c byte, \c base64binary, \c unixts).  The \c object type is not named.  A host application can build the
    entries of its own catalogue of types for these types from @ref Info, so that a type is named in the same words
    everywhere.

    @par Example
    @code{.py}
string name = DataTypeCatalogue::lookup("*int").display_name;  # "Whole number"
string sentence = sprintf("The expression gives %s",
    DataTypeCatalogue::lookupType(AbstractDataProviderTypeMap."hash").sentence_form);
# "The expression gives key-value pairs (a hash)"
    @endcode

    @since %DataProvider 3.11
*/
public class DataTypeCatalogue {
    public {
        #! The catalogue in presentation order
        const list<hash<DataTypeCatalogueInfo>> Info = (
            <DataTypeCatalogueInfo>{
                "name": "any",
                "display_name": "Any",
                "sentence_form": "any value",
                "short_desc": "Any data type",
                "desc": "Any data type",
            },
            <DataTypeCatalogueInfo>{
                "name": "string",
                "display_name": "Text",
                "sentence_form": "text",
                "short_desc": "Text",
                "desc": "Text",
            },
            <DataTypeCatalogueInfo>{
                "name": "int",
                "display_name": "Whole number",
                "sentence_form": "a whole number",
                "short_desc": "A whole number (64-bit)",
                "desc": "A whole number (64-bit)",
            },
            <DataTypeCatalogueInfo>{
                "name": "float",
                "display_name": "Number",
                "sentence_form": "a number",
                "short_desc": "A number with a fractional part (double precision)",
                "desc": "A number with a fractional part (double precision)",
            },
            <DataTypeCatalogueInfo>{
                "name": "number",
                "display_name": "Exact number",
                "sentence_form": "an exact number",
                "short_desc": "A decimal number kept exactly",
                "desc": "A decimal number kept exactly",
            },
            <DataTypeCatalogueInfo>{
                "name": "bool",
                "display_name": "True or false",
                "sentence_form": "true or false",
                "short_desc": "True or false",
                "desc": "True or false",
            },
            <DataTypeCatalogueInfo>{
                "name": "date",
                "display_name": "Date",
                "sentence_form": "a date and time",
                "short_desc": "A date and time",
                "desc": "A date and time",
            },
            <DataTypeCatalogueInfo>{
                "name": "binary",
                "display_name": "Binary data",
                "sentence_form": "binary data",
                "short_desc": "Binary data",
                "desc": "Binary data",
            },
            <DataTypeCatalogueInfo>{
                "name": "data",
                "display_name": "Text or binary data",
                "sentence_form": "text or binary data",
                "short_desc": "Text or binary data",
                "desc": "Text or binary data",
            },
            <DataTypeCatalogueInfo>{
                "name": "hash",
                "display_name": "Key-value pairs (hash)",
                "sentence_form": "key-value pairs (a hash)",
                "short_desc": "Key-value pairs (hash)",
                "desc": "Key-value pairs (a hash): each value is stored under a named key",
            },
            <DataTypeCatalogueInfo>{
                "name": "list",
                "display_name": "List",
                "sentence_form": "a list",
                "short_desc": "List",
                "desc": "List",
            },
            <DataTypeCatalogueInfo>{
                "name": "timeout",
                "display_name": "Timeout",
                "sentence_form": "a timeout",
                "short_desc": "A time limit: a number of milliseconds, or a duration such as 30s",
                "desc": "A time limit: a whole number of milliseconds, or a duration such as 30s, which is converted "
                    "to milliseconds",
            },
            <DataTypeCatalogueInfo>{
                "name": "nothing",
                "display_name": "No value",
                "sentence_form": "no value",
                "short_desc": "No value",
                "desc": "No value",
            },
            <DataTypeCatalogueInfo>{
                "name": "rgbcolor",
                "display_name": "Color",
                "sentence_form": "a color",
                "short_desc": "A color: red, green and blue from 0 to 255 and an opacity from 0 to 1",
                "desc": "A color, stored as a hash: r, g and b are integers from 0 to 255, and a is the opacity, from "
                    "0 (transparent) to 1 (opaque); a missing or null a is opaque. A hex code such as #ff0000 is "
                    "not a stored color.",
            },
            <DataTypeCatalogueInfo>{
                "name": "file",
                "display_name": "File",
                "sentence_form": "a file",
                "short_desc": "A file: its name, its content and its MIME type",
                "desc": "A file, stored as a hash: its name, its content (raw or base64-encoded) and its MIME type",
            },
            <DataTypeCatalogueInfo>{
                "name": "email",
                "display_name": "Email address",
                "sentence_form": "an email address",
                "short_desc": "An email address",
                "desc": "An email address, such as user@example.com",
            },
            <DataTypeCatalogueInfo>{
                "name": "uri",
                "display_name": "Address (URI)",
                "sentence_form": "an address (a URI)",
                "short_desc": "An address that starts with its scheme (a URI)",
                "desc": "An address that starts with its scheme (a URI), such as https://example.com/",
            },
            <DataTypeCatalogueInfo>{
                "name": "uuid",
                "display_name": "Unique identifier (UUID)",
                "sentence_form": "a unique identifier (a UUID)",
                "short_desc": "A universally unique identifier (a UUID)",
                "desc": "A universally unique identifier (a UUID): 32 hexadecimal digits in groups of 8, 4, 4, 4 and "
                    "12, such as 123e4567-e89b-12d3-a456-426614174000",
            },
            <DataTypeCatalogueInfo>{
                "name": "hostname",
                "display_name": "Host name",
                "sentence_form": "a host name",
                "short_desc": "A host or domain name",
                "desc": "A host or domain name, such as example.com",
            },
            <DataTypeCatalogueInfo>{
                "name": "ipv4",
                "display_name": "IPv4 address",
                "sentence_form": "an IPv4 address",
                "short_desc": "An IPv4 address",
                "desc": "An IPv4 address, such as 192.0.2.1",
            },
            <DataTypeCatalogueInfo>{
                "name": "ipv6",
                "display_name": "IPv6 address",
                "sentence_form": "an IPv6 address",
                "short_desc": "An IPv6 address",
                "desc": "An IPv6 address, such as 2001:db8::1",
            },
            <DataTypeCatalogueInfo>{
                "name": "phone",
                "display_name": "Phone number",
                "sentence_form": "a phone number",
                "short_desc": "A telephone number",
                "desc": "A telephone number, such as +1 555 0100",
            },
        );

        #! The catalogue keyed by type name
        const hash<string, hash<DataTypeCatalogueInfo>> Map = map {$1.name: $1}, Info;

        #! The names of the types that the entry of another type names
        /** Aliases of a type, and types that hold a value of a base type in another form
        */
        const hash<string, string> Aliases = {
            "auto": "any",
            "null": "nothing",
            "integer": "int",
            "boolean": "bool",
            "byte": "int",
            "ubyte": "int",
            "base64binary": "binary",
            "base64urlbinary": "binary",
            "hexbinary": "binary",
            "supersoftstring": "string",
            "unixts": "date",
            "unixtsms": "date",
        };
    }

    #! Returns the catalogue entry that names a type, given its name
    /** A type that the catalogue has is named by its own entry; an optional, soft or complex type of one (\c *string,
        \c softint, \c hash<auto>, \c list<string>) and an alias (\c integer, \c boolean, \c auto, \c byte,
        \c base64binary, \c unixts) are named by the entry of the type they are a form of.

        @param type a type name

        @return the type's entry, or @ref nothing for a type that the catalogue does not name

        @par Example
        @code{.py}
string name = DataTypeCatalogue::lookup("hash<auto>").display_name;  # "Key-value pairs (hash)"
        @endcode
    */
    static *hash<DataTypeCatalogueInfo> lookup(string type) {
        *hash<DataTypeCatalogueInfo> info = Map{type};
        if (info) {
            return info;
        }
        string base = type;
        base =~ s/^\*//;
        base =~ s/<.*$//;
        if (*hash<DataTypeCatalogueInfo> base_info = DataTypeCatalogue::lookupName(base)) {
            return base_info;
        }
        if (base =~ /^soft./) {
            return DataTypeCatalogue::lookupName(substr(base, 4));
        }
    }

    #! Returns the entry of a type name or of the type that it is an alias of
    private static *hash<DataTypeCatalogueInfo> lookupName(string name) {
        if (*hash<DataTypeCatalogueInfo> info = Map{name}) {
            return info;
        }
        *string alias = Aliases{name};
        if (alias) {
            return Map{alias};
        }
    }

    #! Returns the catalogue entry that names a type
    /** The type is named by the entry of its name (@ref lookup()); a type whose name the catalogue does not have, such
        as a hash type named after the record it describes, is named by the entry of the type it presents itself as
        (its \c qore.external_name tag), and else by the entry of its base type (a record type is
        \c "Key-value pairs (hash)").

        @param type the type

        @return the type's entry, or @ref nothing for a type that the catalogue does not name

        @par Example
        @code{.py}
string name = DataTypeCatalogue::lookupType(AbstractDataProviderTypeMap."softint").display_name;  # "Whole number"
        @endcode
    */
    static *hash<DataTypeCatalogueInfo> lookupType(AbstractDataProviderType type) {
        if (*hash<DataTypeCatalogueInfo> info = DataTypeCatalogue::lookup(type.getName())) {
            return info;
        }
        *string external_name = type.getTag("qore.external_name");
        if (external_name.val()) {
            if (*hash<DataTypeCatalogueInfo> info = DataTypeCatalogue::lookup(external_name)) {
                return info;
            }
        }
        return DataTypeCatalogue::lookup(type.getBaseTypeName());
    }
}
}
