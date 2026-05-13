# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaDataProviderDefs.qc defines constants and definitions for the KatanaDataProvider module

/*  KatanaDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

    MIT License - see COPYING for details
*/

#! Contains all public definitions in the KatanaDataProvider module
public namespace KatanaDataProvider {
    #! Application name constant
    public const AppName = "Katana";

    #! Default page size for list operations
    public const DefaultPageSize = 50;

    #! Maximum page size for list operations
    public const MaxPageSize = 250;

    #! Default polling interval in seconds
    public const DefaultPollInterval = 120;

    #! Minimum polling interval in seconds
    public const MinPollInterval = 60;

    #! Maximum number of retries for rate-limited or transient error requests
    public const MaxRetries = 3;

    #! Set of retriable socket errors
    public const RetrySet = {
        "SOCKET-CONNECT-ERROR": True,
        "SOCKET-SSL-ERROR": True,
        "SOCKET-CLOSED": True,
        "SOCKET-TIMEOUT": True,
    };

    # --- Native webhook event names ---
    public const EVENT_SO_CREATED = "sales-order-created";
    public const EVENT_SO_UPDATED = "sales-order-updated";
    public const EVENT_SO_DELETED = "sales-order-deleted";
    public const EVENT_SO_PACKED = "sales-order-packed";
    public const EVENT_SO_DELIVERED = "sales-order-delivered";
    public const EVENT_PO_CREATED = "purchase-order-created";
    public const EVENT_PO_UPDATED = "purchase-order-updated";
    public const EVENT_PO_DELETED = "purchase-order-deleted";
    public const EVENT_PO_PARTIAL = "purchase-order-partially-received";
    public const EVENT_PO_RECEIVED = "purchase-order-received";
    public const EVENT_POR_CREATED = "purchase-order-row-created";
    public const EVENT_POR_UPDATED = "purchase-order-row-updated";
    public const EVENT_POR_DELETED = "purchase-order-row-deleted";
    public const EVENT_POR_RECEIVED = "purchase-order-row-received";

    # --- Polling event names ---
    public const EVENT_NEW_PRODUCTS = "watch-new-products";
    public const EVENT_UPD_PRODUCTS = "watch-updated-products";
    public const EVENT_NEW_MATERIALS = "watch-new-materials";
    public const EVENT_UPD_MATERIALS = "watch-updated-materials";
    public const EVENT_NEW_MOS = "watch-new-manufacturing-orders";
    public const EVENT_UPD_MOS = "watch-updated-manufacturing-orders";
    public const EVENT_NEW_SOS = "watch-new-sales-orders";
    public const EVENT_UPD_SOS = "watch-updated-sales-orders";
    public const EVENT_NEW_POS = "watch-new-purchase-orders";
    public const EVENT_UPD_POS = "watch-updated-purchase-orders";
    public const EVENT_NEW_CUSTOMERS = "watch-new-customers";
    public const EVENT_UPD_CUSTOMERS = "watch-updated-customers";
    public const EVENT_NEW_SUPPLIERS = "watch-new-suppliers";
    public const EVENT_UPD_SUPPLIERS = "watch-updated-suppliers";
    public const EVENT_INV_UPDATED = "watch-inventory-updated";
    public const EVENT_NEW_STOCK_ADJ = "watch-new-stock-adjustments";

