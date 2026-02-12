# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareCreateCustomerDataProvider.qc Square create customer action data provider

/*  SquareCreateCustomerDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {

#! Request type for creating a customer
public class SquareCreateCustomerRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "given_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The customer's first name",
                "desc": "The given (first) name of the customer",
                "example_value": "John",
            },
            "family_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The customer's last name",
                "desc": "The family (last) name of the customer",
                "example_value": "Doe",
            },
            "company_name": {
                "type": StringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "The customer's company name",
                "desc": "The company name associated with the customer",
            },
            "email_address": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The customer's email address",
                "desc": "The email address of the customer",
                "example_value": "john@example.com",
            },
            "phone_number": {
                "type": StringOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "The customer's phone number",
                "desc": "The phone number of the customer",
            },
            "address": {
                "type": AutoHashOrNothingType,
                "display_name": "Address",
                "short_desc": "The customer's address",
                "desc": "The physical address (hash with address_line_1, address_line_2, "
                    "locality, administrative_district_level_1, postal_code, country)",
            },
            "note": {
                "type": StringOrNothingType,
                "display_name": "Note",
                "short_desc": "Notes about the customer",
                "desc": "A note about the customer",
            },
            "birthday": {
                "type": StringOrNothingType,
                "display_name": "Birthday",
                "short_desc": "The customer's birthday",
                "desc": "The customer's birthday in YYYY-MM-DD format",
            },
            "reference_id": {
                "type": StringOrNothingType,
                "display_name": "Reference ID",
                "short_desc": "External reference ID",
                "desc": "An optional reference ID from an external system",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Square create customer action data provider
public class SquareCreateCustomerDataProvider inherits SquareDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new customer in Square",
            "type": "SquareCreateCustomerDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new customer in Square";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquareCreateCustomerRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new SquareCustomerDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req.idempotency_key = generateIdempotencyKey();
        return doPost("customers", req).customer;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider
