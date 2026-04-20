# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerDeliveryAddressDataType.qc delivery address sub-type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Delivery address fields used by addOrder / setOrderFields
public class BaseLinkerDeliveryAddressDataType inherits HashDataType {
    public {
        const Fields = {
            "delivery_fullname": {
                "display_name": "Delivery Full Name",
                "short_desc": "Recipient full name",
                "type": SoftStringOrNothingType,
            },
            "delivery_company": {
                "display_name": "Delivery Company",
                "short_desc": "Recipient company name",
                "type": SoftStringOrNothingType,
            },
            "delivery_address": {
                "display_name": "Delivery Address",
                "short_desc": "Street address (line 1)",
                "type": SoftStringOrNothingType,
            },
            "delivery_city": {
                "display_name": "Delivery City",
                "short_desc": "City",
                "type": SoftStringOrNothingType,
            },
            "delivery_state": {
                "display_name": "Delivery State/Province",
                "short_desc": "State or province",
                "type": SoftStringOrNothingType,
            },
            "delivery_postcode": {
                "display_name": "Delivery Postcode",
                "short_desc": "Postal code",
                "type": SoftStringOrNothingType,
            },
            "delivery_country_code": {
                "display_name": "Delivery Country Code",
                "short_desc": "2-letter ISO country code",
                "type": SoftStringOrNothingType,
                "desc": "ISO 3166-1 alpha-2 country code (e.g. `PL`, `DE`, `US`).",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerDeliveryAddressDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
