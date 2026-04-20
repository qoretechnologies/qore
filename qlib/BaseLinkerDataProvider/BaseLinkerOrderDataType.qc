# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerOrderDataType.qc BaseLinker order record type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! BaseLinker order (as returned by getOrders)
public class BaseLinkerOrderDataType inherits HashDataType {
    public {
        const Fields = {
            "order_id": {
                "display_name": "Order ID",
                "short_desc": "BaseLinker order identifier",
                "type": SoftIntType,
            },
            "shop_order_id": {
                "display_name": "Shop Order ID",
                "short_desc": "External shop order ID",
                "type": SoftStringOrNothingType,
            },
            "external_order_id": {
                "display_name": "External Order ID",
                "short_desc": "Marketplace order ID",
                "type": SoftStringOrNothingType,
            },
            "order_source": {
                "display_name": "Order Source",
                "short_desc": "Source channel (shop / marketplace)",
                "type": SoftStringOrNothingType,
            },
            "order_source_id": {
                "display_name": "Order Source ID",
                "short_desc": "Specific source instance id",
                "type": SoftStringOrNothingType,
            },
            "order_source_info": {
                "display_name": "Order Source Info",
                "short_desc": "Free-text source info",
                "type": SoftStringOrNothingType,
            },
            "order_status_id": {
                "display_name": "Status ID",
                "short_desc": "Order status identifier",
                "type": SoftIntOrNothingType,
            },
            "confirmed": {
                "display_name": "Confirmed",
                "short_desc": "Whether the order is confirmed",
                "type": SoftBoolOrNothingType,
            },
            "date_confirmed": {
                "display_name": "Date Confirmed",
                "short_desc": "Confirmation timestamp (Unix seconds)",
                "type": SoftIntOrNothingType,
            },
            "date_add": {
                "display_name": "Date Added",
                "short_desc": "Creation timestamp (Unix seconds)",
                "type": SoftIntOrNothingType,
            },
            "date_in_status": {
                "display_name": "Date In Status",
                "short_desc": "Time the current status was entered (Unix seconds)",
                "type": SoftIntOrNothingType,
            },
            "user_login": {
                "display_name": "User Login",
                "short_desc": "Customer login (marketplaces)",
                "type": SoftStringOrNothingType,
            },
            "phone": {
                "display_name": "Phone",
                "short_desc": "Customer phone number",
                "type": SoftStringOrNothingType,
            },
            "email": {
                "display_name": "Email",
                "short_desc": "Customer email address",
                "type": SoftStringOrNothingType,
            },
            "user_comments": {
                "display_name": "Customer Comments",
                "short_desc": "Comments from the buyer",
                "type": SoftStringOrNothingType,
            },
            "admin_comments": {
                "display_name": "Admin Comments",
                "short_desc": "Internal admin comments",
                "type": SoftStringOrNothingType,
            },
            "currency": {
                "display_name": "Currency",
                "short_desc": "3-letter ISO currency code",
                "type": SoftStringOrNothingType,
            },
            "payment_method": {
                "display_name": "Payment Method",
                "short_desc": "Payment method description",
                "type": SoftStringOrNothingType,
            },
            "payment_method_cod": {
                "display_name": "Cash on Delivery",
                "short_desc": "Whether COD",
                "type": SoftBoolOrNothingType,
            },
            "payment_done": {
                "display_name": "Amount Paid",
                "short_desc": "Amount paid so far",
                "type": SoftFloatOrNothingType,
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
            "delivery_fullname": {"display_name": "Delivery Full Name",
                "short_desc": "Recipient full name",
                "type": SoftStringOrNothingType},
            "delivery_company": {"display_name": "Delivery Company",
                "short_desc": "Recipient company", "type": SoftStringOrNothingType},
            "delivery_address": {"display_name": "Delivery Address",
                "short_desc": "Street address", "type": SoftStringOrNothingType},
            "delivery_city": {"display_name": "Delivery City",
                "short_desc": "City", "type": SoftStringOrNothingType},
            "delivery_state": {"display_name": "Delivery State",
                "short_desc": "State/province", "type": SoftStringOrNothingType},
            "delivery_postcode": {"display_name": "Delivery Postcode",
                "short_desc": "Postal code", "type": SoftStringOrNothingType},
            "delivery_country_code": {"display_name": "Delivery Country Code",
                "short_desc": "ISO country code", "type": SoftStringOrNothingType},
            "invoice_fullname": {"display_name": "Invoice Full Name",
                "short_desc": "Billing name", "type": SoftStringOrNothingType},
            "invoice_company": {"display_name": "Invoice Company",
                "short_desc": "Billing company", "type": SoftStringOrNothingType},
            "invoice_nip": {"display_name": "Invoice Tax ID",
                "short_desc": "Tax / VAT identifier", "type": SoftStringOrNothingType},
            "invoice_address": {"display_name": "Invoice Address",
                "short_desc": "Billing street address", "type": SoftStringOrNothingType},
            "invoice_city": {"display_name": "Invoice City",
                "short_desc": "Billing city", "type": SoftStringOrNothingType},
            "invoice_postcode": {"display_name": "Invoice Postcode",
                "short_desc": "Billing postal code", "type": SoftStringOrNothingType},
            "invoice_country_code": {"display_name": "Invoice Country Code",
                "short_desc": "Billing ISO country code", "type": SoftStringOrNothingType},
            "want_invoice": {
                "display_name": "Wants Invoice",
                "short_desc": "Whether the customer requested an invoice",
                "type": SoftBoolOrNothingType,
            },
            "extra_field_1": {"display_name": "Extra Field 1",
                "short_desc": "Free-form extra field", "type": SoftStringOrNothingType},
            "extra_field_2": {"display_name": "Extra Field 2",
                "short_desc": "Free-form extra field", "type": SoftStringOrNothingType},
            "products": {
                "display_name": "Products",
                "short_desc": "Ordered products",
                "type": new ListDataType("products", new BaseLinkerOrderProductDataType()),
                "desc": "Line items on the order.",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerOrderDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
