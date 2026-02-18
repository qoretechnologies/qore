# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeSubscriptionDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Chargebee subscription record type
/** Represents a subscription in Chargebee.
*/
public class ChargebeeSubscriptionDataType inherits HashDataType {
    private {
        #! Field definitions for Chargebee subscriptions
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Subscription ID",
                "short_desc": "Unique subscription identifier",
                "desc": "The unique identifier for the subscription in Chargebee",
                "required": True,
            },
            "customer_id": {
                "type": StringType,
                "display_name": "Customer ID",
                "short_desc": "Associated customer identifier",
                "desc": "The identifier of the customer who owns this subscription",
                "required": True,
            },
            "currency_code": {
                "type": StringOrNothingType,
                "display_name": "Currency Code",
                "short_desc": "Subscription currency",
                "desc": "The ISO 4217 currency code for the subscription",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Current subscription status",
                "desc": "The current status of the subscription",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "future",
                        "display_name": "Future",
                        "desc": "Subscription is scheduled to start in the future",
                    },
                    <AllowedValueInfo>{
                        "value": "in_trial",
                        "display_name": "In Trial",
                        "desc": "Subscription is in its trial period",
                    },
                    <AllowedValueInfo>{
                        "value": "active",
                        "display_name": "Active",
                        "desc": "Subscription is active and billing",
                    },
                    <AllowedValueInfo>{
                        "value": "non_renewing",
                        "display_name": "Non-Renewing",
                        "desc": "Subscription will not renew at end of term",
                    },
                    <AllowedValueInfo>{
                        "value": "paused",
                        "display_name": "Paused",
                        "desc": "Subscription is temporarily paused",
                    },
                    <AllowedValueInfo>{
                        "value": "cancelled",
                        "display_name": "Cancelled",
                        "desc": "Subscription has been cancelled",
                    },
                    <AllowedValueInfo>{
                        "value": "transferred",
                        "display_name": "Transferred",
                        "desc": "Subscription has been transferred to another site",
                    },
                ),
            },
            "start_date": {
                "type": IntOrNothingType,
                "display_name": "Start Date",
                "short_desc": "Subscription start timestamp",
                "desc": "Unix timestamp (in seconds) when the subscription started",
            },
            "trial_start": {
                "type": IntOrNothingType,
                "display_name": "Trial Start",
                "short_desc": "Trial start timestamp",
                "desc": "Unix timestamp (in seconds) when the trial period started",
            },
            "trial_end": {
                "type": IntOrNothingType,
                "display_name": "Trial End",
                "short_desc": "Trial end timestamp",
                "desc": "Unix timestamp (in seconds) when the trial period ends",
            },
            "current_term_start": {
                "type": IntOrNothingType,
                "display_name": "Current Term Start",
                "short_desc": "Current billing term start",
                "desc": "Unix timestamp (in seconds) when the current billing term started",
            },
            "current_term_end": {
                "type": IntOrNothingType,
                "display_name": "Current Term End",
                "short_desc": "Current billing term end",
                "desc": "Unix timestamp (in seconds) when the current billing term ends",
            },
            "next_billing_at": {
                "type": IntOrNothingType,
                "display_name": "Next Billing At",
                "short_desc": "Next billing timestamp",
                "desc": "Unix timestamp (in seconds) when the next billing occurs",
            },
            "created_at": {
                "type": IntOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "Unix timestamp (in seconds) when the subscription was created",
            },
            "updated_at": {
                "type": IntOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "Unix timestamp (in seconds) when the subscription was last updated",
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
                "short_desc": "Whether the subscription is deleted",
                "desc": "Indicates whether the subscription has been deleted",
            },
            "billing_period": {
                "type": IntOrNothingType,
                "display_name": "Billing Period",
                "short_desc": "Billing period value",
                "desc": "The number of billing period units that make up a billing cycle",
            },
            "billing_period_unit": {
                "type": StringOrNothingType,
                "display_name": "Billing Period Unit",
                "short_desc": "Unit of the billing period",
                "desc": "The unit of the billing period",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "day",
                        "display_name": "Day",
                        "desc": "Billing period measured in days",
                    },
                    <AllowedValueInfo>{
                        "value": "week",
                        "display_name": "Week",
                        "desc": "Billing period measured in weeks",
                    },
                    <AllowedValueInfo>{
                        "value": "month",
                        "display_name": "Month",
                        "desc": "Billing period measured in months",
                    },
                    <AllowedValueInfo>{
                        "value": "year",
                        "display_name": "Year",
                        "desc": "Billing period measured in years",
                    },
                ),
            },
            "remaining_billing_cycles": {
                "type": IntOrNothingType,
                "display_name": "Remaining Billing Cycles",
                "short_desc": "Remaining billing cycles",
                "desc": "The number of billing cycles remaining for the subscription",
            },
            "po_number": {
                "type": StringOrNothingType,
                "display_name": "PO Number",
                "short_desc": "Purchase order number",
                "desc": "The purchase order number associated with the subscription",
            },
            "channel": {
                "type": StringOrNothingType,
                "display_name": "Channel",
                "short_desc": "Sales channel",
                "desc": "The channel through which the subscription was created",
            },
            "mrr": {
                "type": IntOrNothingType,
                "display_name": "MRR",
                "short_desc": "Monthly recurring revenue",
                "desc": "The monthly recurring revenue of the subscription (in cents)",
            },
            "cancel_reason": {
                "type": StringOrNothingType,
                "display_name": "Cancel Reason",
                "short_desc": "Reason for cancellation",
                "desc": "The reason the subscription was cancelled, if applicable",
            },
            "meta_data": {
                "type": AutoHashOrNothingType,
                "display_name": "Metadata",
                "short_desc": "Custom metadata",
                "desc": "A set of key-value pairs stored as additional information for the subscription",
            },
            "subscription_items": {
                "type": SoftListOrNothingType,
                "display_name": "Subscription Items",
                "short_desc": "Items in the subscription",
                "desc": "The list of subscription items (plan and addon details)",
            },
            "shipping_address": {
                "type": AutoHashOrNothingType,
                "display_name": "Shipping Address",
                "short_desc": "Shipping address for the subscription",
                "desc": "The shipping address associated with this subscription",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ChargebeeDataProvider
