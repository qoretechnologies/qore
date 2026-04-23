# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerAddOrderReturnRequestDataType.qc addOrderReturn request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `addOrderReturn` method
/** Only delivery address fields are supported — invoice addresses, payment
    methods, and pickup-point fields are not accepted by `addOrderReturn`.
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
                "desc": "Return status identifier; values are drawn from BaseLinker's "
                    "`getOrderReturnStatusList` endpoint and surfaced via the "
                    "`order_return_statuses` reference data.",
                "ref_data": "order_return_statuses",
            },
            "custom_source_id": {
                "display_name": "Custom Source ID",
                "short_desc": "Custom personal source ID",
                "type": SoftIntOrNothingType,
                "ref_data": "order_sources",
                "desc": "Identifier of a custom personal order source (see `list-order-sources`).",
            },
            "reference_number": {
                "display_name": "Reference Number",
                "short_desc": "External reference number for the return",
                "type": SoftStringOrNothingType,
            },
            "date_add": {
                "display_name": "Date Added",
                "short_desc": "Return creation timestamp (Unix seconds)",
                "type": SoftIntType,
                "required": True,
                "desc": "Return creation timestamp as Unix seconds.",
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
            "refunded": {
                "display_name": "Refunded",
                "short_desc": "Whether the return has been refunded",
                "type": SoftBoolType,
                "required": True,
                "desc": "`True` marks the return as refunded.",
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
                "desc": "ISO 3166-1 alpha-2 country code (e.g. `PL`, `DE`, `US`).",
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
                "desc": "Map of `{\"<extra_field_id>\": <value>}` for custom order-return extra "
                    "fields configured in BaseLinker.",
            },
            "refund_account_number": {
                "display_name": "Refund Account Number",
                "short_desc": "Bank account for the refund",
                "type": SoftStringOrNothingType,
            },
            "refund_iban": {
                "display_name": "Refund IBAN",
                "short_desc": "IBAN for the refund",
                "type": SoftStringOrNothingType,
            },
            "refund_swift": {
                "display_name": "Refund SWIFT",
                "short_desc": "SWIFT/BIC code for the refund",
                "type": SoftStringOrNothingType,
            },
            "products": {
                "display_name": "Products",
                "short_desc": "Returned line items",
                "type": new ListDataType("products", new BaseLinkerOrderProductDataType(), True),
                "desc": "Line items being returned.",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerAddOrderReturnRequestDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
