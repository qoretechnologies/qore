# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInvoiceDataProvider module definition

/*  ZohoInvoiceEstimateDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoInvoiceDataProvider module
public namespace ZohoInvoiceDataProvider {

#! Zoho Invoice estimate data type
public class ZohoInvoiceEstimateDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "estimate_id": {
                "display_name": "Estimate ID",
                "short_desc": "Unique identifier for the estimate",
                "desc": "The unique identifier for this estimate",
                "type": SoftStringType,
            },
            "estimate_number": {
                "display_name": "Estimate Number",
                "short_desc": "Estimate number",
                "desc": "The estimate number visible to customers",
                "type": SoftStringOrNothingType,
            },
            "customer_id": {
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The ID of the customer for this estimate",
                "type": SoftStringType,
            },
            "customer_name": {
                "display_name": "Customer Name",
                "short_desc": "Customer name",
                "desc": "The name of the customer",
                "type": SoftStringOrNothingType,
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Estimate status",
                "desc": "Status of the estimate",
                "type": SoftStringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "draft", "desc": "Draft estimate"},
                    <AllowedValueInfo>{"value": "sent", "desc": "Sent estimate"},
                    <AllowedValueInfo>{"value": "accepted", "desc": "Accepted estimate"},
                    <AllowedValueInfo>{"value": "declined", "desc": "Declined estimate"},
                    <AllowedValueInfo>{"value": "invoiced", "desc": "Invoiced estimate"},
                ),
            },
            "date": {
                "display_name": "Estimate Date",
                "short_desc": "Date of the estimate",
                "desc": "The date of the estimate (`YYYY-MM-DD` format)",
                "type": SoftStringOrNothingType,
            },
            "expiry_date": {
                "display_name": "Expiry Date",
                "short_desc": "Expiration date",
                "desc": "The date the estimate expires",
                "type": SoftStringOrNothingType,
            },
            "currency_id": {
                "display_name": "Currency ID",
                "short_desc": "Currency identifier",
                "desc": "The currency ID for the estimate",
                "type": SoftStringOrNothingType,
            },
            "currency_code": {
                "display_name": "Currency Code",
                "short_desc": "Currency code",
                "desc": "The currency code (e.g., USD, EUR)",
                "type": SoftStringOrNothingType,
            },
            "sub_total": {
                "display_name": "Subtotal",
                "short_desc": "Subtotal before tax",
                "desc": "The subtotal amount before taxes and discounts",
                "type": SoftFloatOrNothingType,
            },
            "tax_total": {
                "display_name": "Tax Total",
                "short_desc": "Total tax amount",
                "desc": "The total tax amount",
                "type": SoftFloatOrNothingType,
            },
            "discount": {
                "display_name": "Discount",
                "short_desc": "Discount amount",
                "desc": "The discount amount or percentage",
                "type": SoftFloatOrNothingType,
            },
            "total": {
                "display_name": "Total",
                "short_desc": "Estimate total",
                "desc": "The total amount of the estimate",
                "type": SoftFloatOrNothingType,
            },
            "reference_number": {
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "An optional reference number",
                "type": SoftStringOrNothingType,
            },
            "line_items": {
                "display_name": "Line Items",
                "short_desc": "Estimate line items",
                "desc": "The line items on the estimate",
                "type": new SoftListDataType("LineItems", new ZohoInvoiceLineItemDataType(), True),
            },
            "notes": {
                "display_name": "Notes",
                "short_desc": "Estimate notes",
                "desc": "Notes to display on the estimate",
                "type": SoftStringOrNothingType,
            },
            "terms": {
                "display_name": "Terms & Conditions",
                "short_desc": "Terms and conditions",
                "desc": "Terms and conditions to display",
                "type": SoftStringOrNothingType,
            },
            "project_id": {
                "display_name": "Project ID",
                "short_desc": "Associated project",
                "desc": "The project ID this estimate is associated with",
                "type": SoftStringOrNothingType,
            },
            "created_time": {
                "display_name": "Created Time",
                "short_desc": "When the estimate was created",
                "desc": "The timestamp when the estimate was created",
                "type": SoftStringOrNothingType,
            },
            "last_modified_time": {
                "display_name": "Last Modified Time",
                "short_desc": "When the estimate was last modified",
                "desc": "The timestamp when the estimate was last modified",
                "type": SoftStringOrNothingType,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Zoho Invoice create estimate request data type
public class ZohoInvoiceCreateEstimateRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating an estimate
        const Fields = {
            "customer_id": {
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The ID of the customer for this estimate",
                "type": SoftStringType,
                "required": True,
            },
            "date": {
                "display_name": "Estimate Date",
                "short_desc": "Date of the estimate",
                "desc": "The date of the estimate (`YYYY-MM-DD` format)",
                "type": SoftStringOrNothingType,
            },
            "expiry_date": {
                "display_name": "Expiry Date",
                "short_desc": "Expiration date",
                "desc": "The date the estimate expires",
                "type": SoftStringOrNothingType,
            },
            "estimate_number": {
                "display_name": "Estimate Number",
                "short_desc": "Custom estimate number",
                "desc": "Custom estimate number (auto-generated if not provided)",
                "type": SoftStringOrNothingType,
            },
            "reference_number": {
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "An optional reference number",
                "type": SoftStringOrNothingType,
            },
            "line_items": {
                "display_name": "Line Items",
                "short_desc": "Estimate line items",
                "desc": "The line items on the estimate",
                "type": new SoftListDataType("LineItems", new ZohoInvoiceLineItemDataType(), True),
                "required": True,
            },
            "discount": {
                "display_name": "Discount",
                "short_desc": "Discount amount",
                "desc": "The discount amount or percentage",
                "type": SoftFloatOrNothingType,
            },
            "notes": {
                "display_name": "Notes",
                "short_desc": "Estimate notes",
                "desc": "Notes to display on the estimate",
                "type": SoftStringOrNothingType,
            },
            "terms": {
                "display_name": "Terms & Conditions",
                "short_desc": "Terms and conditions",
                "desc": "Terms and conditions to display",
                "type": SoftStringOrNothingType,
            },
            "project_id": {
                "display_name": "Project ID",
                "short_desc": "Associated project",
                "desc": "The project ID this estimate is associated with",
                "type": SoftStringOrNothingType,
            },
            "custom_fields": {
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "Custom field values for the estimate",
                "type": AutoHashOrNothingType,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ZohoInvoiceDataProvider
