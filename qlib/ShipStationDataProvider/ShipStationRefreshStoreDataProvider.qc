# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationRefreshStoreDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for refreshing a store
public class ShipStationRefreshStoreRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "storeId": {
                "type": IntType,
                "display_name": "Store ID",
                "short_desc": "The store ID to refresh",
                "desc": "The unique ShipStation store ID to trigger an order refresh for",
                "required": True,
                "preselected": True,
            },
            "refreshDate": {
                "type": DateOrNothingType,
                "display_name": "Refresh Date",
                "short_desc": "Start date for order refresh",
                "desc": "The date from which to start the refresh. "
                    "If not provided, refreshes recent orders.",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for refreshing a store
public class ShipStationRefreshStoreResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "success": {
                "type": SoftBoolType,
                "display_name": "Success",
                "short_desc": "Whether the operation was successful",
                "desc": "Indicates whether the refresh was successfully initiated",
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

#! Data provider for refreshing a store's orders
public class ShipStationRefreshStoreDataProvider inherits ShipStationDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "refresh-store",
            "desc": "Trigger an order refresh for a ShipStation store",
            "type": "ShipStationRefreshStoreDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ShipStationRefreshStoreRequestDataType RequestType();

        #! Response type
        static ShipStationRefreshStoreResponseDataType ResponseType();
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
        return "refresh-store";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Trigger an order refresh for a ShipStation store";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int storeId = req.storeId;
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: refreshing store %d", storeId));

        hash<auto> response = doPost("/stores/refreshstore", req);
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
