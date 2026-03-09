# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareUpdateCustomerDataProvider.qc Square update customer action data provider

/*  SquareUpdateCustomerDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a customer
public class SquareUpdateCustomerRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "customer_id": {
                "type": StringType,
                "display_name": "Customer ID",
                "short_desc": "The customer ID to update",
                "desc": "The unique identifier of the customer to update",
                "required": True,
            },
            "given_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The customer's first name",
                "desc": "The new given (first) name of the customer",
            },
            "family_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The customer's last name",
                "desc": "The new family (last) name of the customer",
            },
            "email_address": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The customer's email address",
                "desc": "The new email address of the customer",
            },
            "phone_number": {
                "type": StringOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "The customer's phone number",
                "desc": "The new phone number of the customer",
            },
            "company_name": {
                "type": StringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "The customer's company name",
                "desc": "The new company name of the customer",
            },
            "note": {
                "type": StringOrNothingType,
                "display_name": "Note",
                "short_desc": "Notes about the customer",
                "desc": "The new note about the customer",
            },
            "version": {
                "type": IntOrNothingType,
                "display_name": "Version",
                "short_desc": "The customer version for concurrency",
                "desc": "The current version of the customer profile (for optimistic concurrency)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Square update customer action data provider
public class SquareUpdateCustomerDataProvider inherits SquareDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing customer in Square",
            "type": "SquareUpdateCustomerDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing customer in Square";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquareUpdateCustomerRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new SquareCustomerDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string customer_id = remove req.customer_id;
        return doPut("customers/" + customer_id, req).customer;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider
