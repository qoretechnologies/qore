# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyCampaignDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InstantlyDataProvider module
public namespace InstantlyDataProvider {

#! Instantly campaign data type
public class InstantlyCampaignDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Campaign ID",
                "short_desc": "Campaign UUID",
                "type": StringOrNothingType,
                "desc": "The unique identifier (UUID) of the campaign",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Campaign name",
                "type": StringType,
                "desc": "The name of the campaign",
                "required": True,
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Campaign status code",
                "type": IntOrNothingType,
                "desc": "The status of the campaign: `0` = Draft, `1` = Active, `2` = Paused, "
                    "`3` = Completed, `4` = Running Subsequences",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 0, "display_name": "Draft"},
                    <AllowedValueInfo>{"value": 1, "display_name": "Active"},
                    <AllowedValueInfo>{"value": 2, "display_name": "Paused"},
                    <AllowedValueInfo>{"value": 3, "display_name": "Completed"},
                    <AllowedValueInfo>{"value": 4, "display_name": "Running Subsequences"},
                ),
            },
            "email_list": {
                "display_name": "Email List",
                "short_desc": "Account emails for sending",
                "type": SoftAutoListOrNothingType,
                "desc": "List of account email addresses used for sending in this campaign",
            },
            "daily_limit": {
                "display_name": "Daily Limit",
                "short_desc": "Daily sending cap",
                "type": IntOrNothingType,
                "desc": "The maximum number of emails that can be sent per day for this campaign",
            },
            "link_tracking": {
                "display_name": "Link Tracking",
                "short_desc": "Link tracking enabled",
                "type": BoolOrNothingType,
                "desc": "Whether link tracking is enabled for this campaign",
            },
            "open_tracking": {
                "display_name": "Open Tracking",
                "short_desc": "Open tracking enabled",
                "type": BoolOrNothingType,
                "desc": "Whether open tracking is enabled for this campaign",
            },
            "created_at": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "type": DateOrNothingType,
                "desc": "The date and time the campaign was created",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace InstantlyDataProvider
