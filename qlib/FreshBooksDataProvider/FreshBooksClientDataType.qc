# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshBooksDataProvider module definition

/*  FreshBooksClientDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshBooksDataProvider module
public namespace FreshBooksDataProvider {

#! FreshBooks client record type
public class FreshBooksClientDataType inherits HashDataType {
    private {
        #! Field definitions for FreshBooks clients
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The client ID",
                "desc": "The unique identifier of the client",
            },
            "fname": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The client first name",
                "desc": "The first name of the client",
                "example_value": "John",
            },
            "lname": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The client last name",
                "desc": "The last name of the client",
                "example_value": "Doe",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The client email address",
                "desc": "The email address of the client",
                "example_value": "john@example.com",
            },
            "organization": {
                "type": StringOrNothingType,
                "display_name": "Organization",
                "short_desc": "The organization/company name",
                "desc": "The organization or company name of the client",
                "example_value": "Acme Corp",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Business Phone",
                "short_desc": "The business phone number",
                "desc": "The business phone number of the client",
            },
            "home_phone": {
                "type": StringOrNothingType,
                "display_name": "Home Phone",
                "short_desc": "The home phone number",
                "desc": "The home phone number of the client",
            },
            "note": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Notes about the client",
                "desc": "Notes about the client",
            },
            "vat_name": {
                "type": StringOrNothingType,
                "display_name": "VAT Name",
                "short_desc": "The VAT name",
                "desc": "The VAT name for the client",
            },
            "vat_number": {
                "type": StringOrNothingType,
                "display_name": "VAT Number",
                "short_desc": "The VAT number",
                "desc": "The VAT number for the client",
            },
            "p_street": {
                "type": StringOrNothingType,
                "display_name": "Street Address",
                "short_desc": "The street address",
                "desc": "The street address of the client",
            },
            "p_city": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "The city",
                "desc": "The city of the client",
            },
            "p_province": {
                "type": StringOrNothingType,
                "display_name": "Province/State",
                "short_desc": "The province or state",
                "desc": "The province or state of the client",
            },
            "p_code": {
                "type": StringOrNothingType,
                "display_name": "Postal/Zip Code",
                "short_desc": "The postal or zip code",
                "desc": "The postal or zip code of the client",
            },
            "p_country": {
                "type": StringOrNothingType,
                "display_name": "Country",
                "short_desc": "The country",
                "desc": "The country of the client",
            },
            "vis_state": {
                "type": IntOrNothingType,
                "display_name": "Visibility State",
                "short_desc": "The visibility state",
                "desc": "The visibility state of the client (0=active, 1=deleted, 2=archived)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! FreshBooks client create request type
public class FreshBooksClientCreateDataType inherits HashDataType {
    private {
        #! Field definitions for creating clients
        const Fields = FreshBooksDataProviderBase::AccountIdField + {
            "fname": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The client first name",
                "desc": "The first name of the client (at least one of fname, lname, or organization is "
                    "required)",
                "example_value": "John",
            },
            "lname": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The client last name",
                "desc": "The last name of the client (at least one of fname, lname, or organization is "
                    "required)",
                "example_value": "Doe",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The client email address",
                "desc": "The email address of the client",
                "example_value": "john@example.com",
            },
            "organization": {
                "type": StringOrNothingType,
                "display_name": "Organization",
                "short_desc": "The organization/company name",
                "desc": "The organization or company name of the client (at least one of fname, lname, or "
                    "organization is required)",
                "example_value": "Acme Corp",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Business Phone",
                "short_desc": "The business phone number",
                "desc": "The business phone number of the client",
            },
            "home_phone": {
                "type": StringOrNothingType,
                "display_name": "Home Phone",
                "short_desc": "The home phone number",
                "desc": "The home phone number of the client",
            },
            "note": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Notes about the client",
                "desc": "Notes about the client",
            },
            "vat_name": {
                "type": StringOrNothingType,
                "display_name": "VAT Name",
                "short_desc": "The VAT name",
                "desc": "The VAT name for the client",
            },
            "vat_number": {
                "type": StringOrNothingType,
                "display_name": "VAT Number",
                "short_desc": "The VAT number",
                "desc": "The VAT number for the client",
            },
            "p_street": {
                "type": StringOrNothingType,
                "display_name": "Street Address",
                "short_desc": "The street address",
                "desc": "The street address of the client",
            },
            "p_city": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "The city",
                "desc": "The city of the client",
            },
            "p_province": {
                "type": StringOrNothingType,
                "display_name": "Province/State",
                "short_desc": "The province or state",
                "desc": "The province or state of the client",
            },
            "p_code": {
                "type": StringOrNothingType,
                "display_name": "Postal/Zip Code",
                "short_desc": "The postal or zip code",
                "desc": "The postal or zip code of the client",
            },
            "p_country": {
                "type": StringOrNothingType,
                "display_name": "Country",
                "short_desc": "The country",
                "desc": "The country of the client",
            },
            "vis_state": {
                "type": IntOrNothingType,
                "display_name": "Visibility State",
                "short_desc": "The visibility state",
                "desc": "The visibility state of the client (0=active, 1=deleted, 2=archived)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! FreshBooks client update request type
public class FreshBooksClientUpdateDataType inherits HashDataType {
    private {
        #! Field definitions for updating clients
        const Fields = FreshBooksDataProviderBase::AccountIdField + {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The client ID (required)",
                "desc": "The unique identifier of the client to update",
                "required": True,
            },
            "fname": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The client first name",
                "desc": "The new first name of the client",
                "example_value": "John",
            },
            "lname": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The client last name",
                "desc": "The new last name of the client",
                "example_value": "Doe",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The client email address",
                "desc": "The new email address of the client",
                "example_value": "john@example.com",
            },
            "organization": {
                "type": StringOrNothingType,
                "display_name": "Organization",
                "short_desc": "The organization/company name",
                "desc": "The new organization or company name of the client",
                "example_value": "Acme Corp",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Business Phone",
                "short_desc": "The business phone number",
                "desc": "The new business phone number of the client",
            },
            "home_phone": {
                "type": StringOrNothingType,
                "display_name": "Home Phone",
                "short_desc": "The home phone number",
                "desc": "The new home phone number of the client",
            },
            "note": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Notes about the client",
                "desc": "The new notes about the client",
            },
            "vat_name": {
                "type": StringOrNothingType,
                "display_name": "VAT Name",
                "short_desc": "The VAT name",
                "desc": "The new VAT name for the client",
            },
            "vat_number": {
                "type": StringOrNothingType,
                "display_name": "VAT Number",
                "short_desc": "The VAT number",
                "desc": "The new VAT number for the client",
            },
            "p_street": {
                "type": StringOrNothingType,
                "display_name": "Street Address",
                "short_desc": "The street address",
                "desc": "The new street address of the client",
            },
            "p_city": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "The city",
                "desc": "The new city of the client",
            },
            "p_province": {
                "type": StringOrNothingType,
                "display_name": "Province/State",
                "short_desc": "The province or state",
                "desc": "The new province or state of the client",
            },
            "p_code": {
                "type": StringOrNothingType,
                "display_name": "Postal/Zip Code",
                "short_desc": "The postal or zip code",
                "desc": "The new postal or zip code of the client",
            },
            "p_country": {
                "type": StringOrNothingType,
                "display_name": "Country",
                "short_desc": "The country",
                "desc": "The new country of the client",
            },
            "vis_state": {
                "type": IntOrNothingType,
                "display_name": "Visibility State",
                "short_desc": "The visibility state",
                "desc": "The new visibility state of the client (0=active, 1=deleted, 2=archived)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FreshBooksDataProvider