    # --- Allowed values ---
    public const MoStatusCreateAllowed = (
        <AllowedValueInfo>{"value": "NOT_STARTED", "display_name": "Not Started"},
    );
    public const MoStatusUpdateAllowed = (
        <AllowedValueInfo>{"value": "NOT_STARTED", "display_name": "Not Started"},
        <AllowedValueInfo>{"value": "IN_PROGRESS", "display_name": "In Progress"},
        <AllowedValueInfo>{"value": "BLOCKED", "display_name": "Blocked"},
        <AllowedValueInfo>{"value": "DONE", "display_name": "Done"},
    );
    public const SoStatusCreateAllowed = (
        <AllowedValueInfo>{"value": "PENDING", "display_name": "Pending"},
        <AllowedValueInfo>{"value": "NOT_SHIPPED", "display_name": "Not Shipped"},
    );
    public const SoStatusUpdateAllowed = (
        <AllowedValueInfo>{"value": "NOT_SHIPPED", "display_name": "Not Shipped"},
        <AllowedValueInfo>{"value": "PACKED", "display_name": "Packed"},
        <AllowedValueInfo>{"value": "DELIVERED", "display_name": "Delivered"},
        <AllowedValueInfo>{"value": "CANCELLED", "display_name": "Cancelled"},
    );
    public const PoStatusCreateAllowed = (
        <AllowedValueInfo>{"value": "NOT_RECEIVED", "display_name": "Not Received"},
    );
    public const PoStatusUpdateAllowed = (
        <AllowedValueInfo>{"value": "NOT_RECEIVED", "display_name": "Not Received"},
        <AllowedValueInfo>{"value": "PARTIALLY_RECEIVED", "display_name": "Partially Received"},
        <AllowedValueInfo>{"value": "RECEIVED", "display_name": "Received"},
    );
    public const PoEntityTypeAllowed = (
        <AllowedValueInfo>{"value": "regular", "display_name": "Regular"},
        <AllowedValueInfo>{"value": "outsourced", "display_name": "Outsourced"},
    );
    public const AddressEntityTypeAllowed = (
        <AllowedValueInfo>{"value": "billing", "display_name": "Billing"},
        <AllowedValueInfo>{"value": "shipping", "display_name": "Shipping"},
    );
    public const SoProductAvailabilityAllowed = (
        <AllowedValueInfo>{"value": "IN_STOCK", "display_name": "In Stock"},
        <AllowedValueInfo>{"value": "EXPECTED", "display_name": "Expected"},
        <AllowedValueInfo>{"value": "PICKED", "display_name": "Picked"},
        <AllowedValueInfo>{"value": "NOT_AVAILABLE", "display_name": "Not Available"},
        <AllowedValueInfo>{"value": "NOT_APPLICABLE", "display_name": "Not Applicable"},
    );
    public const SoIngredientAvailabilityAllowed = (
        <AllowedValueInfo>{"value": "PROCESSED", "display_name": "Processed"},
        <AllowedValueInfo>{"value": "IN_STOCK", "display_name": "In Stock"},
        <AllowedValueInfo>{"value": "NOT_AVAILABLE", "display_name": "Not Available"},
        <AllowedValueInfo>{"value": "EXPECTED", "display_name": "Expected"},
        <AllowedValueInfo>{"value": "NO_RECIPE", "display_name": "No Recipe"},
        <AllowedValueInfo>{"value": "NOT_APPLICABLE", "display_name": "Not Applicable"},
    );
    public const SoProductionStatusAllowed = (
        <AllowedValueInfo>{"value": "NOT_STARTED", "display_name": "Not Started"},
        <AllowedValueInfo>{"value": "NONE", "display_name": "None"},
        <AllowedValueInfo>{"value": "NOT_APPLICABLE", "display_name": "Not Applicable"},
        <AllowedValueInfo>{"value": "IN_PROGRESS", "display_name": "In Progress"},
        <AllowedValueInfo>{"value": "BLOCKED", "display_name": "Blocked"},
        <AllowedValueInfo>{"value": "DONE", "display_name": "Done"},
    );
    public const WebhookEventAllowed = (
        <AllowedValueInfo>{"value": "sales_order.created", "display_name": "Sales Order Created"},
        <AllowedValueInfo>{"value": "sales_order.packed", "display_name": "Sales Order Packed"},
        <AllowedValueInfo>{"value": "sales_order.delivered", "display_name": "Sales Order Delivered"},
        <AllowedValueInfo>{"value": "sales_order.updated", "display_name": "Sales Order Updated"},
        <AllowedValueInfo>{"value": "sales_order.deleted", "display_name": "Sales Order Deleted"},
        <AllowedValueInfo>{"value": "sales_order.availability_updated", "display_name": "Sales Order Availability Updated"},
        <AllowedValueInfo>{"value": "purchase_order.created", "display_name": "Purchase Order Created"},
        <AllowedValueInfo>{"value": "purchase_order.updated", "display_name": "Purchase Order Updated"},
        <AllowedValueInfo>{"value": "purchase_order.deleted", "display_name": "Purchase Order Deleted"},
        <AllowedValueInfo>{"value": "purchase_order.partially_received", "display_name": "Purchase Order Partially Received"},
        <AllowedValueInfo>{"value": "purchase_order.received", "display_name": "Purchase Order Received"},
        <AllowedValueInfo>{"value": "purchase_order_row.created", "display_name": "Purchase Order Row Created"},
        <AllowedValueInfo>{"value": "purchase_order_row.received", "display_name": "Purchase Order Row Received"},
        <AllowedValueInfo>{"value": "purchase_order_row.updated", "display_name": "Purchase Order Row Updated"},
        <AllowedValueInfo>{"value": "purchase_order_row.deleted", "display_name": "Purchase Order Row Deleted"},
        <AllowedValueInfo>{"value": "outsourced_purchase_order.created", "display_name": "Outsourced Purchase Order Created"},
        <AllowedValueInfo>{"value": "outsourced_purchase_order.updated", "display_name": "Outsourced Purchase Order Updated"},
        <AllowedValueInfo>{"value": "outsourced_purchase_order.deleted", "display_name": "Outsourced Purchase Order Deleted"},
        <AllowedValueInfo>{"value": "outsourced_purchase_order.received", "display_name": "Outsourced Purchase Order Received"},
        <AllowedValueInfo>{"value": "outsourced_purchase_order_row.created", "display_name": "Outsourced Purchase Order Row Created"},
        <AllowedValueInfo>{"value": "outsourced_purchase_order_row.updated", "display_name": "Outsourced Purchase Order Row Updated"},
        <AllowedValueInfo>{"value": "outsourced_purchase_order_row.deleted", "display_name": "Outsourced Purchase Order Row Deleted"},
        <AllowedValueInfo>{"value": "outsourced_purchase_order_row.received", "display_name": "Outsourced Purchase Order Row Received"},
        <AllowedValueInfo>{"value": "outsourced_purchase_order_recipe_row.created", "display_name": "Outsourced PO Recipe Row Created"},
        <AllowedValueInfo>{"value": "outsourced_purchase_order_recipe_row.updated", "display_name": "Outsourced PO Recipe Row Updated"},
        <AllowedValueInfo>{"value": "outsourced_purchase_order_recipe_row.deleted", "display_name": "Outsourced PO Recipe Row Deleted"},
        <AllowedValueInfo>{"value": "manufacturing_order.created", "display_name": "Manufacturing Order Created"},
        <AllowedValueInfo>{"value": "manufacturing_order.updated", "display_name": "Manufacturing Order Updated"},
        <AllowedValueInfo>{"value": "manufacturing_order.deleted", "display_name": "Manufacturing Order Deleted"},
        <AllowedValueInfo>{"value": "manufacturing_order.in_progress", "display_name": "Manufacturing Order In Progress"},
        <AllowedValueInfo>{"value": "manufacturing_order.blocked", "display_name": "Manufacturing Order Blocked"},
        <AllowedValueInfo>{"value": "manufacturing_order.done", "display_name": "Manufacturing Order Done"},
        <AllowedValueInfo>{"value": "manufacturing_order_recipe_row.created", "display_name": "MO Recipe Row Created"},
        <AllowedValueInfo>{"value": "manufacturing_order_recipe_row.updated", "display_name": "MO Recipe Row Updated"},
        <AllowedValueInfo>{"value": "manufacturing_order_recipe_row.deleted", "display_name": "MO Recipe Row Deleted"},
        <AllowedValueInfo>{"value": "manufacturing_order_recipe_row.ingredients_in_stock", "display_name": "MO Recipe Row Ingredients In Stock"},
        <AllowedValueInfo>{"value": "manufacturing_order_operation_row.created", "display_name": "MO Operation Row Created"},
        <AllowedValueInfo>{"value": "manufacturing_order_operation_row.updated", "display_name": "MO Operation Row Updated"},
        <AllowedValueInfo>{"value": "manufacturing_order_operation_row.deleted", "display_name": "MO Operation Row Deleted"},
        <AllowedValueInfo>{"value": "manufacturing_order_operation_row.in_progress", "display_name": "MO Operation Row In Progress"},
        <AllowedValueInfo>{"value": "manufacturing_order_operation_row.paused", "display_name": "MO Operation Row Paused"},
        <AllowedValueInfo>{"value": "manufacturing_order_operation_row.blocked", "display_name": "MO Operation Row Blocked"},
        <AllowedValueInfo>{"value": "manufacturing_order_operation_row.completed", "display_name": "MO Operation Row Completed"},
        <AllowedValueInfo>{"value": "current_inventory.product_updated", "display_name": "Inventory Product Updated"},
        <AllowedValueInfo>{"value": "current_inventory.material_updated", "display_name": "Inventory Material Updated"},
        <AllowedValueInfo>{"value": "current_inventory.product_out_of_stock", "display_name": "Inventory Product Out Of Stock"},
        <AllowedValueInfo>{"value": "current_inventory.material_out_of_stock", "display_name": "Inventory Material Out Of Stock"},
        <AllowedValueInfo>{"value": "product.created", "display_name": "Product Created"},
        <AllowedValueInfo>{"value": "product.updated", "display_name": "Product Updated"},
        <AllowedValueInfo>{"value": "product.deleted", "display_name": "Product Deleted"},
        <AllowedValueInfo>{"value": "material.created", "display_name": "Material Created"},
        <AllowedValueInfo>{"value": "material.updated", "display_name": "Material Updated"},
        <AllowedValueInfo>{"value": "material.deleted", "display_name": "Material Deleted"},
        <AllowedValueInfo>{"value": "variant.created", "display_name": "Variant Created"},
        <AllowedValueInfo>{"value": "variant.updated", "display_name": "Variant Updated"},
        <AllowedValueInfo>{"value": "variant.deleted", "display_name": "Variant Deleted"},
        <AllowedValueInfo>{"value": "product_recipe_row.created", "display_name": "Product Recipe Row Created"},
        <AllowedValueInfo>{"value": "product_recipe_row.updated", "display_name": "Product Recipe Row Updated"},
        <AllowedValueInfo>{"value": "product_recipe_row.deleted", "display_name": "Product Recipe Row Deleted"},
    );

    #! Common DPAT_FIND action options shared across list-* actions
    public const CommonListActionOptions = {
        "limit": <ActionOptionInfo>{
            "display_name": "Limit",
            "short_desc": "Maximum number of records to return per page",
            "type": AbstractDataProviderTypeMap."*int",
            "desc": "Maximum number of records to return per page (max `250`)",
            "preselected": True,
        },
        "page": <ActionOptionInfo>{
            "display_name": "Page",
            "short_desc": "Page number (1-based)",
            "type": AbstractDataProviderTypeMap."*int",
            "desc": "Page number for pagination (1-based)",
        },
        "ids": <ActionOptionInfo>{
            "display_name": "IDs",
            "short_desc": "Filter by a list of IDs",
            "type": AbstractDataProviderTypeMap."*softlist",
            "desc": "Filter results to only the records with the given integer IDs",
        },
        "created_at_min": <ActionOptionInfo>{
            "display_name": "Created At (Min)",
            "short_desc": "Only return records created on/after this UTC timestamp",
            "type": AbstractDataProviderTypeMap."*date",
            "desc": "Only return records created on or after this UTC timestamp "
                "(ISO 8601, e.g. `2026-04-09T16:58:14Z`)",
        },
        "created_at_max": <ActionOptionInfo>{
            "display_name": "Created At (Max)",
            "short_desc": "Only return records created on/before this UTC timestamp",
            "type": AbstractDataProviderTypeMap."*date",
            "desc": "Only return records created on or before this UTC timestamp",
        },
        "updated_at_min": <ActionOptionInfo>{
            "display_name": "Updated At (Min)",
            "short_desc": "Only return records updated on/after this UTC timestamp",
            "type": AbstractDataProviderTypeMap."*date",
            "desc": "Only return records updated on or after this UTC timestamp",
        },
        "updated_at_max": <ActionOptionInfo>{
            "display_name": "Updated At (Max)",
            "short_desc": "Only return records updated on/before this UTC timestamp",
            "type": AbstractDataProviderTypeMap."*date",
            "desc": "Only return records updated on or before this UTC timestamp",
        },
        "include_deleted": <ActionOptionInfo>{
            "display_name": "Include Deleted",
            "short_desc": "Include soft-deleted records in the response",
            "type": AbstractDataProviderTypeMap."*bool",
            "desc": "If `True`, includes soft-deleted records in the response",
        },
    };

    #! Katana logo in SVG format (loaded from file)
    public const KatanaLogo = File::readTextFile(get_script_dir() + "/katana-logo.svg");
}
