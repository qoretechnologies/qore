# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationMarketplaceDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! ShipStation marketplace data type
public class ShipStationMarketplaceDataType inherits HashDataType {
    #! Creates the object
    constructor() : HashDataType("ShipStationMarketplace", {
        "marketplaceId": {
            "display_name": "Marketplace ID",
            "short_desc": "Unique identifier for the marketplace",
            "type": IntType,
            "desc": "The unique numeric identifier for the marketplace",
        },
        "name": {
            "display_name": "Name",
            "short_desc": "Marketplace name",
            "type": SoftStringType,
            "desc": "The marketplace name (e.g., 'Amazon', 'eBay', 'Shopify')",
        },
        "canRefresh": {
            "display_name": "Can Refresh",
            "short_desc": "Whether the marketplace can be refreshed",
            "type": SoftBoolOrNothingType,
            "desc": "Whether the marketplace supports store refresh",
        },
        "supportsCustomMappings": {
            "display_name": "Supports Custom Mappings",
            "short_desc": "Whether custom field mappings are supported",
            "type": SoftBoolOrNothingType,
            "desc": "Whether the marketplace supports custom field mappings",
        },
        "supportsCustomStatuses": {
            "display_name": "Supports Custom Statuses",
            "short_desc": "Whether custom statuses are supported",
            "type": SoftBoolOrNothingType,
            "desc": "Whether the marketplace supports custom order statuses",
        },
        "canConfirmShipments": {
            "display_name": "Can Confirm Shipments",
            "short_desc": "Whether shipment confirmation is supported",
            "type": SoftBoolOrNothingType,
            "desc": "Whether the marketplace supports shipment confirmation",
        },
    }) {
    }
}

} # namespace ShipStationDataProvider
