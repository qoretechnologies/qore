# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceCreateCustomerDataProvider.qc BigCommerce create customer action data provider

/*  BigCommerceCreateCustomerDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BigCommerceDataProvider module
public namespace BigCommerceDataProvider {

#! Request type for creating a customer
public class BigCommerceCreateCustomerRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "first_name": {
                "display_name": "First Name",
                "short_desc": "Customer first name",
                "type": StringType,
                "desc": "The customer's first name",
                "required": True,
                "example_value": "John",
            },
            "last_name": {
                "display_name": "Last Name",
                "short_desc": "Customer last name",
                "type": StringType,
                "desc": "The customer's last name",
                "required": True,
                "example_value": "Doe",
            },
            "email": {
                "display_name": "Email",
                "short_desc": "Customer email address",
                "type": StringType,
                "desc": "The customer's email address",
                "required": True,
                "example_value": "john@example.com",
            },
            "company": {
                "display_name": "Company",
                "short_desc": "Company name",
                "type": StringOrNothingType,
                "desc": "The customer's company name",
            },
            "phone": {
                "display_name": "Phone",
                "short_desc": "Phone number",
                "type": StringOrNothingType,
                "desc": "The customer's phone number",
            },
            "notes": {
                "display_name": "Notes",
                "short_desc": "Admin notes",
                "type": StringOrNothingType,
                "desc": "Admin notes about the customer",
            },
            "customer_group_id": {
                "display_name": "Customer Group ID",
                "short_desc": "Customer group ID",
                "type": IntOrNothingType,
                "desc": "ID of the customer group to assign",
                "ref_data": "customer_groups",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! BigCommerce create customer action data provider
public class BigCommerceCreateCustomerDataProvider inherits BigCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new customer in BigCommerce",
            "type": "BigCommerceCreateCustomerDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(BigCommerceRestClient::BigCommerceRestClient rest) : BigCommerceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new customer in BigCommerce";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BigCommerceCreateCustomerRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BigCommerceCustomerDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doCreateSingleV3("customers", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BigCommerceDataProvider
