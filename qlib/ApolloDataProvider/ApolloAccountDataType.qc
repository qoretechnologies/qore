# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ApolloDataProvider module definition

/*  ApolloAccountDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ApolloDataProvider module
public namespace ApolloDataProvider {

#! Apollo account record type
public class ApolloAccountDataType inherits HashDataType {
    public {
        #! Field definitions for Apollo accounts
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "Account ID",
                "desc": "The unique identifier of the account",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Account/company name",
                "desc": "The name of the account or company",
                "example_value": "Acme Corp",
            },
            "website_url": {
                "type": StringOrNothingType,
                "display_name": "Website URL",
                "short_desc": "Website URL",
                "desc": "The website URL of the account",
            },
            "domain": {
                "type": StringOrNothingType,
                "display_name": "Domain",
                "short_desc": "Primary domain",
                "desc": "The primary domain of the account",
            },
            "linkedin_url": {
                "type": StringOrNothingType,
                "display_name": "LinkedIn URL",
                "short_desc": "LinkedIn URL",
                "desc": "The LinkedIn URL of the account",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The phone number of the account",
            },
            "owner_id": {
                "type": StringOrNothingType,
                "display_name": "Owner ID",
                "short_desc": "Owner user ID",
                "desc": "The unique identifier of the user who owns this account",
            },
            "account_stage_id": {
                "type": StringOrNothingType,
                "display_name": "Account Stage ID",
                "short_desc": "Stage ID",
                "desc": "The unique identifier of the account's stage",
            },
            "created_at": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "Created timestamp",
                "desc": "The timestamp when the account was created",
            },
            "label_ids": {
                "type": AutoListOrNothingType,
                "display_name": "Label IDs",
                "short_desc": "Labels",
                "desc": "The list of label IDs associated with the account",
            },
            "typed_custom_fields": {
                "type": AutoHashOrNothingType,
                "display_name": "Custom Fields",
                "short_desc": "Custom fields",
                "desc": "Custom fields associated with the account",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ApolloDataProvider
