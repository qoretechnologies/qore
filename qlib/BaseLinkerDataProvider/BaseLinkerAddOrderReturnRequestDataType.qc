# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerAddOrderReturnRequestDataType.qc addOrderReturn request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `addOrderReturn` method
/** Delivery / invoice address and pickup-point fields are flattened to
    top-level keys, mirroring BaseLinker's flat payload shape.
*/
public class BaseLinkerAddOrderReturnRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_id": {
                "display_name": "Source Order ID",
                "short_desc": "Original order being returned (if any)",
                "type": SoftIntOrNothingType,
                "desc": "Identifier of the original order this return is associated with.",
                "ref_data": "orders",
            },
            "status_id": {
                "display_name": "Return Status ID",
                "short_desc": "Initial return status",
                "type": SoftIntType,
                "required": True,
                "desc": "Return status identifier; see `list-order-return-statuses`.",
                "ref_data": "order_return_statuses",
            },
            "date_add": {
                "display_name": "Date Added",
                "short_desc": "Return creation timestamp (Unix seconds)",
                "type": SoftIntOrNothingType,
                "desc": "Return creation timestamp as Unix seconds. Defaults to the current time.",
            },
            "order_source_id": {
                "display_name": "Order Source ID",
                "short_desc": "Source identifier",
                "type": SoftIntOrNothingType,
                "ref_data": "order_sources",
            },
            "currency": {
                "display_name": "Currency",
                "short_desc": "3-letter ISO 4217 currency code",
                "type": SoftStringType,
                "required": True,
                "desc": "ISO 4217 currency code, e.g. `PLN`, `EUR`, `USD`.",
            },
            "payment_method": {
                "display_name": "Payment Method",
                "short_desc": "Refund method description",
                "type": SoftStringOrNothingType,
            },
            "payment_method_cod": {
                "display_name": "Cash On Delivery",
                "short_desc": "Whether refund is COD",
                "type": SoftBoolOrNothingType,
            },
            "paid": {
                "display_name": "Refunded",
                "short_desc": "Whether the return has been refunded",
                "type": SoftBoolOrNothingType,
            },
            "user_comments": {
                "display_name": "Customer Comments",
                "short_desc": "Free-text comments from the customer",
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
            },
            "phone": {
                "display_name": "Phone",
                "short_desc": "Customer phone number",
                "type": SoftStringOrNothingType,
            },
            "user_login": {
                "display_name": "User Login",
                "short_desc": "Customer login (marketplace username)",
                "type": SoftStringOrNothingType,
            },
            "delivery_method": {
                "display_name": "Delivery Method",
                "short_desc": "Return shipping method name",
                "type": SoftStringOrNothingType,
            },
            "delivery_price": {
                "display_name": "Delivery Price",
                "short_desc": "Return shipping cost",
                "type": SoftFloatOrNothingType,
            },
            "delivery_fullname": {
                "display_name": "Delivery Full Name",
                "short_desc": "Sender full name",
                "type": SoftStringOrNothingType,
            },
            "delivery_company": {
                "display_name": "Delivery Company",
                "short_desc": "Sender company name",
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
            "want_invoice": {
                "display_name": "Wants Invoice",
                "short_desc": "Whether the customer requested an invoice",
                "type": SoftBoolOrNothingType,
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
            "products": {
                "display_name": "Products",
                "short_desc": "Returned line items",
                "type": new ListDataType("products", new BaseLinkerOrderProductDataType()),
                "required": True,
                "desc": "Line items being returned.",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerAddOrderReturnRequestDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
