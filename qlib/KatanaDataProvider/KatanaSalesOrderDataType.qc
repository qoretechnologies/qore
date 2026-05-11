# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaSalesOrderDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntOrNothingType, "display_name": "ID",
                "short_desc": "Sales order ID",
                "desc": "Unique sales order identifier"},
            "order_no": {"type": SoftStringOrNothingType, "display_name": "Order No.",
                "short_desc": "Order number", "desc": "Sales order number"},
            "customer_id": {"type": SoftIntOrNothingType, "display_name": "Customer ID",
                "short_desc": "Customer ID", "desc": "ID of the customer",
                "ref_data": "customers"},
            "location_id": {"type": SoftIntOrNothingType, "display_name": "Location ID",
                "short_desc": "Location ID", "desc": "ID of the location",
                "ref_data": "locations"},
            "order_created_date": {"type": SoftDateOrNothingType,
                "display_name": "Order Created Date",
                "short_desc": "Order created date", "desc": "Date when the order was created"},
            "delivery_date": {"type": SoftDateOrNothingType, "display_name": "Delivery Date",
                "short_desc": "Delivery date", "desc": "Expected delivery date"},
            "picked_date": {"type": SoftDateOrNothingType, "display_name": "Picked Date",
                "short_desc": "Picked date", "desc": "Date when the order was picked"},
            "status": {"type": SoftStringOrNothingType, "display_name": "Status",
                "short_desc": "Order status", "desc": "Current sales order status",
                "allowed_values": SoStatusUpdateAllowed},
            "currency": {"type": SoftStringOrNothingType, "display_name": "Currency",
                "short_desc": "Currency", "desc": "ISO 4217 currency code (e.g. `USD`)"},
            "conversion_rate": {"type": SoftFloatOrNothingType,
                "display_name": "Conversion Rate",
                "short_desc": "Conversion rate", "desc": "Currency conversion rate"},
            "total": {"type": SoftFloatOrNothingType, "display_name": "Total",
                "short_desc": "Total amount", "desc": "Total amount"},
            "total_in_base_currency": {"type": SoftFloatOrNothingType,
                "display_name": "Total in Base Currency",
                "short_desc": "Total in base currency",
                "desc": "Total amount in base currency"},
            "ecommerce_order_type": {"type": SoftStringOrNothingType,
                "display_name": "E-commerce Order Type",
                "short_desc": "E-commerce order type",
                "desc": "Type of e-commerce order"},
            "ecommerce_store_name": {"type": SoftStringOrNothingType,
                "display_name": "E-commerce Store Name",
                "short_desc": "E-commerce store",
                "desc": "Name of the e-commerce store"},
            "ecommerce_order_id": {"type": SoftStringOrNothingType,
                "display_name": "E-commerce Order ID",
                "short_desc": "External order ID",
                "desc": "External e-commerce order ID"},
            "source": {"type": SoftStringOrNothingType, "display_name": "Source",
                "short_desc": "Order source", "desc": "Source of the order"},
            "additional_info": {"type": SoftStringOrNothingType,
                "display_name": "Additional Info",
                "short_desc": "Additional info", "desc": "Free-form additional information"},
            "customer_ref": {"type": SoftStringOrNothingType,
                "display_name": "Customer Reference",
                "short_desc": "Customer reference",
                "desc": "Customer reference number"},
            "tracking_number": {"type": SoftStringOrNothingType,
                "display_name": "Tracking Number",
                "short_desc": "Tracking number", "desc": "Shipping tracking number"},
            "tracking_number_url": {"type": SoftStringOrNothingType,
                "display_name": "Tracking URL",
                "short_desc": "Tracking URL", "desc": "Shipping tracking URL"},
            "sales_order_rows": {"type": new ListDataType("KatanaSoRowsList",
                    new KatanaSalesOrderRowDataType(), True),
                "display_name": "Sales Order Rows",
                "short_desc": "Rows", "desc": "Rows of the sales order"},
            "addresses": {"type": new ListDataType("KatanaSoAddrList",
                    new KatanaSalesOrderAddressDataType(), True),
                "display_name": "Addresses",
                "short_desc": "Addresses", "desc": "Sales order addresses"},
            "created_at": {"type": SoftDateOrNothingType, "display_name": "Created At",
                "short_desc": "Created timestamp",
                "desc": "UTC timestamp when created"},
            "updated_at": {"type": SoftDateOrNothingType, "display_name": "Updated At",
                "short_desc": "Updated timestamp",
                "desc": "UTC timestamp when last updated"},
            "deleted_at": {"type": SoftDateOrNothingType, "display_name": "Deleted At",
                "short_desc": "Deletion timestamp",
                "desc": "UTC timestamp when soft-deleted"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaCreateSalesOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_no": {"type": SoftStringOrNothingType, "display_name": "Order No.",
                "short_desc": "Sales order number",
                "desc": "Order number; auto-generated by Katana if omitted",
                "example_value": "SO-0001"},
            "customer_id": {"type": SoftIntType, "required": True,
                "display_name": "Customer ID",
                "short_desc": "Customer ID", "desc": "ID of the customer (required)",
                "ref_data": "customers"},
            "location_id": {"type": SoftIntOrNothingType, "display_name": "Location ID",
                "short_desc": "Location ID", "desc": "ID of the fulfilling location",
                "ref_data": "locations"},
            "order_created_date": {"type": SoftDateOrNothingType,
                "display_name": "Order Created Date",
                "short_desc": "Order date", "desc": "Date when the order was created"},
            "delivery_date": {"type": SoftDateOrNothingType, "display_name": "Delivery Date",
                "short_desc": "Delivery date", "desc": "Expected delivery date"},
            "currency": {"type": SoftStringOrNothingType, "display_name": "Currency",
                "short_desc": "Currency", "desc": "ISO 4217 currency code"},
            "status": {"type": SoftStringOrNothingType, "display_name": "Status",
                "short_desc": "Initial status",
                "desc": "Initial status (only `NOT_SHIPPED` is allowed on create)",
                "allowed_values": SoStatusCreateAllowed},
            "additional_info": {"type": SoftStringOrNothingType,
                "display_name": "Additional Info",
                "short_desc": "Additional info", "desc": "Free-form additional information"},
            "customer_ref": {"type": SoftStringOrNothingType,
                "display_name": "Customer Reference",
                "short_desc": "Customer reference", "desc": "Customer reference number"},
            "ecommerce_order_type": {"type": SoftStringOrNothingType,
                "display_name": "E-commerce Order Type",
                "short_desc": "E-commerce order type",
                "desc": "Type of e-commerce order"},
            "ecommerce_store_name": {"type": SoftStringOrNothingType,
                "display_name": "E-commerce Store Name",
                "short_desc": "E-commerce store",
                "desc": "Name of the e-commerce store"},
            "ecommerce_order_id": {"type": SoftStringOrNothingType,
                "display_name": "E-commerce Order ID",
                "short_desc": "External order ID",
                "desc": "External e-commerce order ID"},
            "source": {"type": SoftStringOrNothingType, "display_name": "Source",
                "short_desc": "Order source", "desc": "Source of the order"},
            "tracking_number": {"type": SoftStringOrNothingType,
                "display_name": "Tracking Number",
                "short_desc": "Tracking number", "desc": "Shipping tracking number"},
            "tracking_number_url": {"type": SoftStringOrNothingType,
                "display_name": "Tracking URL",
                "short_desc": "Tracking URL", "desc": "Shipping tracking URL"},
            "sales_order_rows": {"type": new ListDataType("KatanaCreateSoRowsList",
                    new KatanaSalesOrderRowDataType(), False),
                "required": True, "display_name": "Sales Order Rows",
                "short_desc": "Order rows",
                "desc": "Rows of the sales order (at least one required)"},
            "addresses": {"type": new ListDataType("KatanaCreateSoAddrList",
                    new KatanaSalesOrderAddressDataType(), True),
                "display_name": "Addresses",
                "short_desc": "Addresses", "desc": "Billing/shipping addresses"},
            "custom_fields": {"type": AutoHashOrNothingType,
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "Free-form hash of custom field values (keys depend on the "
                    "custom field collection configured for the sales order in Katana)"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaUpdateSalesOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Sales Order ID",
                "short_desc": "Sales order ID", "desc": "ID of the sales order to update",
                "ref_data": "sales_orders"},
        } + (KatanaDataProviderBase::makeFieldsOptional(
            KatanaCreateSalesOrderRequestDataType::Fields)
            - ("sales_order_rows", "addresses", "ecommerce_order_type",
               "ecommerce_store_name", "ecommerce_order_id", "source")) + {
            "status": {"type": SoftStringOrNothingType, "display_name": "Status",
                "short_desc": "Status", "desc": "Sales order status",
                "allowed_values": SoStatusUpdateAllowed},
            "picked_date": {"type": SoftDateOrNothingType, "display_name": "Picked Date",
                "short_desc": "Picked date",
                "desc": "Date when the order was picked"},
            "conversion_rate": {"type": SoftFloatOrNothingType,
                "display_name": "Conversion Rate",
                "short_desc": "Currency conversion rate",
                "desc": "Currency conversion rate"},
            "conversion_date": {"type": SoftDateOrNothingType,
                "display_name": "Conversion Date",
                "short_desc": "Conversion date",
                "desc": "Date the conversion rate applies to"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaDeleteSalesOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Sales Order ID",
                "short_desc": "Sales order ID", "desc": "ID of the sales order to delete",
                "ref_data": "sales_orders"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
