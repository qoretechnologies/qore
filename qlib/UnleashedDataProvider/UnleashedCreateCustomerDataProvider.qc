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

        hash<auto> body = {
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

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: creating customer %y", body.CustomerCode));
        hash<auto> response = doPost("/Customers", body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: created customer %y", response.Guid));
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

} # namespace UnleashedDataProvider
