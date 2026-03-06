# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapCompanyDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Keap company record type
public class KeapCompanyDataType inherits HashDataType {
    private {
        #! Field definitions for Keap companies
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The company ID",
                "desc": "The unique identifier of the company",
            },
            "company_name": {
                "type": StringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "The company name",
                "desc": "The name of the company",
                "example_value": "Acme Corp",
            },
            "email_address": {
                "type": StringOrNothingType,
                "display_name": "Email Address",
                "short_desc": "The company email address",
                "desc": "The primary email address of the company",
                "example_value": "info@acme.com",
            },
            "phone_number": {
                "type": StringOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "The company phone number",
                "desc": "The primary phone number of the company",
            },
            "website": {
                "type": StringOrNothingType,
                "display_name": "Website",
                "short_desc": "The company website",
                "desc": "The website URL of the company",
                "example_value": "https://www.acme.com",
            },
            "address": {
                "type": AutoHashOrNothingType,
                "display_name": "Address",
                "short_desc": "The company address",
                "desc": "The physical address of the company (hash with line1, line2, locality, "
                    "region, zip_code, country_code)",
            },
            "date_created": {
                "type": DateOrNothingType,
                "display_name": "Date Created",
                "short_desc": "When the company was created",
                "desc": "The timestamp when the company was created",
            },
            "last_updated": {
                "type": DateOrNothingType,
                "display_name": "Last Updated",
                "short_desc": "When the company was last updated",
                "desc": "The timestamp when the company was last updated",
            },
            "notes": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Notes about the company",
                "desc": "Free-text notes about the company",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KeapDataProvider
