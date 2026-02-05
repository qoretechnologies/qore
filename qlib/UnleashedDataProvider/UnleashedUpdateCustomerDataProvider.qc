# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedUpdateCustomerDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnleashedDataProvider module
public namespace UnleashedDataProvider {

#! Data provider for updating a customer
public class UnleashedUpdateCustomerDataProvider inherits UnleashedDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-customer",
            "desc": "Updates an existing customer in Unleashed",
            "type": "UnleashedUpdateCustomerDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static UnleashedUpdateCustomerRequestDataType RequestType();

        #! Response type
        static UnleashedCustomerDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(UnleashedRestClient::UnleashedRestClient rest) : UnleashedDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(UnleashedRestClient::UnleashedRestConnection conn) : UnleashedDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : UnleashedDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "update-customer";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Updates an existing customer in Unleashed (note: Unleashed uses POST for updates)";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string guid = req.guid;
        @assert(guid.val(), "guid is required");

        hash<auto> body = {};

        # Optional fields
        if (exists req.CustomerName) body.CustomerName = req.CustomerName;
        if (exists req.ContactFirstName) body.ContactFirstName = req.ContactFirstName;
        if (exists req.ContactLastName) body.ContactLastName = req.ContactLastName;
        if (exists req.Email) body.Email = req.Email;
        if (exists req.PhoneNumber) body.PhoneNumber = req.PhoneNumber;
        if (exists req.MobileNumber) body.MobileNumber = req.MobileNumber;
        if (exists req.CreditLimit) body.CreditLimit = req.CreditLimit;
        if (exists req.DiscountRate) body.DiscountRate = req.DiscountRate;
        if (exists req.Taxable) body.Taxable = req.Taxable;
        if (exists req.StopCredit) body.StopCredit = req.StopCredit;

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updating customer %y", guid));
        # Unleashed uses POST for updates
        hash<auto> response = doPost("/Customers/" + guid, body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updated customer %y", response.Guid));
        return response;
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Request data type for updating a customer
public class UnleashedUpdateCustomerRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "guid": {
                "type": StringType,
                "display_name": "Customer GUID",
                "short_desc": "The customer GUID",
                "desc": "The unique identifier (GUID) of the customer to update",
                "required": True,
                "ref_data": "customers",
            },
            "CustomerName": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer Name",
                "short_desc": "Customer name",
                "desc": "The customer's display name",
            },
            "ContactFirstName": {
                "type": SoftStringOrNothingType,
                "display_name": "Contact First Name",
                "short_desc": "Primary contact first name",
                "desc": "First name of the primary contact",
            },
            "ContactLastName": {
                "type": SoftStringOrNothingType,
                "display_name": "Contact Last Name",
                "short_desc": "Primary contact last name",
                "desc": "Last name of the primary contact",
            },
            "Email": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Primary email address",
                "desc": "Primary email address for the customer",
            },
            "PhoneNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "Phone number",
                "desc": "Customer's phone number",
            },
            "MobileNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Mobile Number",
                "short_desc": "Mobile phone number",
                "desc": "Customer's mobile phone number",
            },
            "CreditLimit": {
                "type": SoftFloatOrNothingType,
                "display_name": "Credit Limit",
                "short_desc": "Customer credit limit",
                "desc": "The customer's credit limit",
            },
            "DiscountRate": {
                "type": SoftFloatOrNothingType,
                "display_name": "Discount Rate",
                "short_desc": "Default discount rate",
                "desc": "The customer's default discount rate",
            },
            "Taxable": {
                "type": SoftBoolOrNothingType,
                "display_name": "Taxable",
                "short_desc": "Is customer taxable",
                "desc": "Whether the customer is taxable",
            },
            "StopCredit": {
                "type": SoftBoolOrNothingType,
                "display_name": "Stop Credit",
                "short_desc": "Credit stop status",
                "desc": "Whether credit is stopped for this customer",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("UnleashedUpdateCustomerRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnleashedDataProvider
