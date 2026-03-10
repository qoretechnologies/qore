# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CloseDataProvider module definition

/*  CloseOpportunityDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CloseDataProvider module
public namespace CloseDataProvider {

#! Close opportunity record type
public class CloseOpportunityDataType inherits HashDataType {
    public {
        #! Field definitions for Close opportunities
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The opportunity ID",
                "desc": "The unique identifier of the opportunity",
            },
            "lead_id": {
                "type": StringOrNothingType,
                "display_name": "Lead ID",
                "short_desc": "The associated lead ID",
                "desc": "The unique identifier of the lead this opportunity belongs to",
            },
            "lead_name": {
                "type": StringOrNothingType,
                "display_name": "Lead Name",
                "short_desc": "The associated lead name",
                "desc": "The display name of the lead this opportunity belongs to",
            },
            "status_id": {
                "type": StringOrNothingType,
                "display_name": "Status ID",
                "short_desc": "The opportunity status ID",
                "desc": "The unique identifier of the opportunity's current status",
            },
            "status_label": {
                "type": StringOrNothingType,
                "display_name": "Status Label",
                "short_desc": "The opportunity status label",
                "desc": "The human-readable label of the opportunity's current status",
            },
            "status_type": {
                "type": StringOrNothingType,
                "display_name": "Status Type",
                "short_desc": "The opportunity status type",
                "desc": "The type of the opportunity status: `active`, `won`, or `lost`",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Active",
                        "short_desc": "Active opportunities",
                        "desc": "Active opportunity status",
                        "value": "active",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Won",
                        "short_desc": "Won opportunities",
                        "desc": "Won opportunity status",
                        "value": "won",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Lost",
                        "short_desc": "Lost opportunities",
                        "desc": "Lost opportunity status",
                        "value": "lost",
                    },
                ),
            },
            "value": {
                "type": IntOrNothingType,
                "display_name": "Value",
                "short_desc": "The opportunity value in cents",
                "desc": "The monetary value of the opportunity in cents (e.g., 10000 = $100.00)",
            },
            "value_period": {
                "type": StringOrNothingType,
                "display_name": "Value Period",
                "short_desc": "The value period",
                "desc": "The billing period for the opportunity value: `one_time`, `monthly`, or "
                    "`annual`",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "One Time",
                        "short_desc": "One-time payment",
                        "desc": "One-time billing period",
                        "value": "one_time",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Monthly",
                        "short_desc": "Monthly recurring",
                        "desc": "Monthly billing period",
                        "value": "monthly",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Annual",
                        "short_desc": "Annual recurring",
                        "desc": "Annual billing period",
                        "value": "annual",
                    },
                ),
            },
            "value_formatted": {
                "type": StringOrNothingType,
                "display_name": "Value Formatted",
                "short_desc": "The formatted value string",
                "desc": "The human-readable formatted value of the opportunity (e.g., '$100.00')",
            },
            "value_currency": {
                "type": StringOrNothingType,
                "display_name": "Value Currency",
                "short_desc": "The value currency code",
                "desc": "The ISO 4217 currency code for the opportunity value (e.g., USD)",
            },
            "confidence": {
                "type": IntOrNothingType,
                "display_name": "Confidence",
                "short_desc": "The confidence percentage",
                "desc": "The confidence percentage of winning this opportunity, from 0 to 100",
            },
            "note": {
                "type": StringOrNothingType,
                "display_name": "Note",
                "short_desc": "Notes about the opportunity",
                "desc": "A text note with additional context about the opportunity",
            },
            "contact_id": {
                "type": StringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The associated contact ID",
                "desc": "The unique identifier of the primary contact for this opportunity",
            },
            "user_id": {
                "type": StringOrNothingType,
                "display_name": "User ID",
                "short_desc": "The assigned user ID",
                "desc": "The unique identifier of the user assigned to this opportunity",
            },
            "user_name": {
                "type": StringOrNothingType,
                "display_name": "User Name",
                "short_desc": "The assigned user name",
                "desc": "The display name of the user assigned to this opportunity",
            },
            "date_won": {
                "type": StringOrNothingType,
                "display_name": "Date Won",
                "short_desc": "When the opportunity was won",
                "desc": "The date when the opportunity was marked as won (YYYY-MM-DD format)",
            },
            "date_lost": {
                "type": StringOrNothingType,
                "display_name": "Date Lost",
                "short_desc": "When the opportunity was lost",
                "desc": "The date when the opportunity was marked as lost (YYYY-MM-DD format)",
            },
            "date_created": {
                "type": StringOrNothingType,
                "display_name": "Date Created",
                "short_desc": "When the opportunity was created",
                "desc": "The timestamp when the opportunity was created (ISO 8601 format)",
            },
            "date_updated": {
                "type": StringOrNothingType,
                "display_name": "Date Updated",
                "short_desc": "When the opportunity was last updated",
                "desc": "The timestamp when the opportunity was last updated (ISO 8601 format)",
            },
            "organization_id": {
                "type": StringOrNothingType,
                "display_name": "Organization ID",
                "short_desc": "The organization ID",
                "desc": "The unique identifier of the organization this opportunity belongs to",
            },
            "created_by": {
                "type": StringOrNothingType,
                "display_name": "Created By",
                "short_desc": "The user ID who created the opportunity",
                "desc": "The unique identifier of the user who created this opportunity",
            },
            "updated_by": {
                "type": StringOrNothingType,
                "display_name": "Updated By",
                "short_desc": "The user ID who last updated the opportunity",
                "desc": "The unique identifier of the user who last updated this opportunity",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CloseDataProvider
