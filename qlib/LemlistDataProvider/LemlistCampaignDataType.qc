# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistCampaignDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LemlistDataProvider module
public namespace LemlistDataProvider {

#! Lemlist campaign data type
public class LemlistCampaignDataType inherits HashDataType {
    public {
        const Fields = {
            "_id": {
                "type": StringOrNothingType,
                "display_name": "Campaign ID",
                "short_desc": "Unique campaign identifier",
                "desc": "The unique identifier for the campaign",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Campaign name",
                "desc": "The name of the campaign",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Campaign status",
                "desc": "The current status of the campaign",
            },
            "isPaused": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Paused",
                "short_desc": "Whether the campaign is paused",
                "desc": "`True` if the campaign is currently paused",
            },
            "labels": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Labels",
                "short_desc": "Campaign labels",
                "desc": "Labels applied to the campaign for organization",
            },
            "senders": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Senders",
                "short_desc": "Email senders for the campaign",
                "desc": "List of email accounts used to send messages in this campaign",
            },
            "sequenceRef": {
                "type": StringOrNothingType,
                "display_name": "Sequence Reference",
                "short_desc": "Reference to the campaign sequence",
                "desc": "The identifier of the sequence associated with this campaign",
            },
            "scheduleRef": {
                "type": StringOrNothingType,
                "display_name": "Schedule Reference",
                "short_desc": "Reference to the sending schedule",
                "desc": "The identifier of the schedule used by this campaign",
            },
            "createdAt": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Campaign creation timestamp",
                "desc": "ISO 8601 timestamp when the campaign was created",
            },
            "updatedAt": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Campaign last update timestamp",
                "desc": "ISO 8601 timestamp when the campaign was last updated",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace LemlistDataProvider
