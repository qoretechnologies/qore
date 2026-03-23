# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ConstantContactAccountDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Constant Contact account data type
public class ConstantContactAccountDataType inherits HashDataType {
    public {
        const Fields = {
            "encoded_account_id": {
                "type": StringOrNothingType,
                "display_name": "Encoded Account ID",
                "short_desc": "The encoded account identifier",
                "desc": "The encoded unique identifier for the account",
            },
            "organization_name": {
                "type": StringOrNothingType,
                "display_name": "Organization Name",
                "short_desc": "The organization name",
                "desc": "The name of the organization associated with the account",
            },
            "first_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The account holder's first name",
                "desc": "The first name of the account holder",
            },
            "last_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The account holder's last name",
                "desc": "The last name of the account holder",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The account email",
                "desc": "The email address associated with the account",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "The account phone number",
                "desc": "The phone number associated with the account",
            },
            "website": {
                "type": StringOrNothingType,
                "display_name": "Website",
                "short_desc": "The organization's website",
                "desc": "The website URL associated with the account",
            },
            "country_code": {
                "type": StringOrNothingType,
                "display_name": "Country Code",
                "short_desc": "The two-letter country code",
                "desc": "The two-letter ISO country code for the account",
            },
            "state_code": {
                "type": StringOrNothingType,
                "display_name": "State Code",
                "short_desc": "The state/province code",
                "desc": "The state or province code for the account",
            },
            "time_zone_id": {
                "type": StringOrNothingType,
                "display_name": "Time Zone",
                "short_desc": "The account time zone",
                "desc": "The time zone ID for the account (e.g., `US/Eastern`)",
            },
            "physical_address": {
                "type": new ConstantContactPhysicalAddressDataType(AutoHashOrNothingType),
                "display_name": "Physical Address",
                "short_desc": "The physical address",
                "desc": "The physical address associated with the account, containing "
                    "`address_line1`, `address_line2`, `city`, `state_code`, `postal_code`, "
                    "and `country_code`",
            },
            "company_logo": {
                "type": new ConstantContactCompanyLogoDataType(AutoHashOrNothingType),
                "display_name": "Company Logo",
                "short_desc": "Company logo URL info",
                "desc": "The company logo information containing `url`, `external_url`, "
                    "and `internal_id` fields, if available",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact update account request data type
public class ConstantContactUpdateAccountRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "organization_name": {
                "type": StringOrNothingType,
                "display_name": "Organization Name",
                "short_desc": "The organization name",
                "desc": "The name of the organization",
            },
            "first_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The first name",
                "desc": "The first name of the account holder",
            },
            "last_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The last name",
                "desc": "The last name of the account holder",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "The phone number",
                "desc": "The phone number for the account",
            },
            "website": {
                "type": StringOrNothingType,
                "display_name": "Website",
                "short_desc": "The website URL",
                "desc": "The website URL for the account",
            },
            "country_code": {
                "type": StringOrNothingType,
                "display_name": "Country Code",
                "short_desc": "The two-letter country code",
                "desc": "The two-letter ISO country code",
            },
            "state_code": {
                "type": StringOrNothingType,
                "display_name": "State Code",
                "short_desc": "The state/province code",
                "desc": "The state or province code",
            },
            "time_zone_id": {
                "type": StringOrNothingType,
                "display_name": "Time Zone",
                "short_desc": "The time zone",
                "desc": "The time zone ID (e.g., `US/Eastern`)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact email campaign summary data type
public class ConstantContactEmailCampaignSummaryDataType inherits HashDataType {
    public {
        const Fields = {
            "campaign_id": {
                "type": StringOrNothingType,
                "display_name": "Campaign ID",
                "short_desc": "The campaign identifier",
                "desc": "The unique identifier for the email campaign",
            },
            "sends": {
                "type": IntOrNothingType,
                "display_name": "Sends",
                "short_desc": "Number of emails sent",
                "desc": "The total number of emails sent for this campaign",
            },
            "opens": {
                "type": IntOrNothingType,
                "display_name": "Opens",
                "short_desc": "Number of opens",
                "desc": "The total number of times the campaign was opened",
            },
            "clicks": {
                "type": IntOrNothingType,
                "display_name": "Clicks",
                "short_desc": "Number of clicks",
                "desc": "The total number of link clicks in the campaign",
            },
            "bounces": {
                "type": IntOrNothingType,
                "display_name": "Bounces",
                "short_desc": "Number of bounces",
                "desc": "The total number of bounced emails",
            },
            "unsubscribes": {
                "type": IntOrNothingType,
                "display_name": "Unsubscribes",
                "short_desc": "Number of unsubscribes",
                "desc": "The total number of unsubscribes from this campaign",
            },
            "abuse_reports": {
                "type": IntOrNothingType,
                "display_name": "Abuse Reports",
                "short_desc": "Number of abuse/spam reports",
                "desc": "The total number of abuse/spam reports from this campaign",
            },
            "forwards": {
                "type": IntOrNothingType,
                "display_name": "Forwards",
                "short_desc": "Number of forwards",
                "desc": "The total number of times the campaign was forwarded",
            },
            "not_opened": {
                "type": IntOrNothingType,
                "display_name": "Not Opened",
                "short_desc": "Number not opened",
                "desc": "The number of recipients who did not open the campaign",
            },
            "unique_opens": {
                "type": IntOrNothingType,
                "display_name": "Unique Opens",
                "short_desc": "Number of unique opens",
                "desc": "The number of unique contacts who opened the campaign",
            },
            "unique_clicks": {
                "type": IntOrNothingType,
                "display_name": "Unique Clicks",
                "short_desc": "Number of unique clicks",
                "desc": "The number of unique contacts who clicked a link in the campaign",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ConstantContactDataProvider
