# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedCreateCustomerDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for creating a customer
public class UnleashedCreateCustomerDataProvider inherits UnleashedDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-customer",
            "desc": "Creates a new customer in Unleashed",
            "type": "UnleashedCreateCustomerDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static UnleashedCreateCustomerRequestDataType RequestType();

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
        return "create-customer";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Creates a new customer in Unleashed";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.CustomerCode.val(), "CustomerCode is required");
        @assert(req.CustomerName.val(), "CustomerName is required");

        hash<auto!> body = {
            "CustomerCode": req.CustomerCode,
            "CustomerName": req.CustomerName,
        };

        # Optional fields
        if (exists req.ContactFirstName) body.ContactFirstName = req.ContactFirstName;
        if (exists req.ContactLastName) body.ContactLastName = req.ContactLastName;
        if (exists req.Email) body.Email = req.Email;
        if (exists req.PhoneNumber) body.PhoneNumber = req.PhoneNumber;
        if (exists req.MobileNumber) body.MobileNumber = req.MobileNumber;
        if (exists req.CreditLimit) body.CreditLimit = req.CreditLimit;
        if (exists req.DiscountRate) body.DiscountRate = req.DiscountRate;
        if (exists req.Taxable) body.Taxable = req.Taxable;
        if (exists req.Currency) body.Currency = req.Currency;
        if (exists req.Addresses) body.Addresses = req.Addresses;

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: creating customer %y", body.CustomerCode));
        hash<auto> response = doPost("/Customers", body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: created customer %y", response.Customer.Guid));
        return response.Customer;
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

#! Currency data type for customer creation
public class UnleashedCustomerCurrencyDataType inherits HashDataType {
    public {
        const Fields = {
            "CurrencyCode": {
                "type": SoftStringType,
                "display_name": "Currency Code",
                "short_desc": "Currency code",
                "desc": "ISO currency code (e.g., NZD, USD, GBP)",
                "required": True,
            },
        };
    }

    constructor() : HashDataType("UnleashedCustomerCurrencyDataType") {
        addQoreFields(Fields);
    }
}

#! Address data type for customer creation
public class UnleashedCustomerAddressDataType inherits HashDataType {
    public {
        const Fields = {
            "AddressType": {
                "type": SoftStringType,
                "display_name": "Address Type",
                "short_desc": "Type of address",
                "desc": "The type of address",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Shipping", "desc": "Shipping address"},
                    <AllowedValueInfo>{"value": "Postal", "desc": "Postal/mailing address"},
                    <AllowedValueInfo>{"value": "Physical", "desc": "Physical location address"},
                ),
            },
            "AddressName": {
                "type": SoftStringType,
                "display_name": "Address Name",
                "short_desc": "Name for this address",
                "desc": "Descriptive name for this address (e.g., 'Main Office', 'Warehouse')",
                "required": True,
            },
            "StreetAddress": {
                "type": SoftStringOrNothingType,
                "display_name": "Street Address",
                "short_desc": "Street address",
                "desc": "Street address line",
            },
            "City": {
                "type": SoftStringOrNothingType,
                "display_name": "City",
                "short_desc": "City",
                "desc": "City name",
            },
            "PostalCode": {
                "type": SoftStringOrNothingType,
                "display_name": "Postal Code",
                "short_desc": "Postal/ZIP code",
                "desc": "Postal or ZIP code",
            },
            "Country": {
                "type": SoftStringOrNothingType,
                "display_name": "Country",
                "short_desc": "Country",
                "desc": "Country name",
            },
        };
    }

    constructor() : HashDataType("UnleashedCustomerAddressDataType") {
        addQoreFields(Fields);
    }
}

#! Customer creation request data type
public class UnleashedCreateCustomerRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "CustomerCode": {
                "type": SoftStringType,
                "display_name": "Customer Code",
                "short_desc": "Unique customer code",
                "desc": "The unique customer code (required, cannot be modified after creation)",
                "required": True,
            },
            "CustomerName": {
                "type": SoftStringType,
                "display_name": "Customer Name",
                "short_desc": "Customer name",
                "desc": "The customer's display name (required)",
                "required": True,
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
            "Currency": {
                "type": new UnleashedCustomerCurrencyDataType(),
                "display_name": "Currency",
                "short_desc": "Customer currency",
                "desc": "The currency for this customer",
            },
            "Addresses": {
                "type": new SoftListDataType(new UnleashedCustomerAddressDataType()),
                "display_name": "Addresses",
                "short_desc": "Customer addresses",
                "desc": "List of addresses for this customer",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("UnleashedCreateCustomerRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnleashedDataProvider
