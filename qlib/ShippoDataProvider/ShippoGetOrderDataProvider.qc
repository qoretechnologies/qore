# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoGetOrderDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShippoDataProvider module
public namespace ShippoDataProvider {

#! Data provider for getting a single order
public class ShippoGetOrderDataProvider inherits ShippoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-order",
            "desc": "Retrieves a single order by ID",
            "type": "ShippoGetOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ShippoGetOrderRequestDataType RequestType();

        #! Response type
        static ShippoOrderDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(ShippoRestClient::ShippoRestClient rest) : ShippoDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ShippoRestClient::ShippoRestConnection conn) : ShippoDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : ShippoDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "get-order";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Retrieves a single order by ID";
    }

    #! Makes a request and returns the response
    /** @param req the request
        @param request_options the request options (ignored)

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string order_id = req.order_id;
        @assert(order_id.val(), "order_id is required");

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: getting order %y", order_id));
        hash<auto> response = doGet("/orders/" + order_id);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: retrieved order %y", response.object_id));
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

#! Request data type for getting an order
public class ShippoGetOrderRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "order_id": {
                "type": StringType,
                "display_name": "Order ID",
                "short_desc": "The order object ID",
                "desc": "The unique identifier of the order to retrieve",
                "required": True,
                "ref_data": "orders",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoGetOrderRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace ShippoDataProvider
