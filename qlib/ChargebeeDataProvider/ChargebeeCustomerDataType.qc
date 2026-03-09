# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeCustomerDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Chargebee customer record type
/** Represents a customer in Chargebee.
*/
public class ChargebeeCustomerDataType inherits HashDataType {
    public {
        #! Field definitions for Chargebee customers
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Customer ID",
                "short_desc": "Unique customer identifier",
                "desc": "The unique identifier for the customer in Chargebee",
                "required": True,
            },
            "first_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "Customer's first name",
                "desc": "The first name of the customer",
            },
            "last_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Customer's last name",
                "desc": "The last name of the customer",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "Customer's email address",
                "desc": "The email address of the customer",
                "example_value": "customer@example.com",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Customer's phone number",
                "desc": "The phone number of the customer",
            },
            "company": {
                "type": StringOrNothingType,
                "display_name": "Company",
                "short_desc": "Customer's company name",
                "desc": "The company name associated with the customer",
            },
            "auto_collection": {
                "type": StringOrNothingType,
                "display_name": "Auto Collection",
                "short_desc": "Auto-collection setting",
                "desc": "Whether payments are automatically collected for this customer",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "on",
                        "display_name": "On",
                        "desc": "Payments are automatically collected",
                    },
                    <AllowedValueInfo>{
                        "value": "off",
                        "display_name": "Off",
                        "desc": "Payments are not automatically collected",
                    },
                ),
            },
            "net_term_days": {
                "type": IntOrNothingType,
                "display_name": "Net Term Days",
                "short_desc": "Payment net term in days",
                "desc": "The number of days within which the customer has to make payment for invoices",
            },
            "vat_number": {
                "type": StringOrNothingType,
                "display_name": "VAT Number",
                "short_desc": "Customer's VAT number",
                "desc": "The VAT/tax registration number for the customer",
            },
            "created_at": {
                "type": IntOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "Unix timestamp (in seconds) when the customer was created",
            },
            "updated_at": {
                "type": IntOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "Unix timestamp (in seconds) when the customer was last updated",
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
                "short_desc": "Whether the customer is deleted",
                "desc": "Indicates whether the customer has been deleted",
            },
            "taxability": {
                "type": StringOrNothingType,
                "display_name": "Taxability",
                "short_desc": "Tax status of the customer",
                "desc": "Whether the customer is taxable or exempt from taxes",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "taxable",
                        "display_name": "Taxable",
                        "desc": "Customer is subject to taxes",
                    },
                    <AllowedValueInfo>{
                        "value": "exempt",
                        "display_name": "Exempt",
                        "desc": "Customer is exempt from taxes",
                    },
                ),
            },
            "locale": {
                "type": StringOrNothingType,
                "display_name": "Locale",
                "short_desc": "Customer's locale",
                "desc": "The locale code for the customer (e.g., `en-US`, `fr-FR`)",
            },
            "preferred_currency_code": {
                "type": StringOrNothingType,
                "display_name": "Preferred Currency Code",
                "short_desc": "Customer's preferred currency",
                "desc": "The ISO 4217 currency code preferred by the customer",
            },
            "promotional_credits": {
                "type": IntOrNothingType,
                "display_name": "Promotional Credits",
                "short_desc": "Available promotional credits",
                "desc": "The promotional credits balance for the customer (in cents)",
            },
            "refundable_credits": {
                "type": IntOrNothingType,
                "display_name": "Refundable Credits",
                "short_desc": "Available refundable credits",
                "desc": "The refundable credits balance for the customer (in cents)",
            },
            "excess_payments": {
                "type": IntOrNothingType,
                "display_name": "Excess Payments",
                "short_desc": "Excess payment amount",
                "desc": "The excess payments balance for the customer (in cents)",
            },
            "unbilled_charges": {
                "type": IntOrNothingType,
                "display_name": "Unbilled Charges",
                "short_desc": "Pending unbilled charges",
                "desc": "The total unbilled charges for the customer (in cents)",
            },
            "channel": {
                "type": StringOrNothingType,
                "display_name": "Channel",
                "short_desc": "Sales channel",
                "desc": "The channel through which the customer was acquired",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "web",
                        "display_name": "Web",
                        "desc": "Customer was acquired through the web",
                    },
                    <AllowedValueInfo>{
                        "value": "app_store",
                        "display_name": "App Store",
                        "desc": "Customer was acquired through the Apple App Store",
                    },
                    <AllowedValueInfo>{
                        "value": "play_store",
                        "display_name": "Play Store",
                        "desc": "Customer was acquired through the Google Play Store",
                    },
                ),
            },
            "billing_address": {
                "type": AutoHashOrNothingType,
                "display_name": "Billing Address",
                "short_desc": "Customer's billing address",
                "desc": "The billing address for the customer",
            },
            "payment_method": {
                "type": AutoHashOrNothingType,
                "display_name": "Payment Method",
                "short_desc": "Customer's payment method",
                "desc": "The primary payment method for the customer",
            },
            "meta_data": {
                "type": AutoHashOrNothingType,
                "display_name": "Metadata",
                "short_desc": "Custom metadata",
                "desc": "A set of key-value pairs stored as additional information for the customer",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ChargebeeDataProvider
