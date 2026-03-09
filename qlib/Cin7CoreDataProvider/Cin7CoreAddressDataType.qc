# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreAddressDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the Cin7CoreDataProvider module
public namespace Cin7CoreDataProvider {

#! Cin7Core address data type
public class Cin7CoreAddressDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "Line1": {
                "type": SoftStringOrNothingType,
                "display_name": "Address Line 1",
                "short_desc": "First line of address",
                "desc": "The first line of the address (street address)",
            },
            "Line2": {
                "type": SoftStringOrNothingType,
                "display_name": "Address Line 2",
                "short_desc": "Second line of address",
                "desc": "The second line of the address (apt, suite, etc.)",
            },
            "City": {
                "type": SoftStringOrNothingType,
                "display_name": "City",
                "short_desc": "City name",
                "desc": "The city name",
            },
            "State": {
                "type": SoftStringOrNothingType,
                "display_name": "State/Region",
                "short_desc": "State or region",
                "desc": "The state, province, or region",
            },
            "Postcode": {
                "type": SoftStringOrNothingType,
                "display_name": "Postal Code",
                "short_desc": "Postal/ZIP code",
                "desc": "The postal or ZIP code",
            },
            "Country": {
                "type": SoftStringOrNothingType,
                "display_name": "Country",
                "short_desc": "Country name",
                "desc": "The country name",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreAddressDataType") {
        addQoreFields(Fields);
    }

    #! Creates the object with a base type
    constructor(Type base_type) : HashDataType(base_type, "Cin7CoreAddressDataType") {
        addQoreFields(Fields);
    }
}

#! Cin7Core contact data type
public class Cin7CoreContactDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "ID": {
                "type": SoftStringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "Unique contact identifier",
                "desc": "The unique identifier of the contact",
            },
            "Name": {
                "type": SoftStringOrNothingType,
                "display_name": "Contact Name",
                "short_desc": "Full name",
                "desc": "The full name of the contact",
            },
            "Phone": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The phone number of the contact",
            },
            "Fax": {
                "type": SoftStringOrNothingType,
                "display_name": "Fax",
                "short_desc": "Fax number",
                "desc": "The fax number of the contact",
            },
            "Email": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Email address",
                "desc": "The email address of the contact",
            },
            "Website": {
                "type": SoftStringOrNothingType,
                "display_name": "Website",
                "short_desc": "Website URL",
                "desc": "The website URL of the contact",
            },
            "Comment": {
                "type": SoftStringOrNothingType,
                "display_name": "Comment",
                "short_desc": "Additional notes",
                "desc": "Additional notes or comments about the contact",
            },
            "Default": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Default",
                "short_desc": "Default contact flag",
                "desc": "Whether this is the default contact",
            },
            "IncludeInEmail": {
                "type": SoftBoolOrNothingType,
                "display_name": "Include in Email",
                "short_desc": "Email notification flag",
                "desc": "Whether to include this contact in email notifications",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreContactDataType") {
        addQoreFields(Fields);
    }

    #! Creates the object with a base type
    constructor(Type base_type) : HashDataType(base_type, "Cin7CoreContactDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
