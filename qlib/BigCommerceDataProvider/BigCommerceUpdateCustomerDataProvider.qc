# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceUpdateCustomerDataProvider.qc BigCommerce update customer action data provider

/*  BigCommerceUpdateCustomerDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a customer
public class BigCommerceUpdateCustomerRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Customer ID",
                "type": IntType,
                "desc": "The ID of the customer to update",
                "required": True,
            },
            "first_name": {
                "display_name": "First Name",
                "type": StringOrNothingType,
                "desc": "The customer's first name",
            },
            "last_name": {
                "display_name": "Last Name",
                "type": StringOrNothingType,
                "desc": "The customer's last name",
            },
            "email": {
                "display_name": "Email",
                "type": StringOrNothingType,
                "desc": "The customer's email address",
            },
            "company": {
                "display_name": "Company",
                "type": StringOrNothingType,
                "desc": "The customer's company name",
            },
            "phone": {
                "display_name": "Phone",
                "type": StringOrNothingType,
                "desc": "The customer's phone number",
            },
            "notes": {
                "display_name": "Notes",
                "type": StringOrNothingType,
                "desc": "Admin notes about the customer",
            },
            "customer_group_id": {
                "display_name": "Customer Group ID",
                "type": IntOrNothingType,
                "desc": "ID of the customer group to assign",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! BigCommerce update customer action data provider
public class BigCommerceUpdateCustomerDataProvider inherits BigCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing customer in BigCommerce",
            "type": "BigCommerceUpdateCustomerDataProvider",
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
        return "update";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update an existing customer in BigCommerce";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BigCommerceUpdateCustomerRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BigCommerceCustomerDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doUpdateSingleV3("customers", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BigCommerceDataProvider
