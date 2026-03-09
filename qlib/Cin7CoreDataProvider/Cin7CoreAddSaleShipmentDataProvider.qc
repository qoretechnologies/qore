# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreAddSaleShipmentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for adding a shipment to a sale
public class Cin7CoreAddSaleShipmentDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "add-sale-shipment",
            "desc": "Adds a shipment/tracking to a sale in Cin7Core",
            "type": "Cin7CoreAddSaleShipmentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static Cin7CoreAddSaleShipmentRequestDataType RequestType();

        #! Response type
        static Cin7CoreSaleShipmentDataType ResponseType();
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
        return "add-sale-shipment";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Adds a shipment/tracking to a sale in Cin7Core";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.SaleID.val(), "SaleID is required");

        hash<auto> body = {
            "SaleID": req.SaleID,
        };

        # Optional fields
        if (exists req.TaskID) body.TaskID = req.TaskID;
        if (exists req.Lines) body.Lines = req.Lines;
        if (exists req.ShipDate) body.ShipDate = Cin7CoreDataProviderBase::formatDate(req.ShipDate);
        if (exists req.Carrier) body.Carrier = req.Carrier;
        if (exists req.TrackingNumber) body.TrackingNumber = req.TrackingNumber;
        if (exists req.TrackingUrl) body.TrackingUrl = req.TrackingUrl;
        if (exists req.ShippingNotes) body.ShippingNotes = req.ShippingNotes;
        if (exists req.RequireBy) body.RequireBy = Cin7CoreDataProviderBase::formatDate(req.RequireBy);

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: adding shipment to sale %y", req.SaleID));
        hash<auto> response = doPost("/SaleShip", body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: added shipment"));
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

#! Request data type for adding a sale shipment
public class Cin7CoreAddSaleShipmentRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "SaleID": {
                "type": StringType,
                "display_name": "Sale ID",
                "short_desc": "The sale ID",
                "desc": "The unique identifier (GUID) of the sale",
                "required": True,
                "ref_data": "sales",
            },
            "TaskID": {
                "type": StringOrNothingType,
                "display_name": "Task ID",
                "short_desc": "Fulfilment task ID",
                "desc": "The fulfilment task ID to add shipment to",
            },
            "Lines": {
                "type": new ListDataType("Lines", new Cin7CoreFulfilmentLineDataType(), True),
                "display_name": "Lines",
                "short_desc": "Shipment lines",
                "desc": "List of lines to ship with quantities",
            },
            "ShipDate": {
                "type": DateOrNothingType,
                "display_name": "Ship Date",
                "short_desc": "Shipment date",
                "desc": "The date the items were shipped",
            },
            "Carrier": {
                "type": StringOrNothingType,
                "display_name": "Carrier",
                "short_desc": "Shipping carrier",
                "desc": "The shipping carrier name",
            },
            "TrackingNumber": {
                "type": StringOrNothingType,
                "display_name": "Tracking Number",
                "short_desc": "Tracking number",
                "desc": "The shipment tracking number",
            },
            "TrackingUrl": {
                "type": StringOrNothingType,
                "display_name": "Tracking URL",
                "short_desc": "Tracking URL",
                "desc": "URL to track the shipment",
            },
            "ShippingNotes": {
                "type": StringOrNothingType,
                "display_name": "Shipping Notes",
                "short_desc": "Shipping notes",
                "desc": "Notes about the shipment",
            },
            "RequireBy": {
                "type": DateOrNothingType,
                "display_name": "Required By",
                "short_desc": "Required by date",
                "desc": "Date by which shipment is required",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreAddSaleShipmentRequestDataType") {
        addQoreFields(Fields);
    }
}

#! Response data type for sale shipment
public class Cin7CoreSaleShipmentDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "TaskID": {
                "type": StringOrNothingType,
                "display_name": "Task ID",
                "short_desc": "Fulfilment task ID",
                "desc": "The unique identifier for the fulfilment task",
            },
            "SaleID": {
                "type": StringOrNothingType,
                "display_name": "Sale ID",
                "short_desc": "Associated sale ID",
                "desc": "The sale this shipment belongs to",
            },
            "Status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Shipment status",
                "desc": "The status of the shipment",
            },
            "Carrier": {
                "type": StringOrNothingType,
                "display_name": "Carrier",
                "short_desc": "Shipping carrier",
                "desc": "The shipping carrier name",
            },
            "TrackingNumber": {
                "type": StringOrNothingType,
                "display_name": "Tracking Number",
                "short_desc": "Tracking number",
                "desc": "The shipment tracking number",
            },
            "ShipDate": {
                "type": DateOrNothingType,
                "display_name": "Ship Date",
                "short_desc": "Ship date",
                "desc": "The date the items were shipped",
            },
            "Lines": {
                "type": new ListDataType("Lines", new Cin7CoreFulfilmentLineDataType(), True),
                "display_name": "Lines",
                "short_desc": "Shipped lines",
                "desc": "List of shipped lines",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreSaleShipmentDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
