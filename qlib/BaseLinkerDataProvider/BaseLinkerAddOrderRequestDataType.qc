# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerAddOrderRequestDataType.qc addOrder request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `addOrder` method
/** All delivery / invoice / pickup-point fields are flattened to top-level keys
    because BaseLinker's `addOrder` payload takes them flat — it does not accept
    nested objects.
*/
public class BaseLinkerAddOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_status_id": {
                "display_name": "Order Status ID",
                "short_desc": "Initial order status identifier",
                "type": SoftIntType,
                "required": True,
                "desc": "Initial order status; see `list-order-statuses`.",
                "ref_data": "order_statuses",
            },
            "date_add": {
                "display_name": "Date Added",
                "short_desc": "Order creation timestamp (Unix seconds)",
                "type": SoftIntType,
                "required": True,
                "desc": "Order creation timestamp as Unix seconds.",
                "example_value": 1745280000,
            },
            "currency": {
                "display_name": "Currency",
                "short_desc": "3-letter ISO 4217 currency code",
                "type": SoftStringType,
                "required": True,
                "desc": "ISO 4217 currency code, e.g. `PLN`, `EUR`, `USD`.",
                "example_value": "PLN",
            },
            "payment_method": {
                "display_name": "Payment Method",
                "short_desc": "Payment method description",
                "type": SoftStringType,
                "required": True,
                "example_value": "Credit Card",
            },
            "payment_method_cod": {
                "display_name": "Cash On Delivery",
                "short_desc": "Whether this is a COD order",
                "type": SoftBoolType,
                "required": True,
                "desc": "`True` marks the order as cash-on-delivery.",
            },
            "paid": {
                "display_name": "Paid",
                "short_desc": "Whether the order is fully paid",
                "type": SoftBoolType,
                "required": True,
                "desc": "`True` marks the order as paid in full at creation time.",
            },
            "user_comments": {
                "display_name": "Customer Comments",
                "short_desc": "Free-text comments from the buyer",
                "type": SoftStringOrNothingType,
            },
            "admin_comments": {
                "display_name": "Admin Comments",
                "short_desc": "Internal admin comments",
                "type": SoftStringOrNothingType,
            },
            "email": {
                "display_name": "Email",
                "short_desc": "Customer email address",
                "type": SoftStringOrNothingType,
                "example_value": "customer@example.com",
            },
            "phone": {
                "display_name": "Phone",
                "short_desc": "Customer phone number",
                "type": SoftStringOrNothingType,
                "example_value": "+48 123 456 789",
            },
            "user_login": {
                "display_name": "User Login",
                "short_desc": "Customer login (marketplace username)",
                "type": SoftStringOrNothingType,
            },
            "delivery_method": {
                "display_name": "Delivery Method",
                "short_desc": "Shipping method name",
                "type": SoftStringOrNothingType,
            },
            "delivery_price": {
                "display_name": "Delivery Price",
                "short_desc": "Shipping cost (gross)",
                "type": SoftFloatOrNothingType,
            },
            "delivery_fullname": {
                "display_name": "Delivery Full Name",
                "short_desc": "Recipient full name",
                "type": SoftStringOrNothingType,
            },
            "delivery_company": {
                "display_name": "Delivery Company",
                "short_desc": "Recipient company name",
                "type": SoftStringOrNothingType,
            },
            "delivery_address": {
                "display_name": "Delivery Address",
                "short_desc": "Street address",
                "type": SoftStringOrNothingType,
            },
            "delivery_postcode": {
                "display_name": "Delivery Postcode",
                "short_desc": "Postal code",
                "type": SoftStringOrNothingType,
            },
            "delivery_city": {
                "display_name": "Delivery City",
                "short_desc": "City",
                "type": SoftStringOrNothingType,
            },
            "delivery_state": {
                "display_name": "Delivery State/Province",
                "short_desc": "State or province",
                "type": SoftStringOrNothingType,
            },
            "delivery_country_code": {
                "display_name": "Delivery Country Code",
                "short_desc": "2-letter ISO country code",
                "type": SoftStringOrNothingType,
                "desc": "ISO 3166-1 alpha-2 country code (e.g. `PL`, `DE`, `US`).",
                "example_value": "PL",
            },
            "invoice_fullname": {
                "display_name": "Invoice Full Name",
                "short_desc": "Billing recipient full name",
                "type": SoftStringOrNothingType,
            },
            "invoice_company": {
                "display_name": "Invoice Company",
                "short_desc": "Billing company name",
                "type": SoftStringOrNothingType,
            },
            "invoice_nip": {
                "display_name": "Invoice Tax ID",
                "short_desc": "NIP / VAT / tax identifier",
                "type": SoftStringOrNothingType,
            },
            "invoice_address": {
                "display_name": "Invoice Address",
                "short_desc": "Billing street address",
                "type": SoftStringOrNothingType,
            },
            "invoice_postcode": {
                "display_name": "Invoice Postcode",
                "short_desc": "Billing postal code",
                "type": SoftStringOrNothingType,
            },
            "invoice_city": {
                "display_name": "Invoice City",
                "short_desc": "Billing city",
                "type": SoftStringOrNothingType,
            },
            "invoice_state": {
                "display_name": "Invoice State/Province",
                "short_desc": "Billing state or province",
                "type": SoftStringOrNothingType,
            },
            "invoice_country_code": {
                "display_name": "Invoice Country Code",
                "short_desc": "Billing 2-letter ISO country code",
                "type": SoftStringOrNothingType,
            },
            "delivery_point_id": {
                "display_name": "Delivery Point ID",
                "short_desc": "Pickup-point identifier",
                "type": SoftStringOrNothingType,
                "desc": "Identifier of the pickup point / parcel locker (e.g. InPost locker code).",
            },
            "delivery_point_name": {
                "display_name": "Delivery Point Name",
                "short_desc": "Pickup-point display name",
                "type": SoftStringOrNothingType,
            },
            "delivery_point_address": {
                "display_name": "Delivery Point Address",
                "short_desc": "Pickup-point street address",
                "type": SoftStringOrNothingType,
            },
            "delivery_point_postcode": {
                "display_name": "Delivery Point Postcode",
                "short_desc": "Pickup-point postal code",
                "type": SoftStringOrNothingType,
            },
            "delivery_point_city": {
                "display_name": "Delivery Point City",
                "short_desc": "Pickup-point city",
                "type": SoftStringOrNothingType,
            },
            "want_invoice": {
                "display_name": "Wants Invoice",
                "short_desc": "Whether the customer requested an invoice",
                "type": SoftBoolOrNothingType,
            },
            "custom_source_id": {
                "display_name": "Custom Source ID",
                "short_desc": "Custom personal source ID",
                "type": SoftIntOrNothingType,
                "ref_data": "order_sources",
                "desc": "Identifier of a custom personal order source (see `list-order-sources`).",
            },
            "extra_field_1": {
                "display_name": "Extra Field 1",
                "short_desc": "Free-form extra field",
                "type": SoftStringOrNothingType,
            },
            "extra_field_2": {
                "display_name": "Extra Field 2",
                "short_desc": "Free-form extra field",
                "type": SoftStringOrNothingType,
            },
            "custom_extra_fields": {
                "display_name": "Custom Extra Fields",
                "short_desc": "Map of custom extra field ID to value",
                "type": AutoHashOrNothingType,
                "desc": "Map of `{\"<extra_field_id>\": <value>}`. Text fields take strings; "
                    "file fields take a `{\"title\": \"<filename>\", \"file\": \"<base64>\"}` "
                    "hash where the base64-encoded file must not exceed 2 MB.",
            },
            "products": {
                "display_name": "Products",
                "short_desc": "Ordered line items",
                "type": new ListDataType("products", new BaseLinkerOrderProductDataType()),
                "required": True,
                "desc": "Line items on the order.",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerAddOrderRequestDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
