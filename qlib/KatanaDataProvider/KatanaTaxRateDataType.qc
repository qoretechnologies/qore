# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaTaxRateDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntOrNothingType, "display_name": "ID",
                "short_desc": "Tax rate ID", "desc": "Unique tax rate identifier"},
            "name": {"type": SoftStringOrNothingType, "display_name": "Name",
                "short_desc": "Tax rate name", "desc": "Name of the tax rate"},
            "rate": {"type": SoftFloatOrNothingType, "display_name": "Rate",
                "short_desc": "Tax rate percentage",
                "desc": "Tax rate as a percentage"},
            "is_default_sales": {"type": SoftBoolOrNothingType,
                "display_name": "Default Sales",
                "short_desc": "Default sales tax",
                "desc": "If `True`, this is the default tax rate for sales"},
            "is_default_purchases": {"type": SoftBoolOrNothingType,
                "display_name": "Default Purchases",
                "short_desc": "Default purchases tax",
                "desc": "If `True`, this is the default tax rate for purchases"},
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

public class KatanaCreateTaxRateRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {"type": SoftStringOrNothingType, "display_name": "Name",
                "short_desc": "Tax rate name", "desc": "Tax rate name",
                "example_value": "VAT 20%"},
            "rate": {"type": SoftFloatType, "required": True, "display_name": "Rate",
                "short_desc": "Rate percentage",
                "desc": "Tax rate as a percentage (required, 0-999.999)",
                "example_value": 20.0},
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
