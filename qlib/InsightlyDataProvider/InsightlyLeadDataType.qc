# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InsightlyDataProvider module definition

/*  InsightlyLeadDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InsightlyDataProvider module
public namespace InsightlyDataProvider {

#! Insightly lead record type
public class InsightlyLeadDataType inherits HashDataType {
    public {
        const Fields = {
            "LEAD_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Lead ID",
                "short_desc": "The unique lead identifier",
                "desc": "The unique identifier of the lead",
            },
            "FIRST_NAME": {
                "type": SoftStringOrNothingType,
                "display_name": "First Name",
                "short_desc": "Lead's first name",
                "desc": "The first name of the lead",
            },
            "LAST_NAME": {
                "type": SoftStringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Lead's last name",
                "desc": "The last name of the lead",
            },
            "SALUTATION": {
                "type": SoftStringOrNothingType,
                "display_name": "Salutation",
                "short_desc": "Lead's salutation",
                "desc": "The salutation for the lead (e.g., `Mr`, `Ms`, `Dr`)",
            },
            "TITLE": {
                "type": SoftStringOrNothingType,
                "display_name": "Title",
                "short_desc": "Job title",
                "desc": "The job title of the lead",
            },
            "EMAIL": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Email address",
                "desc": "The email address of the lead",
            },
            "PHONE": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The phone number of the lead",
            },
            "MOBILE": {
                "type": SoftStringOrNothingType,
                "display_name": "Mobile",
                "short_desc": "Mobile phone number",
                "desc": "The mobile phone number of the lead",
            },
            "FAX": {
                "type": SoftStringOrNothingType,
                "display_name": "Fax",
                "short_desc": "Fax number",
                "desc": "The fax number of the lead",
            },
            "LEAD_SOURCE_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Lead Source ID",
                "short_desc": "The lead source identifier",
                "desc": "The ID of the lead source",
            },
            "LEAD_STATUS_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Lead Status ID",
                "short_desc": "The lead status identifier",
                "desc": "The ID of the lead status",
            },
            "INDUSTRY": {
                "type": SoftStringOrNothingType,
                "display_name": "Industry",
                "short_desc": "Industry sector",
                "desc": "The industry sector of the lead",
            },
            "EMPLOYEE_COUNT": {
                "type": SoftIntOrNothingType,
                "display_name": "Employee Count",
                "short_desc": "Number of employees",
                "desc": "The number of employees at the lead's company",
            },
            "LEAD_RATING": {
                "type": SoftIntOrNothingType,
                "display_name": "Lead Rating",
                "short_desc": "Rating from 1 to 5",
                "desc": "The rating of the lead on a scale of `1` to `5`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 1, "display_name": "1 - Lowest",
                        "desc": "Rating 1 (lowest)"},
                    <AllowedValueInfo>{"value": 2, "display_name": "2 - Low",
                        "desc": "Rating 2"},
                    <AllowedValueInfo>{"value": 3, "display_name": "3 - Medium",
                        "desc": "Rating 3"},
                    <AllowedValueInfo>{"value": 4, "display_name": "4 - High",
                        "desc": "Rating 4"},
                    <AllowedValueInfo>{"value": 5, "display_name": "5 - Highest",
                        "desc": "Rating 5 (highest)"},
                ),
            },
            "ORGANISATION_NAME": {
                "type": SoftStringOrNothingType,
                "display_name": "Organisation Name",
                "short_desc": "Company name",
                "desc": "The name of the lead's company or organisation",
            },
            "WEBSITE": {
                "type": SoftStringOrNothingType,
                "display_name": "Website",
                "short_desc": "Website URL",
                "desc": "The website URL of the lead",
            },
            "LEAD_DESCRIPTION": {
                "type": SoftStringOrNothingType,
                "display_name": "Lead Description",
                "short_desc": "Description of the lead",
                "desc": "A description or notes about the lead",
            },
            "RESPONSIBLE_USER_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Responsible User ID",
                "short_desc": "Responsible user identifier",
                "desc": "The ID of the user responsible for this lead",
            },
            "OWNER_USER_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Owner User ID",
                "short_desc": "The owner user identifier",
                "desc": "The ID of the user who owns this lead",
            },
            "VISIBLE_TO": {
                "type": SoftStringOrNothingType,
                "display_name": "Visible To",
                "short_desc": "Visibility setting",
                "desc": "Who can see this lead",
                "allowed_values": VisibleToAllowedValues,
            },
            "CONVERTED": {
                "type": SoftBoolOrNothingType,
                "display_name": "Converted",
                "short_desc": "Whether the lead has been converted",
                "desc": "If `True`, the lead has been converted to a contact/organisation. "
                    "Read-only",
            },
            "CONVERTED_CONTACT_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Converted Contact ID",
                "short_desc": "Converted contact identifier",
                "desc": "The ID of the contact created from this lead conversion. Read-only",
            },
            "CONVERTED_DATE_UTC": {
                "type": SoftDateOrNothingType,
                "display_name": "Converted Date",
                "short_desc": "Conversion timestamp (UTC)",
                "desc": "The UTC timestamp when the lead was converted. Read-only",
            },
            "CONVERTED_ORGANISATION_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Converted Organisation ID",
                "short_desc": "Converted organisation identifier",
                "desc": "The ID of the organisation created from this lead conversion. "
                    "Read-only",
            },
            "CONVERTED_OPPORTUNITY_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Converted Opportunity ID",
                "short_desc": "Converted opportunity identifier",
                "desc": "The ID of the opportunity created from this lead conversion. "
                    "Read-only",
            },
            "CUSTOMFIELDS": {
                "type": new ListDataType("InsightlyLeadCustomFieldList",
                    new InsightlyCustomFieldDataType(), True),
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "A list of custom field values for the lead",
            },
            "ADDRESSES": {
                "type": new ListDataType("InsightlyLeadAddressList",
                    new InsightlyAddressDataType(), True),
                "display_name": "Addresses",
                "short_desc": "Lead addresses",
                "desc": "A list of addresses for the lead",
            },
            "TAGS": {
                "type": new ListDataType("InsightlyLeadTagList",
                    new InsightlyTagDataType(), True),
                "display_name": "Tags",
                "short_desc": "Lead tags",
                "desc": "A list of tags applied to the lead",
            },
            "DATE_CREATED_UTC": {
                "type": SoftDateOrNothingType,
                "display_name": "Date Created",
                "short_desc": "Creation timestamp (UTC)",
                "desc": "The UTC timestamp when the lead was created",
            },
            "DATE_UPDATED_UTC": {
                "type": SoftDateOrNothingType,
                "display_name": "Date Updated",
                "short_desc": "Last update timestamp (UTC)",
                "desc": "The UTC timestamp when the lead was last updated",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Insightly lead request data type for create operations
public class InsightlyCreateLeadRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "LAST_NAME": {
                "type": SoftStringType,
                "display_name": "Last Name",
                "required": True,
                "short_desc": "Lead's last name",
                "desc": "The last name of the lead (required)",
                "example_value": "Johnson",
            },
            "FIRST_NAME": {
                "type": SoftStringOrNothingType,
                "display_name": "First Name",
                "short_desc": "Lead's first name",
                "desc": "The first name of the lead",
                "preselected": True,
                "example_value": "Robert",
            },
            "SALUTATION": {
                "type": SoftStringOrNothingType,
                "display_name": "Salutation",
                "short_desc": "Lead's salutation",
                "desc": "The salutation for the lead (e.g., `Mr`, `Ms`, `Dr`)",
            },
            "TITLE": {
                "type": SoftStringOrNothingType,
                "display_name": "Title",
                "short_desc": "Job title",
                "desc": "The job title of the lead",
                "preselected": True,
            },
            "EMAIL": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Email address",
                "desc": "The email address of the lead",
                "preselected": True,
                "example_value": "robert.johnson@example.com",
            },
            "PHONE": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The phone number of the lead",
                "preselected": True,
            },
            "MOBILE": {
                "type": SoftStringOrNothingType,
                "display_name": "Mobile",
                "short_desc": "Mobile phone number",
                "desc": "The mobile phone number of the lead",
            },
            "FAX": {
                "type": SoftStringOrNothingType,
                "display_name": "Fax",
                "short_desc": "Fax number",
                "desc": "The fax number of the lead",
            },
            "LEAD_SOURCE_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Lead Source ID",
                "short_desc": "The lead source identifier",
                "desc": "The ID of the lead source",
                "ref_data": "lead_sources",
            },
            "LEAD_STATUS_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Lead Status ID",
                "short_desc": "The lead status identifier",
                "desc": "The ID of the lead status",
                "ref_data": "lead_statuses",
            },
            "INDUSTRY": {
                "type": SoftStringOrNothingType,
                "display_name": "Industry",
                "short_desc": "Industry sector",
                "desc": "The industry sector of the lead",
            },
            "EMPLOYEE_COUNT": {
                "type": SoftIntOrNothingType,
                "display_name": "Employee Count",
                "short_desc": "Number of employees",
                "desc": "The number of employees at the lead's company",
            },
            "LEAD_RATING": {
                "type": SoftIntOrNothingType,
                "display_name": "Lead Rating",
                "short_desc": "Rating from 1 to 5",
                "desc": "The rating of the lead on a scale of `1` to `5`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 1, "display_name": "1 - Lowest",
                        "desc": "Rating 1 (lowest)"},
                    <AllowedValueInfo>{"value": 2, "display_name": "2 - Low",
                        "desc": "Rating 2"},
                    <AllowedValueInfo>{"value": 3, "display_name": "3 - Medium",
                        "desc": "Rating 3"},
                    <AllowedValueInfo>{"value": 4, "display_name": "4 - High",
                        "desc": "Rating 4"},
                    <AllowedValueInfo>{"value": 5, "display_name": "5 - Highest",
                        "desc": "Rating 5 (highest)"},
                ),
            },
            "ORGANISATION_NAME": {
                "type": SoftStringOrNothingType,
                "display_name": "Organisation Name",
                "short_desc": "Company name",
                "desc": "The name of the lead's company or organisation",
            },
            "WEBSITE": {
                "type": SoftStringOrNothingType,
                "display_name": "Website",
                "short_desc": "Website URL",
                "desc": "The website URL of the lead",
            },
            "LEAD_DESCRIPTION": {
                "type": SoftStringOrNothingType,
                "display_name": "Lead Description",
                "short_desc": "Description of the lead",
                "desc": "A description or notes about the lead",
            },
            "RESPONSIBLE_USER_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Responsible User ID",
                "short_desc": "Responsible user identifier",
                "desc": "The ID of the user responsible for this lead",
                "ref_data": "users",
            },
            "OWNER_USER_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Owner User ID",
                "short_desc": "The owner user identifier",
                "desc": "The ID of the user who owns this lead",
                "ref_data": "users",
            },
            "VISIBLE_TO": {
                "type": SoftStringOrNothingType,
                "display_name": "Visible To",
                "short_desc": "Visibility setting",
                "desc": "Who can see this lead",
                "allowed_values": VisibleToAllowedValues,
            },
            "CUSTOMFIELDS": {
                "type": new ListDataType("InsightlyLeadCustomFieldList",
                    new InsightlyCustomFieldDataType(), True),
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "A list of custom field values for the lead",
            },
            "ADDRESSES": {
                "type": new ListDataType("InsightlyLeadAddressList",
                    new InsightlyAddressDataType(), True),
                "display_name": "Addresses",
                "short_desc": "Lead addresses",
                "desc": "A list of addresses for the lead",
            },
            "TAGS": {
                "type": new ListDataType("InsightlyLeadTagList",
                    new InsightlyTagDataType(), True),
                "display_name": "Tags",
                "short_desc": "Lead tags",
                "desc": "A list of tags to apply to the lead",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Insightly lead request data type for update operations
public class InsightlyUpdateLeadRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "LEAD_ID": {
                "type": SoftIntType,
                "display_name": "Lead ID",
                "required": True,
                "short_desc": "The lead ID to update",
                "desc": "The unique identifier of the lead to update",
                "ref_data": "leads",
            },
        } + InsightlyDataProviderBase::makeFieldsOptional(
            InsightlyCreateLeadRequestDataType::Fields);
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Insightly lead request data type for delete operations
public class InsightlyDeleteLeadRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "LEAD_ID": {
                "type": SoftIntType,
                "display_name": "Lead ID",
                "required": True,
                "short_desc": "The lead ID to delete",
                "desc": "The unique identifier of the lead to delete",
                "ref_data": "leads",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace InsightlyDataProvider
