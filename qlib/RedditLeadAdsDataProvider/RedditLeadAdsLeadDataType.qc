# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedditLeadAdsDataProvider module definition

/*  RedditLeadAdsLeadDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the RedditLeadAdsDataProvider module
public namespace RedditLeadAdsDataProvider {

#! Reddit Lead Ads lead submission record type
public class RedditLeadAdsLeadDataType inherits HashDataType {
    public {
        #! Field definitions for Reddit lead form submissions
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The lead submission ID",
                "desc": "The unique identifier of the lead form submission",
            },
            "created_at": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the lead was submitted",
                "desc": "The timestamp when the lead form was submitted",
            },
            "campaign_id": {
                "type": StringOrNothingType,
                "display_name": "Campaign ID",
                "short_desc": "The advertising campaign ID",
                "desc": "The ID of the Reddit advertising campaign that generated this lead",
            },
            "campaign_name": {
                "type": StringOrNothingType,
                "display_name": "Campaign Name",
                "short_desc": "The advertising campaign name",
                "desc": "The name of the Reddit advertising campaign that generated this lead",
            },
            "ad_group_id": {
                "type": StringOrNothingType,
                "display_name": "Ad Group ID",
                "short_desc": "The ad group ID",
                "desc": "The ID of the ad group that generated this lead",
            },
            "ad_id": {
                "type": StringOrNothingType,
                "display_name": "Ad ID",
                "short_desc": "The ad ID",
                "desc": "The ID of the specific ad that generated this lead",
            },
            "form_id": {
                "type": StringOrNothingType,
                "display_name": "Form ID",
                "short_desc": "The lead form ID",
                "desc": "The ID of the lead generation form template",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The lead's email address",
                "desc": "The email address submitted by the lead",
                "example_value": "user@example.com",
            },
            "full_name": {
                "type": StringOrNothingType,
                "display_name": "Full Name",
                "short_desc": "The lead's full name",
                "desc": "The full name submitted by the lead",
                "example_value": "Jane Smith",
            },
            "job_title": {
                "type": StringOrNothingType,
                "display_name": "Job Title",
                "short_desc": "The lead's job title",
                "desc": "The job title submitted by the lead",
                "example_value": "Product Manager",
            },
            "company_name": {
                "type": StringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "The lead's company",
                "desc": "The company name submitted by the lead",
                "example_value": "Acme Corp",
            },
            "phone_number": {
                "type": StringOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "The lead's phone number",
                "desc": "The phone number submitted by the lead",
                "example_value": "+1-555-0123",
            },
            "custom_fields": {
                "type": AutoHashOrNothingType,
                "display_name": "Custom Fields",
                "short_desc": "Additional custom form fields",
                "desc": "A hash of any additional custom fields configured in the lead form",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace RedditLeadAdsDataProvider
