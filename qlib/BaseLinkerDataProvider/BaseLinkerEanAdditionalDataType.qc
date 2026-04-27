# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerEanAdditionalDataType.qc additional EAN entry sub-type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! A single additional-EAN entry for `addInventoryProduct`
/** Each entry carries one extra EAN/barcode for the product together with the
    quantity that barcode represents (e.g. a multi-pack barcode).
*/
public class BaseLinkerEanAdditionalDataType inherits HashDataType {
    public {
        const Fields = {
            "ean": {
                "display_name": "EAN",
                "short_desc": "Additional EAN/barcode value",
                "type": SoftStringType,
                "required": True,
                "desc": "Additional EAN/barcode value associated with the product.",
            },
            "quantity": {
                "display_name": "Quantity",
                "short_desc": "Units represented by this EAN",
                "type": SoftIntType,
                "required": True,
                "desc": "Number of product units represented by this EAN (e.g. `1` for a standard "
                    "barcode or a higher value for a multi-pack barcode).",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerEanAdditionalDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
