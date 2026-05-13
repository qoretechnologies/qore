# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaStockAdjustmentDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntOrNothingType, "display_name": "ID",
                "short_desc": "Stock adjustment ID", "desc": "Unique stock adjustment ID"},
            "stock_adjustment_number": {"type": SoftStringOrNothingType,
                "display_name": "Adjustment Number",
                "short_desc": "Adjustment number",
                "desc": "Human-readable adjustment number"},
            "location_id": {"type": SoftIntOrNothingType, "display_name": "Location ID",
                "short_desc": "Location ID", "desc": "Location of the adjustment",
                "ref_data": "locations"},
            "stock_adjustment_date": {"type": SoftDateOrNothingType,
                "display_name": "Adjustment Date",
                "short_desc": "Adjustment date",
                "desc": "Date of the stock adjustment"},
            "reason": {"type": SoftStringOrNothingType, "display_name": "Reason",
                "short_desc": "Reason", "desc": "Reason for the adjustment"},
            "additional_info": {"type": SoftStringOrNothingType,
                "display_name": "Additional Info",
                "short_desc": "Additional info", "desc": "Free-form additional information"},
            "stock_adjustment_rows": {"type": new ListDataType("KatanaSaRowsList",
                    new KatanaStockAdjustmentRowDataType(), True),
                "display_name": "Rows", "short_desc": "Adjustment rows",
                "desc": "Stock adjustment rows"},
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

public class KatanaCreateStockAdjustmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "stock_adjustment_number": {"type": SoftStringType, "required": True,
                "display_name": "Adjustment Number",
                "short_desc": "Adjustment number",
                "desc": "Human-readable adjustment number (required)",
                "example_value": "SA-0001"},
            "location_id": {"type": SoftIntType, "required": True,
                "display_name": "Location ID",
                "short_desc": "Location ID",
                "desc": "Location of the adjustment (required)",
                "ref_data": "locations"},
            "stock_adjustment_date": {"type": SoftDateOrNothingType,
                "display_name": "Adjustment Date",
                "short_desc": "Adjustment date",
                "desc": "Date of the stock adjustment"},
            "reason": {"type": SoftStringOrNothingType, "display_name": "Reason",
                "short_desc": "Reason", "desc": "Reason for the adjustment"},
            "additional_info": {"type": SoftStringOrNothingType,
                "display_name": "Additional Info",
                "short_desc": "Additional info", "desc": "Free-form additional information"},
            "stock_adjustment_rows": {"type": new ListDataType("KatanaCreateSaRowsList",
                    new KatanaStockAdjustmentRowDataType(), False),
                "required": True, "display_name": "Rows",
                "short_desc": "Adjustment rows",
                "desc": "Stock adjustment rows (at least one required)"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaUpdateStockAdjustmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True,
                "display_name": "Stock Adjustment ID",
                "short_desc": "Adjustment ID",
                "desc": "ID of the stock adjustment to update",
                "ref_data": "stock_adjustments"},
        } + (KatanaDataProviderBase::makeFieldsOptional(
            KatanaCreateStockAdjustmentRequestDataType::Fields)
            - ("stock_adjustment_rows",));
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaDeleteStockAdjustmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True,
                "display_name": "Stock Adjustment ID",
                "short_desc": "Adjustment ID",
                "desc": "ID of the stock adjustment to delete",
                "ref_data": "stock_adjustments"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
