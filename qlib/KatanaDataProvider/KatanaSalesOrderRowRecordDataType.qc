# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaSalesOrderRowRecordDataType inherits HashDataType {
    public {
        const Fields = KatanaSalesOrderRowDataType::Fields + {
            "created_at": {"type": SoftDateOrNothingType, "display_name": "Created At",
                "short_desc": "Created timestamp", "desc": "UTC timestamp when created"},
            "updated_at": {"type": SoftDateOrNothingType, "display_name": "Updated At",
                "short_desc": "Updated timestamp", "desc": "UTC timestamp when last updated"},
            "deleted_at": {"type": SoftDateOrNothingType, "display_name": "Deleted At",
                "short_desc": "Deletion timestamp",
                "desc": "UTC timestamp when soft-deleted"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaCreateSalesOrderRowRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "sales_order_id": {"type": SoftIntType, "required": True,
                "display_name": "Sales Order ID",
                "short_desc": "Parent sales order", "desc": "ID of the parent sales order",
                "ref_data": "sales_orders"},
            "variant_id": {"type": SoftIntType, "required": True,
                "display_name": "Variant ID",
                "short_desc": "Variant ID", "desc": "ID of the variant",
                "ref_data": "variants"},
            "quantity": {"type": SoftFloatType, "required": True,
                "display_name": "Quantity",
                "short_desc": "Quantity", "desc": "Quantity to sell"},
            "price_per_unit": {"type": SoftFloatOrNothingType,
                "display_name": "Price Per Unit",
                "short_desc": "Unit price",
                "desc": "Unit price; defaults to the variant's sales price if omitted"},
            "tax_rate_id": {"type": SoftIntOrNothingType, "display_name": "Tax Rate ID",
                "short_desc": "Tax rate ID", "desc": "Tax rate ID", "ref_data": "tax_rates"},
            "location_id": {"type": SoftIntOrNothingType, "display_name": "Location ID",
                "short_desc": "Location ID", "desc": "Location ID",
                "ref_data": "locations"},
            "total_discount": {"type": SoftFloatOrNothingType,
                "display_name": "Total Discount",
                "short_desc": "Total discount", "desc": "Discount amount"},
            "attributes": {"type": new ListDataType("KatanaSoRowAttrList",
                    new KatanaSalesOrderRowAttributeDataType(), True),
                "display_name": "Attributes",
                "short_desc": "Row attributes",
                "desc": "List of `{key, value}` attribute pairs for this row"},
            "custom_fields": {"type": AutoHashOrNothingType,
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "Free-form hash of custom field values"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaUpdateSalesOrderRowRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True,
                "display_name": "Row ID",
                "short_desc": "Sales order row ID", "desc": "ID of the row to update",
                "ref_data": "sales_order_rows"},
        } + (KatanaDataProviderBase::makeFieldsOptional(
            KatanaCreateSalesOrderRowRequestDataType::Fields)
            - ("sales_order_id",));
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaDeleteSalesOrderRowRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Row ID",
                "short_desc": "Sales order row ID", "desc": "ID of the row to delete",
                "ref_data": "sales_order_rows"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

# --- Sales order address standalone request types ---

public class KatanaCreateSalesOrderAddressRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "sales_order_id": {"type": SoftIntType, "required": True,
                "display_name": "Sales Order ID",
                "short_desc": "Parent sales order",
                "desc": "ID of the parent sales order",
                "ref_data": "sales_orders"},
            "entity_type": {"type": SoftStringType, "required": True,
                "display_name": "Entity Type",
                "short_desc": "Address type",
                "desc": "Whether this is a `billing` or `shipping` address",
                "allowed_values": AddressEntityTypeAllowed},
            "first_name": {"type": SoftStringOrNothingType, "display_name": "First Name",
                "short_desc": "First name", "desc": "Recipient first name"},
            "last_name": {"type": SoftStringOrNothingType, "display_name": "Last Name",
                "short_desc": "Last name", "desc": "Recipient last name"},
            "company": {"type": SoftStringOrNothingType, "display_name": "Company",
                "short_desc": "Company", "desc": "Company name"},
            "phone": {"type": SoftStringOrNothingType, "display_name": "Phone",
                "short_desc": "Phone", "desc": "Phone number"},
            "line_1": {"type": SoftStringOrNothingType, "display_name": "Line 1",
                "short_desc": "Address line 1", "desc": "First line of street address"},
            "line_2": {"type": SoftStringOrNothingType, "display_name": "Line 2",
                "short_desc": "Address line 2", "desc": "Second line of street address"},
            "city": {"type": SoftStringOrNothingType, "display_name": "City",
                "short_desc": "City", "desc": "City"},
            "state": {"type": SoftStringOrNothingType, "display_name": "State",
                "short_desc": "State", "desc": "State or region"},
            "zip": {"type": SoftStringOrNothingType, "display_name": "Postal Code",
                "short_desc": "Postal code", "desc": "Postal/ZIP code"},
            "country": {"type": SoftStringOrNothingType, "display_name": "Country",
                "short_desc": "Country", "desc": "Country"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaUpdateSalesOrderAddressRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Address ID",
                "short_desc": "Address ID", "desc": "ID of the address to update",
                "ref_data": "sales_order_addresses"},
        } + (KatanaDataProviderBase::makeFieldsOptional(
            KatanaCreateSalesOrderAddressRequestDataType::Fields)
            - ("sales_order_id", "entity_type"));
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaDeleteSalesOrderAddressRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Address ID",
                "short_desc": "Address ID", "desc": "ID of the address to delete",
                "ref_data": "sales_order_addresses"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
