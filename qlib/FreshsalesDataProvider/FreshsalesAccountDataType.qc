# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshsalesDataProvider module definition

/*  FreshsalesAccountDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshsalesDataProvider module
public namespace FreshsalesDataProvider {

#! Freshsales account record type
public class FreshsalesAccountDataType inherits HashDataType {
    public {
        #! Field definitions for Freshsales accounts
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The account ID",
                "desc": "The unique identifier of the account",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Company name",
                "desc": "The name of the account/company",
            },
            "address": {
                "type": StringOrNothingType,
                "display_name": "Address",
                "short_desc": "Street address",
                "desc": "The street address of the account",
            },
            "city": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "City",
                "desc": "The city of the account",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "State or province",
                "desc": "The state or province of the account",
            },
            "zipcode": {
                "type": StringOrNothingType,
                "display_name": "Zip Code",
                "short_desc": "Postal code",
                "desc": "The postal or zip code of the account",
            },
            "country": {
                "type": StringOrNothingType,
                "display_name": "Country",
                "short_desc": "Country name",
                "desc": "The country of the account",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The phone number of the account",
            },
            "website": {
                "type": StringOrNothingType,
                "display_name": "Website",
                "short_desc": "Company website",
                "desc": "The website URL of the account",
            },
            "industry_type_id": {
                "type": IntOrNothingType,
                "display_name": "Industry Type ID",
                "short_desc": "Industry category",
                "desc": "The ID of the industry type for this account",
            },
            "business_type_id": {
                "type": IntOrNothingType,
                "display_name": "Business Type ID",
                "short_desc": "Business classification",
                "desc": "The ID of the business type for this account",
            },
            "number_of_employees": {
                "type": IntOrNothingType,
                "display_name": "Number of Employees",
                "short_desc": "Employee count",
                "desc": "The number of employees at the account",
            },
            "annual_revenue": {
                "type": FloatOrNothingType,
                "display_name": "Annual Revenue",
                "short_desc": "Yearly revenue",
                "desc": "The annual revenue of the account",
            },
            "owner_id": {
                "type": IntOrNothingType,
                "display_name": "Owner ID",
                "short_desc": "Assigned user ID",
                "desc": "The ID of the user who owns this account",
            },
            "territory_id": {
                "type": IntOrNothingType,
                "display_name": "Territory ID",
                "short_desc": "Territory identifier",
                "desc": "The ID of the territory this account belongs to",
            },
            "facebook": {
                "type": StringOrNothingType,
                "display_name": "Facebook",
                "short_desc": "Facebook profile",
                "desc": "The Facebook profile of the account",
            },
            "twitter": {
                "type": StringOrNothingType,
                "display_name": "Twitter",
                "short_desc": "Twitter handle",
                "desc": "The Twitter handle of the account",
            },
            "linkedin": {
                "type": StringOrNothingType,
                "display_name": "LinkedIn",
                "short_desc": "LinkedIn profile",
                "desc": "The LinkedIn profile of the account",
            },
            "parent_sales_account_id": {
                "type": IntOrNothingType,
                "display_name": "Parent Account ID",
                "short_desc": "Parent company ID",
                "desc": "The ID of the parent account/company",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The timestamp when the account was created",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "The timestamp when the account was last updated",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Freshsales account request data type for create operations
public class FreshsalesCreateAccountRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating an account
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "required": True,
                "short_desc": "Company name",
                "desc": "The name of the account/company",
                "example_value": "Acme Corporation",
            },
            "address": {
                "type": StringOrNothingType,
                "display_name": "Address",
                "short_desc": "Street address",
                "desc": "The street address of the account",
            },
            "city": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "City",
                "desc": "The city of the account",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "State or province",
                "desc": "The state or province of the account",
            },
            "zipcode": {
                "type": StringOrNothingType,
                "display_name": "Zip Code",
                "short_desc": "Postal code",
                "desc": "The postal or zip code of the account",
            },
            "country": {
                "type": StringOrNothingType,
                "display_name": "Country",
                "short_desc": "Country name",
                "desc": "The country of the account",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The phone number of the account",
                "preselected": True,
            },
            "website": {
                "type": StringOrNothingType,
                "display_name": "Website",
                "short_desc": "Company website",
                "desc": "The website URL of the account",
                "preselected": True,
            },
            "industry_type_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Industry Type ID",
                "short_desc": "Industry category",
                "desc": "The ID of the industry type for this account",
                "ref_data": "industry_types",
            },
            "business_type_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Business Type ID",
                "short_desc": "Business classification",
                "desc": "The ID of the business type for this account",
                "ref_data": "business_types",
            },
            "number_of_employees": {
                "type": SoftIntOrNothingType,
                "display_name": "Number of Employees",
                "short_desc": "Employee count",
                "desc": "The number of employees at the account",
            },
            "annual_revenue": {
                "type": SoftFloatOrNothingType,
                "display_name": "Annual Revenue",
                "short_desc": "Yearly revenue",
                "desc": "The annual revenue of the account",
            },
            "owner_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Owner ID",
                "short_desc": "Assigned user ID",
                "desc": "The ID of the user to assign as owner",
                "ref_data": "owners",
            },
            "facebook": {
                "type": StringOrNothingType,
                "display_name": "Facebook",
                "short_desc": "Facebook profile",
                "desc": "The Facebook profile of the account",
            },
            "twitter": {
                "type": StringOrNothingType,
                "display_name": "Twitter",
                "short_desc": "Twitter handle",
                "desc": "The Twitter handle of the account",
            },
            "linkedin": {
                "type": StringOrNothingType,
                "display_name": "LinkedIn",
                "short_desc": "LinkedIn profile",
                "desc": "The LinkedIn profile of the account",
            },
            "territory_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Territory ID",
                "short_desc": "Territory identifier",
                "desc": "The ID of the territory this account belongs to",
                "ref_data": "territories",
            },
            "parent_sales_account_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Parent Account ID",
                "short_desc": "Parent company ID",
                "desc": "The ID of the parent account/company",
                "ref_data": "sales_accounts",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Freshsales account request data type for update operations
public class FreshsalesUpdateAccountRequestDataType inherits HashDataType {
    public {
        #! Field definitions for updating an account
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "Account ID",
                "required": True,
                "short_desc": "The account ID to update",
                "desc": "The unique identifier of the account to update",
            },
        } + FreshsalesDataProviderBase::makeFieldsOptional(
            FreshsalesCreateAccountRequestDataType::Fields);
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Freshsales account request data type for delete operations
public class FreshsalesDeleteAccountRequestDataType inherits HashDataType {
    public {
        #! Field definitions for deleting an account
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "Account ID",
                "required": True,
                "short_desc": "The account ID to delete",
                "desc": "The unique identifier of the account to delete",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FreshsalesDataProvider
