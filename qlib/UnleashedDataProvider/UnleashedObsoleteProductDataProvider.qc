# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedObsoleteProductDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for marking a product as obsolete
public class UnleashedObsoleteProductDataProvider inherits UnleashedDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "obsolete-product",
            "desc": "Marks a product as obsolete in Unleashed",
            "type": "UnleashedObsoleteProductDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static UnleashedObsoleteProductRequestDataType RequestType();

        #! Response type
        static UnleashedProductDataType ResponseType();
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
        return "obsolete-product";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Marks a product as obsolete in Unleashed";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string guid = req.guid;
        @assert(guid.val(), "guid is required");

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: marking product %y as obsolete", guid));
        hash<auto> response = doPost("/Products/Obsolete/" + guid);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: product %y marked obsolete", response.Guid));
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

#! Request data type for marking a product obsolete
public class UnleashedObsoleteProductRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "guid": {
                "type": StringType,
                "display_name": "Product GUID",
                "short_desc": "The product GUID",
                "desc": "The unique identifier (GUID) of the product to mark as obsolete",
                "required": True,
                "ref_data": "products",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("UnleashedObsoleteProductRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnleashedDataProvider
