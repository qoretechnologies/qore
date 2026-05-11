# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaBatchStockDataType inherits HashDataType {
    public {
        const Fields = {
            "batch_id": {"type": SoftIntOrNothingType, "display_name": "Batch ID",
                "short_desc": "Batch ID", "desc": "Unique batch identifier"},
            "batch_number": {"type": SoftStringOrNothingType, "display_name": "Batch Number",
                "short_desc": "Batch number", "desc": "Human-readable batch number"},
            "variant_id": {"type": SoftIntOrNothingType, "display_name": "Variant ID",
                "short_desc": "Variant ID", "desc": "Variant identifier",
                "ref_data": "variants"},
            "location_id": {"type": SoftIntOrNothingType, "display_name": "Location ID",
                "short_desc": "Location ID", "desc": "Location identifier",
                "ref_data": "locations"},
            "quantity_in_stock": {"type": SoftFloatOrNothingType,
                "display_name": "Quantity In Stock",
                "short_desc": "Quantity in stock",
                "desc": "Current quantity in this batch"},
            "expiration_date": {"type": SoftDateOrNothingType,
                "display_name": "Expiration Date",
                "short_desc": "Expiration date",
                "desc": "Date when this batch expires"},
            "batch_created_date": {"type": SoftDateOrNothingType,
                "display_name": "Batch Created Date",
                "short_desc": "Batch created date",
                "desc": "Date when the batch was created"},
            "batch_barcode": {"type": SoftStringOrNothingType,
                "display_name": "Batch Barcode",
                "short_desc": "Batch barcode",
                "desc": "Barcode for the batch"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
