# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerOrderReturnDataType.qc
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! BaseLinker order return
public class BaseLinkerOrderReturnDataType inherits HashDataType {
    public {
        const Fields = {
            "return_id": {"display_name": "Return ID",
                "short_desc": "Return identifier", "type": SoftIntType},
            "order_id": {"display_name": "Order ID",
                "short_desc": "Original order identifier", "type": SoftIntOrNothingType},
            "status_id": {"display_name": "Status ID",
                "short_desc": "Return status identifier", "type": SoftIntOrNothingType},
            "date_add": {"display_name": "Date Added",
                "short_desc": "Creation timestamp (Unix seconds)",
                "type": SoftIntOrNothingType},
            "date_in_status": {"display_name": "Date In Status",
                "short_desc": "Time the current status was entered",
                "type": SoftIntOrNothingType},
            "currency": {"display_name": "Currency",
                "short_desc": "ISO currency code", "type": SoftStringOrNothingType},
            "payment_method": {"display_name": "Payment Method",
                "short_desc": "Payment method label", "type": SoftStringOrNothingType},
            "payment_done": {"display_name": "Amount Refunded",
                "short_desc": "Amount refunded so far", "type": SoftFloatOrNothingType},
            "user_login": {"display_name": "User Login",
                "short_desc": "Customer login", "type": SoftStringOrNothingType},
            "phone": {"display_name": "Phone",
                "short_desc": "Customer phone", "type": SoftStringOrNothingType},
            "email": {"display_name": "Email",
                "short_desc": "Customer email", "type": SoftStringOrNothingType},
            "user_comments": {"display_name": "Customer Comments",
                "short_desc": "Comments from the customer", "type": SoftStringOrNothingType},
            "admin_comments": {"display_name": "Admin Comments",
                "short_desc": "Internal admin comments", "type": SoftStringOrNothingType},
            "delivery_fullname": {"display_name": "Delivery Full Name",
                "short_desc": "Recipient name", "type": SoftStringOrNothingType},
            "delivery_address": {"display_name": "Delivery Address",
                "short_desc": "Street address", "type": SoftStringOrNothingType},
            "delivery_city": {"display_name": "Delivery City",
                "short_desc": "City", "type": SoftStringOrNothingType},
            "delivery_postcode": {"display_name": "Delivery Postcode",
                "short_desc": "Postal code", "type": SoftStringOrNothingType},
            "delivery_country_code": {"display_name": "Delivery Country Code",
                "short_desc": "ISO country code", "type": SoftStringOrNothingType},
            "products": {
                "display_name": "Returned Products",
                "short_desc": "Products being returned",
                "type": new ListDataType("products", new BaseLinkerOrderProductDataType()),
            },
        };
    }
    constructor() : HashDataType("BaseLinkerOrderReturnDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
