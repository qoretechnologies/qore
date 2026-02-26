# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInvoiceDataProvider module definition

/*  ZohoInvoiceProjectDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Invoice project data type
public class ZohoInvoiceProjectDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "project_id": {
                "display_name": "Project ID",
                "short_desc": "Unique identifier for the project",
                "desc": "The unique identifier for this project",
                "type": SoftStringType,
            },
            "project_name": {
                "display_name": "Project Name",
                "short_desc": "Name of the project",
                "desc": "The name of the project",
                "type": SoftStringType,
            },
            "customer_id": {
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The ID of the customer for this project",
                "type": SoftStringType,
            },
            "customer_name": {
                "display_name": "Customer Name",
                "short_desc": "Customer name",
                "desc": "The name of the customer",
                "type": SoftStringOrNothingType,
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Project description",
                "desc": "Description of the project",
                "type": SoftStringOrNothingType,
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Project status",
                "desc": "Status of the project",
                "type": SoftStringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "active", "desc": "Active project"},
                    <AllowedValueInfo>{"value": "inactive", "desc": "Inactive project"},
                ),
            },
            "billing_type": {
                "display_name": "Billing Type",
                "short_desc": "Type of billing",
                "desc": "The billing type (fixed_cost_for_project, based_on_project_hours, based_on_task_hours, based_on_staff_hours)",
                "type": SoftStringOrNothingType,
            },
            "rate": {
                "display_name": "Rate",
                "short_desc": "Billing rate",
                "desc": "The billing rate for the project",
                "type": SoftFloatOrNothingType,
            },
            "budget_type": {
                "display_name": "Budget Type",
                "short_desc": "Type of budget",
                "desc": "The budget type (total_project_cost, hours_per_project, hours_per_task, hours_per_staff)",
                "type": SoftStringOrNothingType,
            },
            "budget_hours": {
                "display_name": "Budget Hours",
                "short_desc": "Budgeted hours",
                "desc": "The budgeted hours for the project",
                "type": SoftFloatOrNothingType,
            },
            "budget_amount": {
                "display_name": "Budget Amount",
                "short_desc": "Budget amount",
                "desc": "The budget amount for the project",
                "type": SoftFloatOrNothingType,
            },
            "total_hours": {
                "display_name": "Total Hours",
                "short_desc": "Total logged hours",
                "desc": "The total hours logged on the project",
                "type": SoftFloatOrNothingType,
            },
            "total_cost": {
                "display_name": "Total Cost",
                "short_desc": "Total cost",
                "desc": "The total cost of the project",
                "type": SoftFloatOrNothingType,
            },
            "billed_hours": {
                "display_name": "Billed Hours",
                "short_desc": "Billed hours",
                "desc": "The hours that have been billed",
                "type": SoftFloatOrNothingType,
            },
            "billed_amount": {
                "display_name": "Billed Amount",
                "short_desc": "Billed amount",
                "desc": "The amount that has been billed",
                "type": SoftFloatOrNothingType,
            },
            "unbilled_hours": {
                "display_name": "Unbilled Hours",
                "short_desc": "Unbilled hours",
                "desc": "The hours that have not been billed",
                "type": SoftFloatOrNothingType,
            },
            "unbilled_amount": {
                "display_name": "Unbilled Amount",
                "short_desc": "Unbilled amount",
                "desc": "The amount that has not been billed",
                "type": SoftFloatOrNothingType,
            },
            "currency_code": {
                "display_name": "Currency Code",
                "short_desc": "Currency code",
                "desc": "The currency code (e.g., USD, EUR)",
                "type": SoftStringOrNothingType,
            },
            "created_time": {
                "display_name": "Created Time",
                "short_desc": "When the project was created",
                "desc": "The timestamp when the project was created",
                "type": SoftStringOrNothingType,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Zoho Invoice create project request data type
public class ZohoInvoiceCreateProjectRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a project
        const Fields = {
            "project_name": {
                "display_name": "Project Name",
                "short_desc": "Name of the project",
                "desc": "The name of the project",
                "type": SoftStringType,
                "required": True,
            },
            "customer_id": {
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The ID of the customer for this project",
                "type": SoftStringType,
                "required": True,
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Project description",
                "desc": "Description of the project",
                "type": SoftStringOrNothingType,
            },
            "billing_type": {
                "display_name": "Billing Type",
                "short_desc": "Type of billing",
                "desc": "The billing type (fixed_cost_for_project, based_on_project_hours, based_on_task_hours, based_on_staff_hours)",
                "type": SoftStringOrNothingType,
            },
            "rate": {
                "display_name": "Rate",
                "short_desc": "Billing rate",
                "desc": "The billing rate for the project",
                "type": SoftFloatOrNothingType,
            },
            "budget_type": {
                "display_name": "Budget Type",
                "short_desc": "Type of budget",
                "desc": "The budget type (total_project_cost, hours_per_project, hours_per_task, hours_per_staff)",
                "type": SoftStringOrNothingType,
            },
            "budget_hours": {
                "display_name": "Budget Hours",
                "short_desc": "Budgeted hours",
                "desc": "The budgeted hours for the project",
                "type": SoftFloatOrNothingType,
            },
            "budget_amount": {
                "display_name": "Budget Amount",
                "short_desc": "Budget amount",
                "desc": "The budget amount for the project",
                "type": SoftFloatOrNothingType,
            },
            "custom_fields": {
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "Custom field values for the project",
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
