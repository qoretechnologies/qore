# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaManufacturingOrderDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntOrNothingType, "display_name": "ID",
                "short_desc": "MO ID", "desc": "Unique manufacturing order identifier"},
            "order_no": {"type": SoftStringOrNothingType, "display_name": "Order No.",
                "short_desc": "Order number", "desc": "Manufacturing order number"},
            "variant_id": {"type": SoftIntOrNothingType, "display_name": "Variant ID",
                "short_desc": "Variant ID", "desc": "The variant being manufactured",
                "ref_data": "variants"},
            "location_id": {"type": SoftIntOrNothingType, "display_name": "Location ID",
                "short_desc": "Location", "desc": "Manufacturing location ID",
                "ref_data": "locations"},
            "status": {"type": SoftStringOrNothingType, "display_name": "Status",
                "short_desc": "Current status", "desc": "Current MO status",
                "allowed_values": MoStatusUpdateAllowed},
            "planned_quantity": {"type": SoftFloatOrNothingType,
                "display_name": "Planned Quantity",
                "short_desc": "Planned quantity",
                "desc": "Planned quantity to manufacture"},
            "actual_quantity": {"type": SoftFloatOrNothingType,
                "display_name": "Actual Quantity",
                "short_desc": "Actual quantity",
                "desc": "Actual quantity manufactured"},
            "batch_transactions": {"type": new ListDataType("KatanaMoBatchTxList",
                    new KatanaBatchTransactionDataType(), True),
                "display_name": "Batch Transactions", "short_desc": "Batch transactions",
                "desc": "Batch transactions associated with the MO"},
            "production_deadline_date": {"type": SoftDateOrNothingType,
                "display_name": "Production Deadline",
                "short_desc": "Production deadline",
                "desc": "Production deadline date"},
            "order_created_date": {"type": SoftDateOrNothingType,
                "display_name": "Order Created Date",
                "short_desc": "Order created date",
                "desc": "Date when the order was created"},
            "done_date": {"type": SoftDateOrNothingType, "display_name": "Done Date",
                "short_desc": "Completion date", "desc": "Date when the MO was completed"},
            "additional_info": {"type": SoftStringOrNothingType,
                "display_name": "Additional Info",
                "short_desc": "Additional info", "desc": "Free-form additional information"},
            "is_linked_to_sales_order": {"type": SoftBoolOrNothingType,
                "display_name": "Linked to Sales Order",
                "short_desc": "Linked to sales order",
                "desc": "If `True`, this MO is linked to a sales order"},
            "sales_order_id": {"type": SoftIntOrNothingType,
                "display_name": "Sales Order ID",
                "short_desc": "Linked sales order ID",
                "desc": "ID of the linked sales order, if any",
                "ref_data": "sales_orders"},
            "created_at": {"type": SoftDateOrNothingType, "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "UTC timestamp when created"},
            "updated_at": {"type": SoftDateOrNothingType, "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "UTC timestamp when last updated"},
            "deleted_at": {"type": SoftDateOrNothingType, "display_name": "Deleted At",
                "short_desc": "Deletion timestamp",
                "desc": "UTC timestamp when soft-deleted"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaCreateManufacturingOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_no": {"type": SoftStringType, "required": True, "display_name": "Order No.",
                "short_desc": "MO order number", "desc": "Order number (required)",
                "example_value": "MO-0001"},
            "variant_id": {"type": SoftIntType, "required": True,
                "display_name": "Variant ID",
                "short_desc": "Variant to manufacture", "desc": "The variant to manufacture",
                "ref_data": "variants"},
            "location_id": {"type": SoftIntType, "required": True,
                "display_name": "Location ID",
                "short_desc": "Manufacturing location",
                "desc": "ID of the manufacturing location", "ref_data": "locations"},
            "planned_quantity": {"type": SoftFloatType, "required": True,
                "display_name": "Planned Quantity",
                "short_desc": "Planned quantity",
                "desc": "Planned quantity to manufacture"},
            "actual_quantity": {"type": SoftFloatOrNothingType,
                "display_name": "Actual Quantity",
                "short_desc": "Actual quantity",
                "desc": "Actual quantity manufactured"},
            "status": {"type": SoftStringOrNothingType, "display_name": "Status",
                "short_desc": "Initial status",
                "desc": "Initial status (only `NOT_STARTED` is allowed on create)",
                "allowed_values": MoStatusCreateAllowed},
            "production_deadline_date": {"type": SoftDateOrNothingType,
                "display_name": "Production Deadline",
                "short_desc": "Production deadline", "desc": "Production deadline date"},
            "order_created_date": {"type": SoftDateOrNothingType,
                "display_name": "Order Created Date",
                "short_desc": "Order created date",
                "desc": "Date when the order was created"},
            "additional_info": {"type": SoftStringOrNothingType,
                "display_name": "Additional Info",
                "short_desc": "Additional info", "desc": "Free-form additional information"},
            "batch_transactions": {"type": new ListDataType("KatanaCreateMoBatchTxList",
                    new KatanaBatchTransactionDataType(), True),
                "display_name": "Batch Transactions",
                "short_desc": "Batch transactions",
                "desc": "Batch transactions to apply"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaUpdateManufacturingOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True,
                "display_name": "Manufacturing Order ID",
                "short_desc": "MO ID to update", "desc": "ID of the MO to update",
                "ref_data": "manufacturing_orders"},
        } + KatanaDataProviderBase::makeFieldsOptional(
            KatanaCreateManufacturingOrderRequestDataType::Fields) + {
            "status": {"type": SoftStringOrNothingType, "display_name": "Status",
                "short_desc": "MO status", "desc": "Manufacturing order status",
                "allowed_values": MoStatusUpdateAllowed},
            "done_date": {"type": SoftDateOrNothingType, "display_name": "Done Date",
                "short_desc": "Completion date",
                "desc": "Date the manufacturing order was completed"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaDeleteManufacturingOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True,
                "display_name": "Manufacturing Order ID",
                "short_desc": "MO ID to delete", "desc": "ID of the MO to delete",
                "ref_data": "manufacturing_orders"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
