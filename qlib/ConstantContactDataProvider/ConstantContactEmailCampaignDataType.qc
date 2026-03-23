# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ConstantContactEmailCampaignDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace ConstantContactDataProvider {

#! Constant Contact email campaign data type
public class ConstantContactEmailCampaignDataType inherits HashDataType {
    public {
        const Fields = {
            "campaign_id": {
                "type": StringType,
                "display_name": "Campaign ID",
                "short_desc": "The unique campaign identifier",
                "desc": "The unique identifier for the email campaign (UUID format)",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The campaign name",
                "desc": "The name of the email campaign",
                "example_value": "Monthly Newsletter - March 2026",
            },
            "current_status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The current campaign status",
                "desc": "The current status of the campaign",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Draft", "display_name": "Draft",
                        "desc": "Campaign is in draft state"},
                    <AllowedValueInfo>{"value": "Scheduled", "display_name": "Scheduled",
                        "desc": "Campaign is scheduled for sending"},
                    <AllowedValueInfo>{"value": "Executing", "display_name": "Executing",
                        "desc": "Campaign is currently being sent"},
                    <AllowedValueInfo>{"value": "Done", "display_name": "Done",
                        "desc": "Campaign has been sent"},
                    <AllowedValueInfo>{"value": "Error", "display_name": "Error",
                        "desc": "Campaign encountered an error"},
                    <AllowedValueInfo>{"value": "Removed", "display_name": "Removed",
                        "desc": "Campaign was removed"},
                ),
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The campaign format type",
                "desc": "The format type of the campaign",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "CUSTOM_CODE_EMAIL",
                        "display_name": "Custom Code Email",
                        "desc": "An email created with custom HTML code"},
                    <AllowedValueInfo>{"value": "NEWSLETTER",
                        "display_name": "Newsletter",
                        "desc": "A newsletter campaign"},
                    <AllowedValueInfo>{"value": "PRODUCT_UPDATE",
                        "display_name": "Product Update",
                        "desc": "A product update campaign"},
                    <AllowedValueInfo>{"value": "ANNOUNCEMENT",
                        "display_name": "Announcement",
                        "desc": "An announcement campaign"},
                ),
            },
            "type_code": {
                "type": IntOrNothingType,
                "display_name": "Type Code",
                "short_desc": "The numeric type code",
                "desc": "A numeric code representing the campaign type",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the campaign was created",
                "desc": "The timestamp when the campaign was created (ISO-8601 format)",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the campaign was last updated",
                "desc": "The timestamp when the campaign was last updated (ISO-8601 format)",
            },
            "last_sent_date": {
                "type": StringOrNothingType,
                "display_name": "Last Sent Date",
                "short_desc": "When the campaign was last sent",
                "desc": "The date the campaign was last sent (ISO-8601 format)",
            },
            "campaign_activities": {
                "type": new ListDataType("CampaignActivityRefList",
                    new ConstantContactCampaignActivityRefDataType(), True),
                "display_name": "Campaign Activities",
                "short_desc": "Associated campaign activities",
                "desc": "A list of campaign activity objects associated with this campaign, "
                    "each containing `campaign_activity_id`, `role`, and `contact_list_ids`",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact create email campaign request data type
public class ConstantContactCreateEmailCampaignRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The campaign name",
                "desc": "The name of the email campaign",
                "example_value": "Monthly Newsletter",
            },
            "email_campaign_activities": {
                "type": new ListDataType("EmailCampaignActivityList",
                    new ConstantContactEmailCampaignActivityDataType(), True),
                "display_name": "Campaign Activities",
                "short_desc": "Campaign activity configurations",
                "desc": "A list of email campaign activity objects. Each activity includes "
                    "`format_type`, `from_email`, `from_name`, `reply_to_email`, `subject`, "
                    "`html_content`, and `contact_list_ids`",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact schedule email campaign request data type
public class ConstantContactScheduleEmailCampaignRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "campaign_activity_id": {
                "type": StringType,
                "display_name": "Campaign Activity ID",
                "short_desc": "The campaign activity to schedule",
                "desc": "The unique identifier of the campaign activity to schedule",
            },
            "scheduled_date": {
                "type": StringType,
                "display_name": "Scheduled Date",
                "short_desc": "When to send the campaign",
                "desc": "The date and time to send the campaign in ISO-8601 format "
                    "(e.g., `2026-04-01T09:00:00.000Z`)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ConstantContactDataProvider
