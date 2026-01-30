# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationAssignUserToOrderDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for assigning a user to an order
public class ShipStationAssignUserToOrderRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "orderIds": {
                "type": SoftAutoListType,
                "display_name": "Order IDs",
                "short_desc": "The order IDs to assign",
                "desc": "Array of order IDs to assign the user to",
                "required": True,
            },
            "userId": {
                "type": SoftStringType,
                "display_name": "User ID",
                "short_desc": "The user ID to assign",
                "desc": "The user ID (GUID) to assign to the orders",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for assign user operation
public class ShipStationAssignUserResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "success": {
                "type": SoftBoolType,
                "display_name": "Success",
                "short_desc": "Whether the operation was successful",
                "desc": "Indicates whether the user was successfully assigned",
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

#! Data provider for assigning a user to orders
public class ShipStationAssignUserToOrderDataProvider inherits ShipStationDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "assign-user-to-order",
            "desc": "Assign a user to one or more ShipStation orders",
            "type": "ShipStationAssignUserToOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ShipStationAssignUserToOrderRequestDataType RequestType();

        #! Response type
        static ShipStationAssignUserResponseDataType ResponseType();
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
        return "assign-user-to-order";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Assign a user to one or more ShipStation orders";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: assigning user %s to orders %y", req.userId, req.orderIds));

        hash<auto> response = doPost("/orders/assignuser", req);
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
