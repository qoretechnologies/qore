# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationUpdateWarehouseDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a warehouse
public class ShipStationUpdateWarehouseRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "warehouseId": {
                "type": IntType,
                "display_name": "Warehouse ID",
                "short_desc": "The warehouse ID to update",
                "desc": "The unique ShipStation warehouse ID to update",
                "required": True,
            },
            "warehouseName": {
                "type": StringType,
                "display_name": "Warehouse Name",
                "short_desc": "Name for the warehouse",
                "desc": "A descriptive name for the warehouse",
                "required": True,
            },
            "originAddress": {
                "type": new ShipStationAddressDataType(),
                "display_name": "Origin Address",
                "short_desc": "Warehouse address",
                "desc": "The ship-from address for this warehouse",
                "required": True,
            },
            "returnAddress": {
                "type": new ShipStationAddressDataType(),
                "display_name": "Return Address",
                "short_desc": "Return address",
                "desc": "The return address for shipments from this warehouse",
            },
            "isDefault": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Default",
                "short_desc": "Set as default warehouse",
                "desc": "Whether this warehouse should be the default for new orders",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for updating a warehouse
public class ShipStationUpdateWarehouseDataProvider inherits ShipStationDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-warehouse",
            "desc": "Update a ShipStation warehouse (ship-from location)",
            "type": "ShipStationUpdateWarehouseDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ShipStationUpdateWarehouseRequestDataType RequestType();

        #! Response type
        static ShipStationWarehouseDataType ResponseType();
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
        return "update-warehouse";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update a ShipStation warehouse (ship-from location)";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int warehouseId = req.warehouseId;
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updating warehouse %d", warehouseId));

        hash<auto> body = req - "warehouseId";
        hash<auto> response = doPut(sprintf("/warehouses/%d", warehouseId), body);
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
