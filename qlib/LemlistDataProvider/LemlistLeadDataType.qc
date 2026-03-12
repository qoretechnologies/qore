# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistLeadDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Lemlist lead data type
public class LemlistLeadDataType inherits HashDataType {
    public {
        const Fields = {
            "_id": {
                "type": StringOrNothingType,
                "display_name": "Lead ID",
                "short_desc": "Unique lead identifier",
                "desc": "The unique identifier for the lead",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "Lead email address",
                "desc": "The email address of the lead",
            },
            "firstName": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "Lead first name",
                "desc": "The first name of the lead",
            },
            "lastName": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Lead last name",
                "desc": "The last name of the lead",
            },
            "companyName": {
                "type": StringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "Lead company name",
                "desc": "The name of the company the lead works for",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Lead phone number",
                "desc": "The phone number of the lead",
            },
            "linkedinUrl": {
                "type": StringOrNothingType,
                "display_name": "LinkedIn URL",
                "short_desc": "Lead LinkedIn profile URL",
                "desc": "The LinkedIn profile URL of the lead",
            },
            "website": {
                "type": StringOrNothingType,
                "display_name": "Website",
                "short_desc": "Lead or company website",
                "desc": "The website URL associated with the lead",
            },
            "campaignId": {
                "type": StringOrNothingType,
                "display_name": "Campaign ID",
                "short_desc": "Associated campaign identifier",
                "desc": "The identifier of the campaign this lead belongs to",
            },
            "isPaused": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Paused",
                "short_desc": "Whether outreach to this lead is paused",
                "desc": "`True` if outreach to this lead is currently paused",
            },
            "isUnsubscribed": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Unsubscribed",
                "short_desc": "Whether the lead has unsubscribed",
                "desc": "`True` if the lead has unsubscribed from outreach",
            },
            "isBounced": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Bounced",
                "short_desc": "Whether emails to this lead have bounced",
                "desc": "`True` if emails sent to this lead have bounced",
            },
            "isInterested": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Interested",
                "short_desc": "Whether the lead has expressed interest",
                "desc": "`True` if the lead has been marked as interested",
            },
            "createdAt": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Lead creation timestamp",
                "desc": "ISO 8601 timestamp when the lead was added",
            },
            "updatedAt": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Lead last update timestamp",
                "desc": "ISO 8601 timestamp when the lead was last updated",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace LemlistDataProvider
