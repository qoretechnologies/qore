# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaSupplierAddressDataType.qc Katana supplier address sub-type
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Katana supplier address sub-type
public class KatanaSupplierAddressDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntOrNothingType,
                "display_name": "ID",
                "short_desc": "Address ID",
                "desc": "The unique identifier of the supplier address",
            },
            "supplier_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Supplier ID",
                "short_desc": "Supplier ID",
                "desc": "The ID of the supplier this address belongs to",
                "ref_data": "suppliers",
            },
            "line_1": {
                "type": SoftStringOrNothingType,
                "display_name": "Line 1",
                "short_desc": "Address line 1",
                "desc": "First line of the street address",
            },
            "line_2": {
                "type": SoftStringOrNothingType,
                "display_name": "Line 2",
                "short_desc": "Address line 2",
                "desc": "Second line of the street address",
            },
            "city": {
                "type": SoftStringOrNothingType,
                "display_name": "City",
                "short_desc": "City",
                "desc": "City name",
            },
            "state": {
                "type": SoftStringOrNothingType,
                "display_name": "State",
                "short_desc": "State",
                "desc": "State, province, or region",
            },
            "zip": {
                "type": SoftStringOrNothingType,
                "display_name": "Postal Code",
                "short_desc": "Postal/ZIP code",
                "desc": "Postal or ZIP code",
            },
            "country": {
                "type": SoftStringOrNothingType,
                "display_name": "Country",
                "short_desc": "Country",
                "desc": "Country name or ISO code",
            },
            "created_at": {
                "type": SoftDateOrNothingType,
                "display_name": "Created At",
                "short_desc": "Created",
                "desc": "UTC creation timestamp",
            },
            "updated_at": {
                "type": SoftDateOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Updated",
                "desc": "UTC update timestamp",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KatanaDataProvider
