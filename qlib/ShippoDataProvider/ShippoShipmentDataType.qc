# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoShipmentDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Shippo shipment data type
public class ShippoShipmentDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "object_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Object ID",
                "short_desc": "Unique shipment identifier",
                "desc": "The unique identifier of the shipment object in Shippo",
            },
            "object_created": {
                "type": SoftStringOrNothingType,
                "display_name": "Created",
                "short_desc": "Creation timestamp",
                "desc": "ISO 8601 timestamp when the shipment was created",
            },
            "object_updated": {
                "type": SoftStringOrNothingType,
                "display_name": "Updated",
                "short_desc": "Update timestamp",
                "desc": "ISO 8601 timestamp when the shipment was last updated",
            },
            "object_owner": {
                "type": SoftStringOrNothingType,
                "display_name": "Owner",
                "short_desc": "Owner email",
                "desc": "Email of the user who owns this shipment",
            },
            "status": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Shipment status",
                "desc": "The status of the shipment (WAITING, QUEUED, SUCCESS, ERROR)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "WAITING", "desc": "Waiting for rates"},
                    <AllowedValueInfo>{"value": "QUEUED", "desc": "Queued for processing"},
                    <AllowedValueInfo>{"value": "SUCCESS", "desc": "Successfully processed"},
                    <AllowedValueInfo>{"value": "ERROR", "desc": "Error occurred"},
                ),
            },
            "address_from": {
                "type": new ShippoAddressDataType(),
                "display_name": "From Address",
                "short_desc": "Sender address",
                "desc": "The origin/sender address",
            },
            "address_to": {
                "type": new ShippoAddressDataType(),
                "display_name": "To Address",
                "short_desc": "Recipient address",
                "desc": "The destination/recipient address",
            },
            "address_return": {
                "type": new ShippoAddressDataType(),
                "display_name": "Return Address",
                "short_desc": "Return address",
                "desc": "The return address (if different from `address_from`)",
            },
            "parcels": {
                "type": new ListDataType("parcels", new ShippoParcelDataType(), True),
                "display_name": "Parcels",
                "short_desc": "Package details",
                "desc": "Array of parcel objects",
            },
            "rates": {
                "type": new ListDataType("rates", new ShippoRateDataType(), True),
                "display_name": "Rates",
                "short_desc": "Available shipping rates",
                "desc": "Array of rate objects for this shipment",
            },
            "shipment_date": {
                "type": SoftStringOrNothingType,
                "display_name": "Shipment Date",
                "short_desc": "Expected shipment date",
                "desc": "The date the shipment will be sent (ISO 8601 format)",
            },
            "extra": {
                "type": new ShippoShipmentExtraDataType(AutoHashOrNothingType),
                "display_name": "Extra",
                "short_desc": "Extra options",
                "desc": "Additional shipment options including insurance, signature confirmation, "
                    "billing, COD, and more",
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
            "messages": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Messages",
                "short_desc": "Status messages",
                "desc": "Array of messages related to the shipment status (each with `source` and `text`)",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoShipmentDataType") {
        addQoreFields(Fields);
    }
}

} # namespace ShippoDataProvider
