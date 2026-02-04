# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationAddTagToOrderDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShipStationDataProvider module
public namespace ShipStationDataProvider {

#! Request type for adding a tag to an order
public class ShipStationAddTagToOrderRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "orderId": {
                "type": IntType,
                "display_name": "Order ID",
                "short_desc": "The order ID",
                "desc": "The ShipStation order ID to add the tag to",
                "required": True,
            },
            "tagId": {
                "type": IntType,
                "display_name": "Tag ID",
                "short_desc": "The tag ID to add",
                "desc": "The ID of the tag to add to the order",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for add tag operation
public class ShipStationAddTagResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "success": {
                "type": SoftBoolType,
                "display_name": "Success",
                "short_desc": "Whether the operation was successful",
                "desc": "Indicates whether the tag was successfully added",
            },
            "message": {
                "type": SoftStringOrNothingType,
                "display_name": "Message",
                "short_desc": "Response message",
                "desc": "A message describing the result",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for adding a tag to an order
public class ShipStationAddTagToOrderDataProvider inherits ShipStationDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "add-tag-to-order",
            "desc": "Add a tag to a ShipStation order",
            "type": "ShipStationAddTagToOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ShipStationAddTagToOrderRequestDataType RequestType();

        #! Response type
        static ShipStationAddTagResponseDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(ShipStationRestClient::ShipStationRestClient rest)
            : ShipStationDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ShipStationRestClient::ShipStationRestConnection conn) : ShipStationDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "add-tag-to-order";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Add a tag to a ShipStation order";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: adding tag %d to order %d", req.tagId, req.orderId));

        hash<auto> response = doPost("/orders/addtag", req);
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

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ShipStationDataProvider
