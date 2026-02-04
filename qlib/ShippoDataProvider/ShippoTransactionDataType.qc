# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoTransactionDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Shippo transaction (shipping label) data type
public class ShippoTransactionDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "object_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Object ID",
                "short_desc": "Unique transaction identifier",
                "desc": "The unique identifier of the transaction object in Shippo",
            },
            "object_created": {
                "type": SoftStringOrNothingType,
                "display_name": "Created",
                "short_desc": "Creation timestamp",
                "desc": "ISO 8601 timestamp when the transaction was created",
            },
            "object_updated": {
                "type": SoftStringOrNothingType,
                "display_name": "Updated",
                "short_desc": "Update timestamp",
                "desc": "ISO 8601 timestamp when the transaction was last updated",
            },
            "object_owner": {
                "type": SoftStringOrNothingType,
                "display_name": "Owner",
                "short_desc": "Owner email",
                "desc": "Email of the user who owns this transaction",
            },
            "status": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Transaction status",
                "desc": "The status of the transaction (QUEUED, WAITING, SUCCESS, ERROR, REFUNDED)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "QUEUED", "desc": "Queued for processing"},
                    <AllowedValueInfo>{"value": "WAITING", "desc": "Waiting for carrier response"},
                    <AllowedValueInfo>{"value": "SUCCESS", "desc": "Label created successfully"},
                    <AllowedValueInfo>{"value": "ERROR", "desc": "Error occurred"},
                    <AllowedValueInfo>{"value": "REFUNDED", "desc": "Label has been refunded"},
                ),
            },
            "object_state": {
                "type": SoftStringOrNothingType,
                "display_name": "Object State",
                "short_desc": "Validity state",
                "desc": "Indicates whether the transaction is valid (VALID or INVALID)",
            },
            "rate": {
                "type": SoftStringOrNothingType,
                "display_name": "Rate ID",
                "short_desc": "Rate object ID",
                "desc": "The object ID of the rate used to create this transaction",
            },
            "tracking_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Tracking Number",
                "short_desc": "Shipment tracking number",
                "desc": "The tracking number assigned by the carrier",
            },
            "tracking_status": {
                "type": SoftStringOrNothingType,
                "display_name": "Tracking Status",
                "short_desc": "Current tracking status",
                "desc": "The current tracking status of the shipment",
            },
            "tracking_url_provider": {
                "type": SoftStringOrNothingType,
                "display_name": "Tracking URL (Provider)",
                "short_desc": "Carrier tracking URL",
                "desc": "URL to the carrier's tracking page",
            },
            "eta": {
                "type": SoftStringOrNothingType,
                "display_name": "ETA",
                "short_desc": "Estimated delivery date",
                "desc": "Estimated delivery date/time",
            },
            "label_url": {
                "type": SoftStringOrNothingType,
                "display_name": "Label URL",
                "short_desc": "URL to shipping label",
                "desc": "URL to download the shipping label PDF",
            },
            "commercial_invoice_url": {
                "type": SoftStringOrNothingType,
                "display_name": "Commercial Invoice URL",
                "short_desc": "URL to commercial invoice",
                "desc": "URL to download the commercial invoice for international shipments",
            },
            "label_file_type": {
                "type": SoftStringOrNothingType,
                "display_name": "Label File Type",
                "short_desc": "Label format",
                "desc": "The file format of the shipping label",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "PNG", "desc": "PNG image"},
                    <AllowedValueInfo>{"value": "PNG_2.3x7.5", "desc": "PNG 2.3x7.5 inches"},
                    <AllowedValueInfo>{"value": "PDF", "desc": "PDF document"},
                    <AllowedValueInfo>{"value": "PDF_2.3x7.5", "desc": "PDF 2.3x7.5 inches"},
                    <AllowedValueInfo>{"value": "PDF_4x6", "desc": "PDF 4x6 inches"},
                    <AllowedValueInfo>{"value": "PDF_4x8", "desc": "PDF 4x8 inches"},
                    <AllowedValueInfo>{"value": "PDF_A4", "desc": "PDF A4"},
                    <AllowedValueInfo>{"value": "PDF_A5", "desc": "PDF A5"},
                    <AllowedValueInfo>{"value": "PDF_A6", "desc": "PDF A6"},
                    <AllowedValueInfo>{"value": "ZPLII", "desc": "ZPL thermal printer format"},
                ),
            },
            "messages": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Messages",
                "short_desc": "Status messages",
                "desc": "Array of messages related to the transaction status",
            },
            "metadata": {
                "type": SoftStringOrNothingType,
                "display_name": "Metadata",
                "short_desc": "Custom metadata",
                "desc": "A string of up to 100 characters for storing additional information",
            },
            "parcel": {
                "type": SoftStringOrNothingType,
                "display_name": "Parcel ID",
                "short_desc": "Parcel object ID",
                "desc": "The object ID of the parcel for this transaction",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoTransactionDataType") {
        addQoreFields(Fields);
    }
}

} # namespace ShippoDataProvider
