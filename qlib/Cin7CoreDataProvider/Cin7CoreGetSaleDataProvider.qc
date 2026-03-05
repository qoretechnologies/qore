# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreGetSaleDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for getting a single sale
public class Cin7CoreGetSaleDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-sale",
            "desc": "Retrieves a single sale/order by ID",
            "type": "Cin7CoreGetSaleDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static Cin7CoreGetSaleRequestDataType RequestType();

        #! Response type
        static Cin7CoreSaleDataType ResponseType();
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
        return "get-sale";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Retrieves a single sale/order by ID";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string sale_id = req.ID;
        @assert(sale_id.val(), "ID is required");

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: getting sale %y", sale_id));
        hash<auto> response = doGet("/Sale", {"ID": sale_id});
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: retrieved sale %y", response.SaleOrderNumber ?? response.ID));
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

#! Request data type for getting a sale
public class Cin7CoreGetSaleRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "ID": {
                "type": StringType,
                "display_name": "Sale ID",
                "short_desc": "The sale ID",
                "desc": "The unique identifier (GUID) of the sale to retrieve",
                "required": True,
                "ref_data": "sales",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreGetSaleRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
