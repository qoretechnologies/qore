# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Katana sales order row sub-type
public class KatanaSalesOrderRowDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntOrNothingType,
                "display_name": "ID",
                "short_desc": "Row ID",
                "desc": "The unique identifier of the row",
                "ref_data": "sales_order_rows",
            },
            "sales_order_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Sales Order ID",
                "short_desc": "Parent sales order ID",
                "desc": "The ID of the sales order this row belongs to",
                "ref_data": "sales_orders",
            },
            "variant_id": {
                "type": SoftIntType,
                "display_name": "Variant ID",
                "required": True,
                "short_desc": "Variant ID",
                "desc": "The ID of the variant being sold",
                "ref_data": "variants",
            },
            "quantity": {
                "type": SoftFloatType,
                "display_name": "Quantity",
                "required": True,
                "short_desc": "Quantity",
                "desc": "Quantity of the variant",
            },
            "price_per_unit": {
                "type": SoftFloatOrNothingType,
                "display_name": "Price Per Unit",
                "short_desc": "Unit price",
                "desc": "Price per unit; defaults to the variant's sales price if omitted",
            },
            "tax_rate_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Tax Rate ID",
                "short_desc": "Tax rate ID",
                "desc": "The ID of the tax rate applied to this row",
                "ref_data": "tax_rates",
            },
            "location_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Location ID",
                "short_desc": "Location ID",
                "desc": "The ID of the location for this row",
                "ref_data": "locations",
            },
            "total_discount": {
                "type": SoftFloatOrNothingType,
                "display_name": "Total Discount",
                "short_desc": "Total discount amount",
                "desc": "Total discount applied to this row",
            },
            "attributes": {
                "type": new ListDataType("KatanaSalesOrderRowAttributesList",
                    new KatanaSalesOrderRowAttributeDataType(), True),
                "display_name": "Attributes",
                "short_desc": "Row attributes",
                "desc": "List of `{key, value}` attribute pairs for this row",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
