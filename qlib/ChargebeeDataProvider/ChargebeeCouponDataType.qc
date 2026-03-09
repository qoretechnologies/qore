# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeCouponDataType.qc Copyright 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#! Contains all public definitions in the ChargebeeDataProvider module
public namespace ChargebeeDataProvider {

#! Chargebee coupon record type
/** Represents a coupon in Chargebee.
*/
public class ChargebeeCouponDataType inherits HashDataType {
    public {
        #! Field definitions for Chargebee coupons
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Coupon ID",
                "short_desc": "Unique coupon identifier",
                "desc": "The unique identifier for the coupon in Chargebee",
                "required": True,
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Coupon name",
                "desc": "The display name of the coupon",
                "required": True,
            },
            "discount_type": {
                "type": StringOrNothingType,
                "display_name": "Discount Type",
                "short_desc": "Type of discount",
                "desc": "The type of discount offered by this coupon",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "fixed_amount",
                        "display_name": "Fixed Amount",
                        "desc": "A fixed monetary amount is discounted",
                    },
                    <AllowedValueInfo>{
                        "value": "percentage",
                        "display_name": "Percentage",
                        "desc": "A percentage of the total is discounted",
                    },
                    <AllowedValueInfo>{
                        "value": "offer_quantity",
                        "display_name": "Offer Quantity",
                        "desc": "A quantity-based offer discount",
                    },
                ),
            },
            "discount_percentage": {
                "type": FloatOrNothingType,
                "display_name": "Discount Percentage",
                "short_desc": "Percentage discount value",
                "desc": "The percentage discount (applicable when discount_type is `percentage`)",
            },
            "discount_amount": {
                "type": IntOrNothingType,
                "display_name": "Discount Amount",
                "short_desc": "Fixed discount amount",
                "desc": "The fixed discount amount in cents (applicable when discount_type is `fixed_amount`)",
            },
            "discount_quantity": {
                "type": IntOrNothingType,
                "display_name": "Discount Quantity",
                "short_desc": "Quantity discount value",
                "desc": "The quantity discount (applicable when discount_type is `offer_quantity`)",
            },
            "currency_code": {
                "type": StringOrNothingType,
                "display_name": "Currency Code",
                "short_desc": "Coupon currency",
                "desc": "The ISO 4217 currency code for the coupon (for fixed_amount discounts)",
            },
            "duration_type": {
                "type": StringOrNothingType,
                "display_name": "Duration Type",
                "short_desc": "Duration of the discount",
                "desc": "How long the coupon discount applies",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "one_time",
                        "display_name": "One Time",
                        "desc": "Discount applies only once",
                    },
                    <AllowedValueInfo>{
                        "value": "forever",
                        "display_name": "Forever",
                        "desc": "Discount applies for the lifetime of the subscription",
                    },
                    <AllowedValueInfo>{
                        "value": "limited_period",
                        "display_name": "Limited Period",
                        "desc": "Discount applies for a limited period",
                    },
                ),
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Current coupon status",
                "desc": "The current status of the coupon",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "active",
                        "display_name": "Active",
                        "desc": "Coupon is active and can be applied",
                    },
                    <AllowedValueInfo>{
                        "value": "expired",
                        "display_name": "Expired",
                        "desc": "Coupon has expired",
                    },
                    <AllowedValueInfo>{
                        "value": "archived",
                        "display_name": "Archived",
                        "desc": "Coupon has been archived",
                    },
                    <AllowedValueInfo>{
                        "value": "deleted",
                        "display_name": "Deleted",
                        "desc": "Coupon has been deleted",
                    },
                    <AllowedValueInfo>{
                        "value": "future",
                        "display_name": "Future",
                        "desc": "Coupon is scheduled to become active in the future",
                    },
                ),
            },
            "apply_on": {
                "type": StringOrNothingType,
                "display_name": "Apply On",
                "short_desc": "Where the discount applies",
                "desc": "Determines what the coupon discount is applied to",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "invoice_amount",
                        "display_name": "Invoice Amount",
                        "desc": "Discount applies to the entire invoice amount",
                    },
                    <AllowedValueInfo>{
                        "value": "each_specified_item",
                        "display_name": "Each Specified Item",
                        "desc": "Discount applies to each specified item individually",
                    },
                ),
            },
            "valid_from": {
                "type": IntOrNothingType,
                "display_name": "Valid From",
                "short_desc": "Coupon validity start",
                "desc": "Unix timestamp (in seconds) when the coupon becomes valid",
            },
            "valid_till": {
                "type": IntOrNothingType,
                "display_name": "Valid Till",
                "short_desc": "Coupon validity end",
                "desc": "Unix timestamp (in seconds) when the coupon expires",
            },
            "max_redemptions": {
                "type": IntOrNothingType,
                "display_name": "Max Redemptions",
                "short_desc": "Maximum number of redemptions",
                "desc": "The maximum number of times this coupon can be redeemed",
            },
            "redemptions": {
                "type": IntOrNothingType,
                "display_name": "Redemptions",
                "short_desc": "Current redemption count",
                "desc": "The number of times this coupon has been redeemed",
            },
            "period": {
                "type": IntOrNothingType,
                "display_name": "Period",
                "short_desc": "Duration period value",
                "desc": "The number of period units for limited_period duration coupons",
            },
            "period_unit": {
                "type": StringOrNothingType,
                "display_name": "Period Unit",
                "short_desc": "Duration period unit",
                "desc": "The unit of the period for limited_period duration coupons",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "day",
                        "display_name": "Day",
                        "desc": "Period measured in days",
                    },
                    <AllowedValueInfo>{
                        "value": "week",
                        "display_name": "Week",
                        "desc": "Period measured in weeks",
                    },
                    <AllowedValueInfo>{
                        "value": "month",
                        "display_name": "Month",
                        "desc": "Period measured in months",
                    },
                    <AllowedValueInfo>{
                        "value": "year",
                        "display_name": "Year",
                        "desc": "Period measured in years",
                    },
                ),
            },
            "created_at": {
                "type": IntOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "Unix timestamp (in seconds) when the coupon was created",
            },
            "resource_version": {
                "type": IntOrNothingType,
                "display_name": "Resource Version",
                "short_desc": "Version number for concurrency control",
                "desc": "An incrementing version number used for concurrency control",
            },
            "deleted": {
                "type": SoftBoolOrNothingType,
                "display_name": "Deleted",
                "short_desc": "Whether the coupon is deleted",
                "desc": "Indicates whether the coupon has been deleted",
            },
            "meta_data": {
                "type": AutoHashOrNothingType,
                "display_name": "Metadata",
                "short_desc": "Custom metadata",
                "desc": "A set of key-value pairs stored as additional information for the coupon",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ChargebeeDataProvider
