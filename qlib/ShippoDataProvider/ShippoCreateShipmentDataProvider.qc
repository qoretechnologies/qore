# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoCreateShipmentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for creating shipments
public class ShippoCreateShipmentDataProvider inherits ShippoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-shipment",
            "desc": "Creates a new shipment in Shippo to get shipping rates",
            "type": "ShippoCreateShipmentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ShippoCreateShipmentRequestDataType RequestType();

        #! Response type
        static ShippoShipmentDataType ResponseType();
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
        return "create-shipment";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Creates a new shipment in Shippo to get shipping rates";
    }

    #! Makes a request and returns the response
    /** @param req the request
        @param request_options the request options (ignored)

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {};

        # Required fields
        body.address_from = req.address_from;
        body.address_to = req.address_to;
        body.parcels = req.parcels;

        # Optional fields
        if (exists req.address_return) {
            body.address_return = req.address_return;
        }
        if (exists req.shipment_date) {
            body.shipment_date = req.shipment_date;
        }
        if (exists req.extra) {
            body.extra = req.extra;
        }
        if (exists req.customs_declaration) {
            body.customs_declaration = req.customs_declaration;
        }
        if (exists req.metadata) {
            body.metadata = req.metadata;
        }
        if (exists req.async) {
            body.async = req.async;
        }

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: creating shipment with body: %y", body));
        hash<auto> response = doPost("/shipments", body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: created shipment %y", response.object_id));
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

#! Request data type for creating shipments
public class ShippoCreateShipmentRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "address_from": {
                "type": new ShippoAddressInputDataType(),
                "display_name": "From Address",
                "short_desc": "Sender address",
                "desc": "The origin/sender address for the shipment",
                "required": True,
            },
            "address_to": {
                "type": new ShippoAddressInputDataType(),
                "display_name": "To Address",
                "short_desc": "Recipient address",
                "desc": "The destination/recipient address for the shipment",
                "required": True,
            },
            "parcels": {
                "type": SoftAutoListType,
                "display_name": "Parcels",
                "short_desc": "Package details",
                "desc": "Array of parcel objects describing the packages to ship",
                "required": True,
            },
            "address_return": {
                "type": AutoHashOrNothingType,
                "display_name": "Return Address",
                "short_desc": "Return address",
                "desc": "The return address (defaults to address_from if not specified)",
            },
            "shipment_date": {
                "type": SoftStringOrNothingType,
                "display_name": "Shipment Date",
                "short_desc": "Expected shipment date",
                "desc": "The date the shipment will be sent (ISO 8601 format). Defaults to current date.",
            },
            "extra": {
                "type": AutoHashOrNothingType,
                "display_name": "Extra Options",
                "short_desc": "Additional options",
                "desc": "Extra shipment options like insurance, signature confirmation, etc.",
            },
            "customs_declaration": {
                "type": SoftStringOrNothingType,
                "display_name": "Customs Declaration",
                "short_desc": "Customs declaration ID",
                "desc": "Object ID of the customs declaration for international shipments",
            },
            "metadata": {
                "type": SoftStringOrNothingType,
                "display_name": "Metadata",
                "short_desc": "Custom metadata",
                "desc": "A string of up to 100 characters for storing additional information",
            },
            "async": {
                "type": SoftBoolOrNothingType,
                "display_name": "Async",
                "short_desc": "Asynchronous processing",
                "desc": "If true, rates will be generated asynchronously. Default is false.",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoCreateShipmentRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace ShippoDataProvider
