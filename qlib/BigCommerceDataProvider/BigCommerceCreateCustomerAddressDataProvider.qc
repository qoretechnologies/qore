# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceCreateCustomerAddressDataProvider.qc BigCommerce create customer address action data provider

/*  BigCommerceCreateCustomerAddressDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a customer address
public class BigCommerceCreateCustomerAddressRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "customer_id": {
                "display_name": "Customer ID",
                "short_desc": "The customer to add address to",
                "type": IntType,
                "desc": "ID of the customer to add the address to",
                "required": True,
                "ref_data": "customers",
            },
            "first_name": {
                "display_name": "First Name",
                "short_desc": "Recipient first name",
                "type": StringType,
                "desc": "First name of the address recipient",
                "required": True,
            },
            "last_name": {
                "display_name": "Last Name",
                "short_desc": "Recipient last name",
                "type": StringType,
                "desc": "Last name of the address recipient",
                "required": True,
            },
            "address1": {
                "display_name": "Address Line 1",
                "short_desc": "Street address line 1",
                "type": StringType,
                "desc": "First line of the street address",
                "required": True,
            },
            "city": {
                "display_name": "City",
                "short_desc": "City name",
                "type": StringType,
                "desc": "City name",
                "required": True,
            },
            "state_or_province": {
                "display_name": "State/Province",
                "short_desc": "State or province",
                "type": StringType,
                "desc": "State or province name",
                "required": True,
            },
            "postal_code": {
                "display_name": "Postal Code",
                "short_desc": "Postal or ZIP code",
                "type": StringType,
                "desc": "Postal or ZIP code",
                "required": True,
            },
            "country_code": {
                "display_name": "Country Code",
                "short_desc": "Country code (ISO 3166-1)",
                "type": StringType,
                "desc": "ISO 3166-1 alpha-2 country code (e.g., US, GB, DE)",
                "required": True,
            },
            "address2": {
                "display_name": "Address Line 2",
                "short_desc": "Street address line 2",
                "type": StringOrNothingType,
                "desc": "Second line of the street address",
            },
            "company": {
                "display_name": "Company",
                "short_desc": "Company name",
                "type": StringOrNothingType,
                "desc": "Company name for the address",
            },
            "phone": {
                "display_name": "Phone",
                "short_desc": "Phone number",
                "type": StringOrNothingType,
                "desc": "Phone number for the address",
            },
            "address_type": {
                "display_name": "Address Type",
                "short_desc": "Type of address",
                "type": StringOrNothingType,
                "desc": "Type of address",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "residential",
                        "desc": "Residential address"},
                    <AllowedValueInfo>{"value": "commercial",
                        "desc": "Commercial address"},
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! BigCommerce create customer address action data provider
public class BigCommerceCreateCustomerAddressDataProvider inherits BigCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new customer address in BigCommerce",
            "type": "BigCommerceCreateCustomerAddressDataProvider",
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
        return "Create a new customer address in BigCommerce";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BigCommerceCreateCustomerAddressRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BigCommerceCustomerAddressDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doCreateSingleV3("customers/addresses", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BigCommerceDataProvider
