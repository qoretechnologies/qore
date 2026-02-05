# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreGetSupplierDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for getting a single supplier
public class Cin7CoreGetSupplierDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-supplier",
            "desc": "Retrieves a single supplier by ID",
            "type": "Cin7CoreGetSupplierDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static Cin7CoreGetSupplierRequestDataType RequestType();

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
        return "get-supplier";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Retrieves a single supplier by ID";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string supplier_id = req.ID;
        @assert(supplier_id.val(), "ID is required");

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: getting supplier %y", supplier_id));
        hash<auto> response = doGet("/Supplier", {"ID": supplier_id});
        # Cin7Core returns a SupplierList array even for single ID lookup
        if (response.SupplierList && response.SupplierList.size() > 0) {
            @debug(log(LoggerLevel::DEBUG, "doRequestImpl: retrieved supplier %y",
                response.SupplierList[0].Name));
            return response.SupplierList[0];
        }
        throw "CIN7CORE-NOT-FOUND", sprintf("Supplier %y not found", supplier_id);
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

#! Request data type for getting a supplier
public class Cin7CoreGetSupplierRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "ID": {
                "type": StringType,
                "display_name": "Supplier ID",
                "short_desc": "The supplier ID",
                "desc": "The unique identifier (GUID) of the supplier to retrieve",
                "required": True,
                "ref_data": "suppliers",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreGetSupplierRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
