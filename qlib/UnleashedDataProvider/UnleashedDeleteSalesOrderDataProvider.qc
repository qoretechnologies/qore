# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedDeleteSalesOrderDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for deleting a sales order
public class UnleashedDeleteSalesOrderDataProvider inherits UnleashedDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-sales-order",
            "desc": "Deletes a sales order from Unleashed",
            "type": "UnleashedDeleteSalesOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static UnleashedDeleteSalesOrderRequestDataType RequestType();

        #! Response type
        static HashDataType ResponseType("UnleashedDeleteSalesOrderResponse");
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
        return "delete-sales-order";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Deletes a sales order from Unleashed";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string guid = req.guid;
        @assert(guid.val(), "guid is required");

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: deleting sales order %y", guid));
        hash<auto> response = doDelete("/SalesOrders/" + guid);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: deleted sales order %y", guid));
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

#! Request data type for deleting a sales order
public class UnleashedDeleteSalesOrderRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "guid": {
                "type": StringType,
                "display_name": "Sales Order GUID",
                "short_desc": "The sales order GUID",
                "desc": "The unique identifier (GUID) of the sales order to delete",
                "required": True,
                "ref_data": "salesorders",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("UnleashedDeleteSalesOrderRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnleashedDataProvider
