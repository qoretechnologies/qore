# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeCreateCustomerDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ChargebeeDataProvider module
public namespace ChargebeeDataProvider {

#! Request type for creating a customer
public class ChargebeeCreateCustomerRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "first_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "Customer's first name",
                "desc": "The first name of the customer",
            },
            "last_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Customer's last name",
                "desc": "The last name of the customer",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "Customer's email address",
                "desc": "The email address of the customer",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Customer's phone number",
                "desc": "The phone number of the customer",
            },
            "company": {
                "type": StringOrNothingType,
                "display_name": "Company",
                "short_desc": "Customer's company name",
                "desc": "The company name of the customer",
            },
            "auto_collection": {
                "type": StringOrNothingType,
                "display_name": "Auto Collection",
                "short_desc": "Auto-collect payments",
                "desc": "Whether to automatically collect charges. When set to `on`, charges will be "
                    "automatically collected; when `off`, invoices will be created as pending",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "on", "desc": "Automatically collect charges"},
                    <AllowedValueInfo>{"value": "off", "desc": "Do not automatically collect charges"},
                ),
            },
            "net_term_days": {
                "type": IntOrNothingType,
                "display_name": "Net Term Days",
                "short_desc": "Payment due period in days",
                "desc": "The number of days within which the customer has to make payment for invoices",
            },
            "vat_number": {
                "type": StringOrNothingType,
                "display_name": "VAT Number",
                "short_desc": "Customer's VAT number",
                "desc": "The VAT/tax registration number of the customer",
            },
            "locale": {
                "type": StringOrNothingType,
                "display_name": "Locale",
                "short_desc": "Customer's locale",
                "desc": "The locale of the customer (e.g., `en-US`, `fr-FR`)",
            },
            "meta_data": {
                "type": AutoHashOrNothingType,
                "display_name": "Metadata",
                "short_desc": "Custom metadata",
                "desc": "A set of key-value pairs for storing additional information about the customer",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a new customer
public class ChargebeeCreateCustomerDataProvider inherits ChargebeeDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-customer",
            "desc": "Create a new customer in Chargebee",
            "type": "ChargebeeCreateCustomerDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ChargebeeCreateCustomerRequestDataType RequestType();

        #! Response type
        static ChargebeeCustomerDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(ChargebeeRestClient::ChargebeeRestClient rest)
            : ChargebeeDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ChargebeeRestClient::ChargebeeRestConnection conn) : ChargebeeDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "create-customer";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new customer in Chargebee";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: creating customer"));

        hash<auto> response = doPost("/customers", req);
        return response.customer;
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ChargebeeDataProvider
