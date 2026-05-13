# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Katana purchase order row sub-type
public class KatanaPurchaseOrderRowDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntOrNothingType,
                "display_name": "ID",
                "short_desc": "Row ID",
                "desc": "The unique identifier of the row",
                "ref_data": "purchase_order_rows",
            },
            "purchase_order_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Purchase Order ID",
                "short_desc": "Parent purchase order ID",
                "desc": "The ID of the purchase order this row belongs to",
                "ref_data": "purchase_orders",
            },
            "variant_id": {
                "type": SoftIntType,
                "display_name": "Variant ID",
                "required": True,
                "short_desc": "Variant ID",
                "desc": "The ID of the variant being purchased",
                "ref_data": "variants",
            },
            "quantity": {
                "type": SoftFloatType,
                "display_name": "Quantity",
                "required": True,
                "short_desc": "Quantity",
                "desc": "Quantity of the variant being purchased",
            },
            "price_per_unit": {
                "type": SoftFloatType,
                "display_name": "Price Per Unit",
                "required": True,
                "short_desc": "Unit price",
                "desc": "Price per unit in the purchase order's currency",
            },
            "tax_rate_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Tax Rate ID",
                "short_desc": "Tax rate ID",
                "desc": "The ID of the tax rate applied to this row",
                "ref_data": "tax_rates",
            },
            "purchase_uom_conversion_rate": {
                "type": SoftFloatOrNothingType,
                "display_name": "UoM Conversion Rate",
                "short_desc": "Purchase UoM conversion rate",
                "desc": "Conversion rate between the purchase unit of measure and the stock UoM",
            },
            "purchase_uom": {
                "type": SoftStringOrNothingType,
                "display_name": "Purchase UoM",
                "short_desc": "Purchase unit of measure",
                "desc": "Unit of measure used for the purchase",
            },
            "arrival_date": {
                "type": SoftDateOrNothingType,
                "display_name": "Arrival Date",
                "short_desc": "Expected arrival date",
                "desc": "Expected arrival date for this row",
            },
            "tax_rate": {
                "type": SoftStringOrNothingType,
                "display_name": "Tax Rate",
                "short_desc": "Tax rate value",
                "desc": "Tax rate value as a string (alternative to `tax_rate_id`)",
            },
            "tax_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Tax Name",
                "short_desc": "Tax name",
                "desc": "Name of the tax applied to this row",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
