# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerDeliveryPointDataType.qc pickup-point sub-type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Pickup-point (parcel locker) fields used by addOrder
public class BaseLinkerDeliveryPointDataType inherits HashDataType {
    public {
        const Fields = {
            "delivery_point_id": {
                "display_name": "Delivery Point ID",
                "short_desc": "Courier-specific pickup-point identifier",
                "type": SoftStringOrNothingType,
                "desc": "Identifier of the pickup point / parcel locker (e.g. InPost locker code).",
            },
            "delivery_point_name": {
                "display_name": "Delivery Point Name",
                "short_desc": "Display name of the pickup point",
                "type": SoftStringOrNothingType,
            },
            "delivery_point_address": {
                "display_name": "Delivery Point Address",
                "short_desc": "Pickup-point street address",
                "type": SoftStringOrNothingType,
            },
            "delivery_point_city": {
                "display_name": "Delivery Point City",
                "short_desc": "Pickup-point city",
                "type": SoftStringOrNothingType,
            },
            "delivery_point_postcode": {
                "display_name": "Delivery Point Postcode",
                "short_desc": "Pickup-point postal code",
                "type": SoftStringOrNothingType,
            },
        };
    }
    constructor() : HashDataType("BaseLinkerDeliveryPointDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
