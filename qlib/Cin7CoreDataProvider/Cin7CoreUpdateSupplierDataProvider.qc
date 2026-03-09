# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreUpdateSupplierDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for updating a supplier
public class Cin7CoreUpdateSupplierDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-supplier",
            "desc": "Updates an existing supplier in Cin7Core",
            "type": "Cin7CoreUpdateSupplierDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static Cin7CoreUpdateSupplierRequestDataType RequestType();

        #! Response type
        static Cin7CoreSupplierDataType ResponseType();
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
        return "update-supplier";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Updates an existing supplier in Cin7Core";
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
        if (exists req.AccountPayable) body.AccountPayable = req.AccountPayable;
        if (exists req.TaxRule) body.TaxRule = req.TaxRule;
        if (exists req.Discount) body.Discount = req.Discount;
        if (exists req.Comments) body.Comments = req.Comments;
        if (exists req.TaxNumber) body.TaxNumber = req.TaxNumber;
        if (exists req.Contacts) body.Contacts = req.Contacts;
        if (exists req.Addresses) body.Addresses = req.Addresses;
        if (exists req.Tags) body.Tags = req.Tags;

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updating supplier %y", body.ID));
        hash<auto> response = doPut("/Supplier", body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updated supplier %y", response.ID));
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

#! Request data type for updating a supplier
public class Cin7CoreUpdateSupplierRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "ID": {
                "type": StringType,
                "display_name": "Supplier ID",
                "short_desc": "The supplier ID to update",
                "desc": "The unique identifier (GUID) of the supplier to update",
                "required": True,
                "ref_data": "suppliers",
            },
            "Name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Supplier name",
                "desc": "The supplier's company or individual name",
            },
            "Status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Supplier status",
                "desc": "The supplier's status (Active, Deprecated, etc.)",
            },
            "Currency": {
                "type": StringOrNothingType,
                "display_name": "Currency",
                "short_desc": "Default currency",
                "desc": "The default currency code for this supplier (e.g., USD, EUR)",
            },
            "PaymentTerm": {
                "type": StringOrNothingType,
                "display_name": "Payment Term",
                "short_desc": "Payment terms",
                "desc": "The payment terms for this supplier",
            },
            "AccountPayable": {
                "type": StringOrNothingType,
                "display_name": "Account Payable",
                "short_desc": "Accounts payable account",
                "desc": "The accounts payable account for this supplier",
            },
            "TaxRule": {
                "type": StringOrNothingType,
                "display_name": "Tax Rule",
                "short_desc": "Tax rule",
                "desc": "The tax rule to apply to this supplier",
            },
            "Discount": {
                "type": FloatOrNothingType,
                "display_name": "Discount",
                "short_desc": "Default discount",
                "desc": "The default discount percentage for this supplier",
            },
            "Comments": {
                "type": StringOrNothingType,
                "display_name": "Comments",
                "short_desc": "Comments",
                "desc": "Additional comments or notes about the supplier",
            },
            "TaxNumber": {
                "type": StringOrNothingType,
                "display_name": "Tax Number",
                "short_desc": "Tax identification number",
                "desc": "The supplier's tax identification number",
            },
            "Contacts": {
                "type": new ListDataType("Contacts", new Cin7CoreContactDataType(), True),
                "display_name": "Contacts",
                "short_desc": "Contact list",
                "desc": "List of contacts for this supplier",
            },
            "Addresses": {
                "type": new ListDataType("Addresses", new Cin7CoreAddressDataType(), True),
                "display_name": "Addresses",
                "short_desc": "Address list",
                "desc": "List of addresses for this supplier",
            },
            "Tags": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Tags",
                "short_desc": "Tags",
                "desc": "Tags associated with this supplier",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreUpdateSupplierRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
