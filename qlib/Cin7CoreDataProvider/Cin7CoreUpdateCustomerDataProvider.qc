# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreUpdateCustomerDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the Cin7CoreDataProvider module
public namespace Cin7CoreDataProvider {

#! Data provider for updating a customer
public class Cin7CoreUpdateCustomerDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-customer",
            "desc": "Updates an existing customer in Cin7Core",
            "type": "Cin7CoreUpdateCustomerDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static Cin7CoreUpdateCustomerRequestDataType RequestType();

        #! Response type
        static Cin7CoreCustomerDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(Cin7CoreRestClient::Cin7CoreRestClient rest) : Cin7CoreDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(Cin7CoreRestClient::Cin7CoreRestConnection conn) : Cin7CoreDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : Cin7CoreDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "update-customer";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Updates an existing customer in Cin7Core";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.ID.val(), "ID is required");

        hash<auto> body = {};

        # Required fields
        body.ID = req.ID;

        # Optional fields - only include if provided
        if (exists req.Name) body.Name = req.Name;
        if (exists req.Status) body.Status = req.Status;
        if (exists req.Currency) body.Currency = req.Currency;
        if (exists req.PaymentTerm) body.PaymentTerm = req.PaymentTerm;
        if (exists req.AccountReceivable) body.AccountReceivable = req.AccountReceivable;
        if (exists req.RevenueAccount) body.RevenueAccount = req.RevenueAccount;
        if (exists req.TaxRule) body.TaxRule = req.TaxRule;
        if (exists req.Discount) body.Discount = req.Discount;
        if (exists req.Comments) body.Comments = req.Comments;
        if (exists req.TaxNumber) body.TaxNumber = req.TaxNumber;
        if (exists req.CreditLimit) body.CreditLimit = req.CreditLimit;
        if (exists req.Contacts) body.Contacts = req.Contacts;
        if (exists req.Addresses) body.Addresses = req.Addresses;
        if (exists req.Tags) body.Tags = req.Tags;

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updating customer %y", body.ID));
        hash<auto> response = doPut("/Customer", body);
        # API returns CustomerList array, extract the updated customer
        hash<auto> customer = response.CustomerList[0] ?? response;
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updated customer %y", customer.ID));
        return customer;
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
public class Cin7CoreUpdateCustomerRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "ID": {
                "type": StringType,
                "display_name": "Customer ID",
                "short_desc": "The customer ID",
                "desc": "The unique identifier (GUID) of the customer to update (required)",
                "required": True,
                "ref_data": "customers",
            },
            "Name": {
                "type": SoftStringOrNothingType,
                "display_name": "Name",
                "short_desc": "Customer name",
                "desc": "The customer's name or company name",
            },
            "Status": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Customer status",
                "desc": "Active or inactive status",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Active", "desc": "Active customer"},
                    <AllowedValueInfo>{"value": "Deprecated", "desc": "Deprecated/inactive customer"},
                ),
            },
            "Currency": {
                "type": SoftStringOrNothingType,
                "display_name": "Currency",
                "short_desc": "Default currency",
                "desc": "Three-letter ISO currency code",
            },
            "PaymentTerm": {
                "type": SoftStringOrNothingType,
                "display_name": "Payment Term",
                "short_desc": "Payment terms",
                "desc": "The payment terms for this customer",
            },
            "AccountReceivable": {
                "type": SoftStringOrNothingType,
                "display_name": "Account Receivable",
                "short_desc": "AR account",
                "desc": "The account receivable account code",
            },
            "RevenueAccount": {
                "type": SoftStringOrNothingType,
                "display_name": "Revenue Account",
                "short_desc": "Revenue account",
                "desc": "The revenue account code",
            },
            "TaxRule": {
                "type": SoftStringOrNothingType,
                "display_name": "Tax Rule",
                "short_desc": "Tax rule",
                "desc": "The tax rule to apply",
            },
            "Discount": {
                "type": SoftFloatOrNothingType,
                "display_name": "Discount",
                "short_desc": "Default discount %",
                "desc": "Default discount percentage",
            },
            "Comments": {
                "type": SoftStringOrNothingType,
                "display_name": "Comments",
                "short_desc": "Notes",
                "desc": "Additional notes about the customer",
            },
            "TaxNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Tax Number",
                "short_desc": "Tax ID",
                "desc": "The customer's tax identification number",
            },
            "CreditLimit": {
                "type": SoftFloatOrNothingType,
                "display_name": "Credit Limit",
                "short_desc": "Credit limit",
                "desc": "The customer's credit limit",
            },
            "Contacts": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Contacts",
                "short_desc": "Contact list",
                "desc": "List of contacts for this customer",
            },
            "Addresses": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Addresses",
                "short_desc": "Address list",
                "desc": "List of addresses for this customer",
            },
            "Tags": {
                "type": SoftStringOrNothingType,
                "display_name": "Tags",
                "short_desc": "Customer tags",
                "desc": "Comma-separated tags for the customer",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreUpdateCustomerRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
