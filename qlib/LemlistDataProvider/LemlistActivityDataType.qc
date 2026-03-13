# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistActivityDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Lemlist activity data type
public class LemlistActivityDataType inherits HashDataType {
    public {
        const Fields = {
            "_id": {
                "type": StringOrNothingType,
                "display_name": "Activity ID",
                "short_desc": "Unique activity identifier",
                "desc": "The unique identifier for the activity",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "Activity type",
                "desc": "The type of activity (e.g., `emailsSent`, `emailsOpened`, `emailsClicked`)",
            },
            "campaignId": {
                "type": StringOrNothingType,
                "display_name": "Campaign ID",
                "short_desc": "Associated campaign identifier",
                "desc": "The identifier of the campaign this activity belongs to",
            },
            "campaignName": {
                "type": StringOrNothingType,
                "display_name": "Campaign Name",
                "short_desc": "Campaign name",
                "desc": "The name of the campaign associated with this activity",
            },
            "leadId": {
                "type": StringOrNothingType,
                "display_name": "Lead ID",
                "short_desc": "Associated lead identifier",
                "desc": "The identifier of the lead associated with this activity",
            },
            "leadEmail": {
                "type": StringOrNothingType,
                "display_name": "Lead Email",
                "short_desc": "Lead email address",
                "desc": "The email address of the lead associated with this activity",
            },
            "sequenceStepId": {
                "type": StringOrNothingType,
                "display_name": "Sequence Step ID",
                "short_desc": "Associated sequence step identifier",
                "desc": "The identifier of the sequence step that triggered this activity",
            },
            "isFirst": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is First",
                "short_desc": "Whether this is the first occurrence",
                "desc": "`True` if this is the first time this activity type occurred for this lead in this campaign",
            },
            "createdAt": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Activity timestamp",
                "desc": "ISO 8601 timestamp when the activity occurred",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace LemlistDataProvider
