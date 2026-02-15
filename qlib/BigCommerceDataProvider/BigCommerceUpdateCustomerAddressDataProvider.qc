# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceUpdateCustomerAddressDataProvider.qc BigCommerce update customer address action data provider

/*  BigCommerceUpdateCustomerAddressDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a customer address
public class BigCommerceUpdateCustomerAddressRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Address ID",
                "type": IntType,
                "desc": "The ID of the address to update",
                "required": True,
            },
            "first_name": {
                "display_name": "First Name",
                "type": StringOrNothingType,
                "desc": "First name of the address recipient",
            },
            "last_name": {
                "display_name": "Last Name",
                "type": StringOrNothingType,
                "desc": "Last name of the address recipient",
            },
            "address1": {
                "display_name": "Address Line 1",
                "type": StringOrNothingType,
                "desc": "First line of the street address",
            },
            "address2": {
                "display_name": "Address Line 2",
                "type": StringOrNothingType,
                "desc": "Second line of the street address",
            },
            "city": {
                "display_name": "City",
                "type": StringOrNothingType,
                "desc": "City name",
            },
            "state_or_province": {
                "display_name": "State/Province",
                "type": StringOrNothingType,
                "desc": "State or province name",
            },
            "postal_code": {
                "display_name": "Postal Code",
                "type": StringOrNothingType,
                "desc": "Postal or ZIP code",
            },
            "country_code": {
                "display_name": "Country Code",
                "type": StringOrNothingType,
                "desc": "ISO 3166-1 alpha-2 country code (e.g., US, GB, DE)",
            },
            "company": {
                "display_name": "Company",
                "type": StringOrNothingType,
                "desc": "Company name for the address",
            },
            "phone": {
                "display_name": "Phone",
                "type": StringOrNothingType,
                "desc": "Phone number for the address",
            },
            "address_type": {
                "display_name": "Address Type",
                "type": StringOrNothingType,
                "desc": "Type of address (residential or commercial)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! BigCommerce update customer address action data provider
public class BigCommerceUpdateCustomerAddressDataProvider inherits BigCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing customer address in BigCommerce",
            "type": "BigCommerceUpdateCustomerAddressDataProvider",
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
        return "Update an existing customer address in BigCommerce";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BigCommerceUpdateCustomerAddressRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BigCommerceCustomerAddressDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doUpdateSingleV3("customers/addresses", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BigCommerceDataProvider
