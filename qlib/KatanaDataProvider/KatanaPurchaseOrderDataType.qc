# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaPurchaseOrderDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntOrNothingType, "display_name": "ID",
                "short_desc": "PO ID", "desc": "Unique purchase order identifier"},
            "order_no": {"type": SoftStringOrNothingType, "display_name": "Order No.",
                "short_desc": "Order number", "desc": "Purchase order number"},
            "entity_type": {"type": SoftStringOrNothingType, "display_name": "Entity Type",
                "short_desc": "Entity type",
                "desc": "Whether the purchase order is `regular` or `outsourced`",
                "allowed_values": PoEntityTypeAllowed},
            "supplier_id": {"type": SoftIntOrNothingType, "display_name": "Supplier ID",
                "short_desc": "Supplier ID", "desc": "Supplier identifier",
                "ref_data": "suppliers"},
            "location_id": {"type": SoftIntOrNothingType, "display_name": "Location ID",
                "short_desc": "Location ID", "desc": "Receiving location ID",
                "ref_data": "locations"},
            "status": {"type": SoftStringOrNothingType, "display_name": "Status",
                "short_desc": "Status", "desc": "Purchase order status",
                "allowed_values": PoStatusUpdateAllowed},
            "currency": {"type": SoftStringOrNothingType, "display_name": "Currency",
                "short_desc": "Currency", "desc": "ISO 4217 currency code"},
            "expected_arrival_date": {"type": SoftDateOrNothingType,
                "display_name": "Expected Arrival",
                "short_desc": "Expected arrival",
                "desc": "Expected arrival date"},
            "order_created_date": {"type": SoftDateOrNothingType,
                "display_name": "Order Created Date",
                "short_desc": "Order date", "desc": "Date when the order was created"},
            "additional_info": {"type": SoftStringOrNothingType,
                "display_name": "Additional Info",
                "short_desc": "Additional info", "desc": "Free-form additional information"},
            "tracking_location_id": {"type": SoftIntOrNothingType,
                "display_name": "Tracking Location ID",
                "short_desc": "Tracking location ID",
                "desc": "ID of the tracking location",
                "ref_data": "locations"},
            "total": {"type": SoftFloatOrNothingType, "display_name": "Total",
                "short_desc": "Total", "desc": "Total amount"},
            "total_in_base_currency": {"type": SoftFloatOrNothingType,
                "display_name": "Total in Base Currency",
                "short_desc": "Total base currency",
                "desc": "Total amount in base currency"},
            "purchase_order_rows": {"type": new ListDataType("KatanaPoRowsList",
                    new KatanaPurchaseOrderRowDataType(), True),
                "display_name": "Purchase Order Rows",
                "short_desc": "Rows", "desc": "Purchase order rows"},
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

public class KatanaCreatePurchaseOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "supplier_id": {"type": SoftIntType, "required": True,
                "display_name": "Supplier ID",
                "short_desc": "Supplier ID", "desc": "Supplier ID (required)",
                "ref_data": "suppliers"},
            "location_id": {"type": SoftIntType, "required": True,
                "display_name": "Location ID",
                "short_desc": "Location ID", "desc": "Receiving location ID (required)",
                "ref_data": "locations"},
            "order_no": {"type": SoftStringOrNothingType, "display_name": "Order No.",
                "short_desc": "PO number", "desc": "Order number (auto-generated if omitted)"},
            "entity_type": {"type": SoftStringOrNothingType, "display_name": "Entity Type",
                "short_desc": "Entity type",
                "desc": "Whether the purchase order is `regular` or `outsourced`",
                "allowed_values": PoEntityTypeAllowed},
            "currency": {"type": SoftStringOrNothingType, "display_name": "Currency",
                "short_desc": "Currency", "desc": "ISO 4217 currency code"},
            "expected_arrival_date": {"type": SoftDateOrNothingType,
                "display_name": "Expected Arrival",
                "short_desc": "Expected arrival",
                "desc": "Expected arrival date"},
            "order_created_date": {"type": SoftDateOrNothingType,
                "display_name": "Order Created Date",
                "short_desc": "Order date", "desc": "Date when the order was created"},
            "status": {"type": SoftStringOrNothingType, "display_name": "Status",
                "short_desc": "Initial status",
                "desc": "Initial status (only `NOT_RECEIVED` is allowed on create)",
                "allowed_values": PoStatusCreateAllowed},
            "additional_info": {"type": SoftStringOrNothingType,
                "display_name": "Additional Info",
                "short_desc": "Additional info", "desc": "Free-form additional information"},
            "tracking_location_id": {"type": SoftIntOrNothingType,
                "display_name": "Tracking Location ID",
                "short_desc": "Tracking location ID",
                "desc": "ID of the tracking location",
                "ref_data": "locations"},
            "purchase_order_rows": {"type": new ListDataType("KatanaCreatePoRowsList",
                    new KatanaPurchaseOrderRowDataType(), False),
                "required": True, "display_name": "Purchase Order Rows",
                "short_desc": "Rows", "desc": "Order rows (at least one required)"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaUpdatePurchaseOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True,
                "display_name": "Purchase Order ID",
                "short_desc": "PO ID", "desc": "ID of the purchase order to update",
                "ref_data": "purchase_orders"},
        } + (KatanaDataProviderBase::makeFieldsOptional(
            KatanaCreatePurchaseOrderRequestDataType::Fields)
            - ("purchase_order_rows",)) + {
            "status": {"type": SoftStringOrNothingType, "display_name": "Status",
                "short_desc": "Status", "desc": "Purchase order status",
                "allowed_values": PoStatusUpdateAllowed},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaDeletePurchaseOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True,
                "display_name": "Purchase Order ID",
                "short_desc": "PO ID", "desc": "ID of the purchase order to delete",
                "ref_data": "purchase_orders"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
