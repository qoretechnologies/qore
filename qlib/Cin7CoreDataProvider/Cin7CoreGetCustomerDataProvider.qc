# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreGetCustomerDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for getting a single customer
public class Cin7CoreGetCustomerDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-customer",
            "desc": "Retrieves a single customer by ID",
            "type": "Cin7CoreGetCustomerDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static Cin7CoreGetCustomerRequestDataType RequestType();

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
        return "get-customer";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Retrieves a single customer by ID";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string customer_id = req.ID;
        @assert(customer_id.val(), "ID is required");

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: getting customer %y", customer_id));
        hash<auto> response = doGet("/Customer", {"ID": customer_id});
        # Cin7Core returns a CustomerList array even for single ID lookup
        if (response.CustomerList && response.CustomerList.size() > 0) {
            @debug(log(LoggerLevel::DEBUG, "doRequestImpl: retrieved customer %y",
                response.CustomerList[0].Name));
            return response.CustomerList[0];
        }
        throw "CIN7CORE-NOT-FOUND", sprintf("Customer %y not found", customer_id);
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

#! Request data type for getting a customer
public class Cin7CoreGetCustomerRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "ID": {
                "type": StringType,
                "display_name": "Customer ID",
                "short_desc": "The customer ID",
                "desc": "The unique identifier (GUID) of the customer to retrieve",
                "required": True,
                "ref_data": "customers",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreGetCustomerRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
