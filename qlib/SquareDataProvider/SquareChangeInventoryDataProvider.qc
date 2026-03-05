# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareChangeInventoryDataProvider.qc Square change inventory action

/*  SquareChangeInventoryDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {

#! Request type for changing inventory (user-friendly flattened fields)
public class SquareChangeInventoryRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "change_type": {
                "type": StringType,
                "display_name": "Change Type",
                "short_desc": "Type of inventory change (required)",
                "desc": "The type of change: ADJUSTMENT (add/remove stock) or "
                    "PHYSICAL_COUNT (set absolute count)",
                "required": True,
            },
            "catalog_object_id": {
                "type": StringType,
                "display_name": "Catalog Object ID",
                "short_desc": "The item variation ID (required)",
                "desc": "The ID of the catalog item variation to adjust",
                "required": True,
            },
            "location_id": {
                "type": StringType,
                "display_name": "Location ID",
                "short_desc": "The location ID (required)",
                "desc": "The ID of the location where the change applies",
                "required": True,
            },
            "quantity": {
                "type": StringType,
                "display_name": "Quantity",
                "short_desc": "The quantity (required)",
                "desc": "The quantity for the change (string format, e.g. '5'). "
                    "For ADJUSTMENT, this is the delta. For PHYSICAL_COUNT, this is the "
                    "absolute count.",
                "required": True,
            },
            "from_state": {
                "type": StringOrNothingType,
                "display_name": "From State",
                "short_desc": "Starting inventory state",
                "desc": "The starting inventory state for ADJUSTMENT type (e.g. NONE, IN_STOCK)",
            },
            "to_state": {
                "type": StringOrNothingType,
                "display_name": "To State",
                "short_desc": "Ending inventory state",
                "desc": "The ending inventory state for ADJUSTMENT type (e.g. IN_STOCK, SOLD)",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "Inventory state for physical count",
                "desc": "The inventory state for PHYSICAL_COUNT type (e.g. IN_STOCK)",
            },
            "occurred_at": {
                "type": StringOrNothingType,
                "display_name": "Occurred At",
                "short_desc": "When the change occurred",
                "desc": "The timestamp when the change occurred (RFC 3339 format, defaults to now)",
            },
            "reference_id": {
                "type": StringOrNothingType,
                "display_name": "Reference ID",
                "short_desc": "External reference ID",
                "desc": "An optional reference ID from an external system",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Square change inventory action data provider
public class SquareChangeInventoryDataProvider inherits SquareDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "change",
            "desc": "Change inventory counts in Square",
            "type": "SquareChangeInventoryDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    string getName() {
        return "change";
    }

    *string getDesc() {
        return "Change inventory counts in Square";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquareChangeInventoryRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new SquareInventoryCountDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string occurred_at = req.occurred_at ?? now_us().format("YYYY-MM-DDTHH:mm:ss.uuuuuuZ");

        hash<auto> change;
        if (req.change_type == "PHYSICAL_COUNT") {
            change = {
                "type": "PHYSICAL_COUNT",
                "physical_count": {
                    "catalog_object_id": req.catalog_object_id,
                    "location_id": req.location_id,
                    "quantity": string(req.quantity),
                    "state": req.state ?? "IN_STOCK",
                    "occurred_at": occurred_at,
                },
            };
            if (req.reference_id) {
                change.physical_count.reference_id = req.reference_id;
            }
        } else {
            # ADJUSTMENT
            change = {
                "type": "ADJUSTMENT",
                "adjustment": {
                    "catalog_object_id": req.catalog_object_id,
                    "location_id": req.location_id,
                    "quantity": string(req.quantity),
                    "from_state": req.from_state ?? "NONE",
                    "to_state": req.to_state ?? "IN_STOCK",
                    "occurred_at": occurred_at,
                },
            };
            if (req.reference_id) {
                change.adjustment.reference_id = req.reference_id;
            }
        }

        hash<auto> body = {
            "idempotency_key": generateIdempotencyKey(),
            "changes": (change,),
        };
        hash<auto> result = doPost("inventory/changes/batch-create", body);

        # Return the first count from the result
        if (result.counts && result.counts.size() > 0) {
            return result.counts[0];
        }
        return result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider
