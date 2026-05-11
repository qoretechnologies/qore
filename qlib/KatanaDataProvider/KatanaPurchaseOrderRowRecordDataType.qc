# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaPurchaseOrderRowRecordDataType inherits HashDataType {
    public {
        const Fields = KatanaPurchaseOrderRowDataType::Fields + {
            "received_quantity": {"type": SoftFloatOrNothingType,
                "display_name": "Received Quantity",
                "short_desc": "Received quantity",
                "desc": "Quantity received so far"},
            "created_at": {"type": SoftDateOrNothingType, "display_name": "Created At",
                "short_desc": "Created", "desc": "UTC creation timestamp"},
            "updated_at": {"type": SoftDateOrNothingType, "display_name": "Updated At",
                "short_desc": "Updated", "desc": "UTC update timestamp"},
            "deleted_at": {"type": SoftDateOrNothingType, "display_name": "Deleted At",
                "short_desc": "Deleted", "desc": "UTC soft-deletion timestamp"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaCreatePurchaseOrderRowRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "purchase_order_id": {"type": SoftIntType, "required": True,
                "display_name": "Purchase Order ID",
                "short_desc": "Parent PO ID", "desc": "ID of the parent purchase order",
                "ref_data": "purchase_orders"},
            "variant_id": {"type": SoftIntType, "required": True,
                "display_name": "Variant ID",
                "short_desc": "Variant", "desc": "ID of the variant",
                "ref_data": "variants"},
            "quantity": {"type": SoftFloatType, "required": True,
                "display_name": "Quantity",
                "short_desc": "Quantity", "desc": "Quantity"},
            "price_per_unit": {"type": SoftFloatType, "required": True,
                "display_name": "Price Per Unit",
                "short_desc": "Unit price", "desc": "Price per unit"},
            "tax_rate_id": {"type": SoftIntOrNothingType, "display_name": "Tax Rate ID",
                "short_desc": "Tax rate", "desc": "Tax rate ID",
                "ref_data": "tax_rates"},
            "tax_rate": {"type": SoftStringOrNothingType,
                "display_name": "Tax Rate",
                "short_desc": "Tax rate value",
                "desc": "Tax rate value as a string (alternative to `tax_rate_id`)"},
            "tax_name": {"type": SoftStringOrNothingType,
                "display_name": "Tax Name",
                "short_desc": "Tax name",
                "desc": "Name of the tax applied to this row"},
            "purchase_uom_conversion_rate": {"type": SoftFloatOrNothingType,
                "display_name": "UoM Conversion Rate",
                "short_desc": "UoM conversion",
                "desc": "Conversion rate between purchase UoM and stock UoM"},
            "purchase_uom": {"type": SoftStringOrNothingType,
                "display_name": "Purchase UoM",
                "short_desc": "Purchase UoM", "desc": "Purchase unit of measure"},
            "arrival_date": {"type": SoftDateOrNothingType, "display_name": "Arrival Date",
                "short_desc": "Arrival date", "desc": "Expected arrival date"},
            "currency": {"type": SoftStringOrNothingType, "display_name": "Currency",
                "short_desc": "Currency",
                "desc": "ISO 4217 currency code for this row (overrides PO currency)"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaUpdatePurchaseOrderRowRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Row ID",
                "short_desc": "PO row ID", "desc": "ID of the row to update",
                "ref_data": "purchase_order_rows"},
        } + (KatanaDataProviderBase::makeFieldsOptional(
            KatanaCreatePurchaseOrderRowRequestDataType::Fields)
            - ("purchase_order_id", "currency")) + {
            "received_date": {"type": SoftDateOrNothingType,
                "display_name": "Received Date",
                "short_desc": "Received date",
                "desc": "Date and time when the row was received"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaDeletePurchaseOrderRowRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Row ID",
                "short_desc": "PO row ID", "desc": "ID of the row to delete",
                "ref_data": "purchase_order_rows"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
