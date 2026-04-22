# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerInventoryManufacturerDataType.qc inventory manufacturer record
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! An inventory manufacturer (from `getInventoryManufacturers`)
public class BaseLinkerInventoryManufacturerDataType inherits HashDataType {
    public {
        const Fields = {
            "manufacturer_id": {
                "display_name": "Manufacturer ID",
                "short_desc": "Manufacturer identifier",
                "type": SoftIntType,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Manufacturer name",
                "type": SoftStringOrNothingType,
            },
            "manufacturer_name": {
                "display_name": "Manufacturer Name",
                "short_desc": "Manufacturer name (duplicate of `name`)",
                "type": SoftStringOrNothingType,
            },
            "manufacturer_photo": {
                "display_name": "Photo URL",
                "short_desc": "URL of the manufacturer photo",
                "type": SoftStringOrNothingType,
            },
            "manufacturer_url": {
                "display_name": "Website",
                "short_desc": "Manufacturer website URL",
                "type": SoftStringOrNothingType,
            },
            "manufacturer_street": {
                "display_name": "Street",
                "short_desc": "Street address",
                "type": SoftStringOrNothingType,
            },
            "manufacturer_postcode": {
                "display_name": "Postcode",
                "short_desc": "Postal code",
                "type": SoftStringOrNothingType,
            },
            "manufacturer_city": {
                "display_name": "City",
                "short_desc": "City",
                "type": SoftStringOrNothingType,
            },
            "manufacturer_state": {
                "display_name": "State/Province",
                "short_desc": "State or province",
                "type": SoftStringOrNothingType,
            },
            "manufacturer_country_code": {
                "display_name": "Country Code",
                "short_desc": "2-letter ISO country code",
                "type": SoftStringOrNothingType,
            },
            "manufacturer_email": {
                "display_name": "Email",
                "short_desc": "Contact email",
                "type": SoftStringOrNothingType,
            },
            "manufacturer_phone": {
                "display_name": "Phone",
                "short_desc": "Contact phone",
                "type": SoftStringOrNothingType,
            },
        };
    }
    constructor() : HashDataType("BaseLinkerInventoryManufacturerDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
