# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConstantContactSubDataTypes.qc Typed sub-types for structured fields in Constant Contact

/*  ConstantContactSubDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Constant Contact email address sub-type
public class ConstantContactEmailAddressDataType inherits HashDataType {
    public {
        const Fields = {
            "address": {
                "type": StringOrNothingType,
                "display_name": "Address",
                "short_desc": "The email address",
                "desc": "The email address string",
                "example_value": "john@example.com",
            },
            "permission_to_send": {
                "type": StringOrNothingType,
                "display_name": "Permission to Send",
                "short_desc": "Email sending permission status",
                "desc": "The permission status for sending emails",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "implicit",
                        "display_name": "Implicit",
                        "desc": "Implicitly granted permission",
                    },
                    <AllowedValueInfo>{
                        "value": "explicit",
                        "display_name": "Explicit",
                        "desc": "Explicitly granted permission",
                    },
                    <AllowedValueInfo>{
                        "value": "not_set",
                        "display_name": "Not Set",
                        "desc": "Permission not yet set",
                    },
                    <AllowedValueInfo>{
                        "value": "pending_confirmation",
                        "display_name": "Pending Confirmation",
                        "desc": "Awaiting confirmation",
                    },
                    <AllowedValueInfo>{
                        "value": "temp_hold",
                        "display_name": "Temporary Hold",
                        "desc": "Temporarily on hold",
                    },
                    <AllowedValueInfo>{
                        "value": "unsubscribed",
                        "display_name": "Unsubscribed",
                        "desc": "Contact has unsubscribed",
                    },
                ),
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the email address was added",
                "desc": "The timestamp when the email address was added (ISO-8601 format)",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the email address was last updated",
                "desc": "The timestamp when the email address was last updated (ISO-8601 format)",
            },
            "opt_in_source": {
                "type": StringOrNothingType,
                "display_name": "Opt-In Source",
                "short_desc": "The source of the opt-in",
                "desc": "The source that generated the opt-in for the email address",
            },
            "opt_in_date": {
                "type": StringOrNothingType,
                "display_name": "Opt-In Date",
                "short_desc": "When the contact opted in",
                "desc": "The date the contact opted in (ISO-8601 format)",
            },
        };
    }

    constructor() : HashDataType("ConstantContactEmailAddressDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "ConstantContactEmailAddressDataType") {
        addQoreFields(Fields);
    }
}

#! Constant Contact phone number sub-type
public class ConstantContactPhoneNumberDataType inherits HashDataType {
    public {
        const Fields = {
            "phone_number": {
                "type": StringOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "The phone number",
                "desc": "The phone number string",
                "example_value": "+1-555-555-1234",
            },
            "kind": {
                "type": StringOrNothingType,
                "display_name": "Kind",
                "short_desc": "The type of phone number",
                "desc": "The type of phone number",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "home", "display_name": "Home",
                        "desc": "Home phone number"},
                    <AllowedValueInfo>{"value": "work", "display_name": "Work",
                        "desc": "Work phone number"},
                    <AllowedValueInfo>{"value": "mobile", "display_name": "Mobile",
                        "desc": "Mobile phone number"},
                    <AllowedValueInfo>{"value": "other", "display_name": "Other",
                        "desc": "Other phone number"},
                ),
            },
        };
    }

    constructor() : HashDataType("ConstantContactPhoneNumberDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "ConstantContactPhoneNumberDataType") {
        addQoreFields(Fields);
    }
}

#! Constant Contact street address sub-type
public class ConstantContactStreetAddressDataType inherits HashDataType {
    public {
        const Fields = {
            "kind": {
                "type": StringOrNothingType,
                "display_name": "Kind",
                "short_desc": "The type of address",
                "desc": "The type of street address",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "home", "display_name": "Home",
                        "desc": "Home address"},
                    <AllowedValueInfo>{"value": "work", "display_name": "Work",
                        "desc": "Work address"},
                    <AllowedValueInfo>{"value": "other", "display_name": "Other",
                        "desc": "Other address"},
                ),
            },
            "street": {
                "type": StringOrNothingType,
                "display_name": "Street",
                "short_desc": "The street address",
                "desc": "The street address line",
                "example_value": "123 Main St",
            },
            "city": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "The city",
                "desc": "The city name",
                "example_value": "Springfield",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "The state or province code",
                "desc": "The state or province code",
                "example_value": "IL",
            },
            "postal_code": {
                "type": StringOrNothingType,
                "display_name": "Postal Code",
                "short_desc": "The postal/ZIP code",
                "desc": "The postal or ZIP code",
                "example_value": "62701",
            },
            "country": {
                "type": StringOrNothingType,
                "display_name": "Country",
                "short_desc": "The country code",
                "desc": "The two-letter ISO country code",
                "example_value": "US",
            },
        };
    }

    constructor() : HashDataType("ConstantContactStreetAddressDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "ConstantContactStreetAddressDataType") {
        addQoreFields(Fields);
    }
}

#! Constant Contact custom field value sub-type
public class ConstantContactCustomFieldValueDataType inherits HashDataType {
    public {
        const Fields = {
            "custom_field_id": {
                "type": StringType,
                "display_name": "Custom Field ID",
                "short_desc": "The custom field identifier",
                "desc": "The unique identifier of the custom field",
            },
            "value": {
                "type": StringOrNothingType,
                "display_name": "Value",
                "short_desc": "The custom field value",
                "desc": "The value for this custom field",
            },
        };
    }

    constructor() : HashDataType("ConstantContactCustomFieldValueDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "ConstantContactCustomFieldValueDataType") {
        addQoreFields(Fields);
    }
}

#! Constant Contact physical address sub-type (for account)
public class ConstantContactPhysicalAddressDataType inherits HashDataType {
    public {
        const Fields = {
            "address_line1": {
                "type": StringOrNothingType,
                "display_name": "Address Line 1",
                "short_desc": "First line of the address",
                "desc": "The first line of the physical address",
                "example_value": "123 Business Ave",
            },
            "address_line2": {
                "type": StringOrNothingType,
                "display_name": "Address Line 2",
                "short_desc": "Second line of the address",
                "desc": "The second line of the physical address (suite, floor, etc.)",
            },
            "city": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "The city",
                "desc": "The city name",
                "example_value": "Waltham",
            },
            "state_code": {
                "type": StringOrNothingType,
                "display_name": "State Code",
                "short_desc": "The state or province code",
                "desc": "The state or province code",
                "example_value": "MA",
            },
            "postal_code": {
                "type": StringOrNothingType,
                "display_name": "Postal Code",
                "short_desc": "The postal/ZIP code",
                "desc": "The postal or ZIP code",
                "example_value": "02451",
            },
            "country_code": {
                "type": StringOrNothingType,
                "display_name": "Country Code",
                "short_desc": "The two-letter country code",
                "desc": "The two-letter ISO country code",
                "example_value": "US",
            },
        };
    }

    constructor() : HashDataType("ConstantContactPhysicalAddressDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "ConstantContactPhysicalAddressDataType") {
        addQoreFields(Fields);
    }
}

#! Constant Contact email campaign activity sub-type (for create campaign request)
public class ConstantContactEmailCampaignActivityDataType inherits HashDataType {
    public {
        const Fields = {
            "format_type": {
                "type": IntOrNothingType,
                "display_name": "Format Type",
                "short_desc": "The email format type code",
                "desc": "The format type of the email",
                "example_value": 5,
                "allowed_values": (
                    <AllowedValueInfo>{"value": 1, "display_name": "Newsletter"},
                    <AllowedValueInfo>{"value": 2, "display_name": "Product Update"},
                    <AllowedValueInfo>{"value": 3, "display_name": "Announcement"},
                    <AllowedValueInfo>{"value": 4, "display_name": "Birthday/Anniversary"},
                    <AllowedValueInfo>{"value": 5, "display_name": "Custom Code Email"},
                ),
            },
            "from_email": {
                "type": StringOrNothingType,
                "display_name": "From Email",
                "short_desc": "The sender email address",
                "desc": "The email address used as the sender",
                "example_value": "newsletter@example.com",
            },
            "from_name": {
                "type": StringOrNothingType,
                "display_name": "From Name",
                "short_desc": "The sender display name",
                "desc": "The display name of the sender",
                "example_value": "My Company Newsletter",
            },
            "reply_to_email": {
                "type": StringOrNothingType,
                "display_name": "Reply-To Email",
                "short_desc": "The reply-to email address",
                "desc": "The email address for replies",
                "example_value": "replies@example.com",
            },
            "subject": {
                "type": StringOrNothingType,
                "display_name": "Subject",
                "short_desc": "The email subject line",
                "desc": "The subject line of the email",
                "example_value": "Monthly Newsletter - March 2026",
            },
            "html_content": {
                "type": StringOrNothingType,
                "display_name": "HTML Content",
                "short_desc": "The email HTML body content",
                "desc": "The HTML content of the email body (format types `1` and `5` only)",
            },
            "preheader": {
                "type": StringOrNothingType,
                "display_name": "Preheader",
                "short_desc": "Preview text shown in inbox",
                "desc": "The preview text displayed after the subject line in email clients "
                    "(format types `3`, `4`, and `5` only)",
                "example_value": "Check out our latest updates!",
            },
            "physical_address_in_footer": {
                "type": new ConstantContactFooterAddressDataType(AutoHashOrNothingType),
                "display_name": "Physical Address in Footer",
                "short_desc": "Address override for email footer",
                "desc": "Override the organization's physical address shown in the email footer",
            },
            "contact_list_ids": {
                "type": AutoListOrNothingType,
                "display_name": "Contact List IDs",
                "short_desc": "Target contact list IDs",
                "desc": "List of contact list IDs to send the campaign to",
            },
        };
    }

    constructor() : HashDataType("ConstantContactEmailCampaignActivityDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type,
            "ConstantContactEmailCampaignActivityDataType") {
        addQoreFields(Fields);
    }
}

#! Constant Contact campaign activity reference sub-type (for campaign response)
public class ConstantContactCampaignActivityRefDataType inherits HashDataType {
    public {
        const Fields = {
            "campaign_activity_id": {
                "type": StringOrNothingType,
                "display_name": "Campaign Activity ID",
                "short_desc": "The campaign activity identifier",
                "desc": "The unique identifier for the campaign activity",
            },
            "role": {
                "type": StringOrNothingType,
                "display_name": "Role",
                "short_desc": "The activity role",
                "desc": "The role of the campaign activity",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "primary_email",
                        "display_name": "Primary Email",
                        "desc": "The main marketing email message"},
                    <AllowedValueInfo>{"value": "resend",
                        "display_name": "Resend",
                        "desc": "A followup to non-openers"},
                    <AllowedValueInfo>{"value": "permalink",
                        "display_name": "Permalink",
                        "desc": "A permanent web-accessible link"},
                ),
            },
            "contact_list_ids": {
                "type": AutoListOrNothingType,
                "display_name": "Contact List IDs",
                "short_desc": "Associated contact list IDs",
                "desc": "List of contact list IDs associated with this campaign activity",
            },
        };
    }

    constructor() : HashDataType("ConstantContactCampaignActivityRefDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type,
            "ConstantContactCampaignActivityRefDataType") {
        addQoreFields(Fields);
    }
}

#! Constant Contact physical address in footer sub-type (for email campaigns)
public class ConstantContactFooterAddressDataType inherits HashDataType {
    public {
        const Fields = {
            "address_line1": {
                "type": StringOrNothingType,
                "display_name": "Address Line 1",
                "short_desc": "Primary address line",
                "desc": "The primary address line",
                "example_value": "123 Business Ave",
            },
            "address_line2": {
                "type": StringOrNothingType,
                "display_name": "Address Line 2",
                "short_desc": "Secondary address line",
                "desc": "The secondary address line",
            },
            "address_line3": {
                "type": StringOrNothingType,
                "display_name": "Address Line 3",
                "short_desc": "Third address line",
                "desc": "The third address line",
            },
            "address_optional": {
                "type": StringOrNothingType,
                "display_name": "Address Optional",
                "short_desc": "Additional address details",
                "desc": "Additional address details (suite, floor, etc.)",
            },
            "city": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "The city",
                "desc": "The city name",
                "example_value": "Waltham",
            },
            "state_code": {
                "type": StringOrNothingType,
                "display_name": "State Code",
                "short_desc": "State or province code",
                "desc": "The state or province code",
                "example_value": "MA",
            },
            "state_name": {
                "type": StringOrNothingType,
                "display_name": "State Name",
                "short_desc": "Full state or province name",
                "desc": "The full state or province name",
            },
            "postal_code": {
                "type": StringOrNothingType,
                "display_name": "Postal Code",
                "short_desc": "The postal/ZIP code",
                "desc": "The postal or ZIP code",
                "example_value": "02451",
            },
            "country_code": {
                "type": StringOrNothingType,
                "display_name": "Country Code",
                "short_desc": "Two-letter country code",
                "desc": "The two-letter ISO country code",
                "example_value": "US",
            },
            "country_name": {
                "type": StringOrNothingType,
                "display_name": "Country Name",
                "short_desc": "Full country name",
                "desc": "The full country name",
                "example_value": "United States",
            },
            "organization_name": {
                "type": StringOrNothingType,
                "display_name": "Organization Name",
                "short_desc": "Business or organization name",
                "desc": "The business or organization name for the footer",
            },
        };
    }

    constructor() : HashDataType("ConstantContactFooterAddressDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type,
            "ConstantContactFooterAddressDataType") {
        addQoreFields(Fields);
    }
}

#! Constant Contact note sub-type
public class ConstantContactNoteDataType inherits HashDataType {
    public {
        const Fields = {
            "note_id": {
                "type": StringOrNothingType,
                "display_name": "Note ID",
                "short_desc": "The note identifier",
                "desc": "The unique identifier for the note",
            },
            "content": {
                "type": StringOrNothingType,
                "display_name": "Content",
                "short_desc": "The note content",
                "desc": "The text content of the note",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the note was created",
                "desc": "The timestamp when the note was created (ISO-8601 format)",
            },
        };
    }

    constructor() : HashDataType("ConstantContactNoteDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "ConstantContactNoteDataType") {
        addQoreFields(Fields);
    }
}

#! Constant Contact bulk activity exclude sub-type
public class ConstantContactBulkActivityExcludeDataType inherits HashDataType {
    public {
        const Fields = {
            "contact_ids": {
                "type": AutoListOrNothingType,
                "display_name": "Exclude Contact IDs",
                "short_desc": "Contact IDs to exclude",
                "desc": "A list of contact IDs to exclude from the activity",
            },
        };
    }

    constructor() : HashDataType("ConstantContactBulkActivityExcludeDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type,
            "ConstantContactBulkActivityExcludeDataType") {
        addQoreFields(Fields);
    }
}

#! Constant Contact bulk activity source sub-type
/** The source fields are mutually exclusive — use only one to identify the set of contacts
    for the bulk activity.
*/
public class ConstantContactBulkActivitySourceDataType inherits HashDataType {
    public {
        const Fields = {
            "contact_ids": {
                "type": AutoListOrNothingType,
                "display_name": "Contact IDs",
                "short_desc": "List of contact IDs (max 500)",
                "desc": "A list of contact IDs to include in the activity (max 500). "
                    "Mutually exclusive with other source fields",
            },
            "list_ids": {
                "type": AutoListOrNothingType,
                "display_name": "Source List IDs",
                "short_desc": "List of source contact list IDs",
                "desc": "A list of contact list IDs whose members will be included in the "
                    "activity. Mutually exclusive with other source fields",
            },
            "tag_ids": {
                "type": AutoListOrNothingType,
                "display_name": "Tag IDs",
                "short_desc": "List of tag IDs (max 50)",
                "desc": "A list of tag IDs whose contacts will be included in the activity "
                    "(max 50). Mutually exclusive with other source fields",
            },
            "segment_id": {
                "type": IntOrNothingType,
                "display_name": "Segment ID",
                "short_desc": "Segment ID to use as source",
                "desc": "A segment ID whose contacts will be included in the activity. "
                    "Mutually exclusive with other source fields",
            },
            "all_active_contacts": {
                "type": BoolOrNothingType,
                "display_name": "All Active Contacts",
                "short_desc": "Include all active contacts",
                "desc": "Set to `True` to include all billable contacts. "
                    "Mutually exclusive with other source fields",
            },
            "new_subscriber": {
                "type": BoolOrNothingType,
                "display_name": "New Subscribers",
                "short_desc": "Include contacts from last 30 days",
                "desc": "Set to `True` to include only contacts created in the last 30 days. "
                    "Mutually exclusive with other source fields",
            },
            "engagement_level": {
                "type": StringOrNothingType,
                "display_name": "Engagement Level",
                "short_desc": "Filter by engagement level",
                "desc": "Filter contacts by engagement level. "
                    "Mutually exclusive with other source fields",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "unqualified",
                        "display_name": "Unqualified",
                        "desc": "Unqualified engagement",
                    },
                    <AllowedValueInfo>{
                        "value": "low",
                        "display_name": "Low",
                        "desc": "Low engagement",
                    },
                    <AllowedValueInfo>{
                        "value": "medium",
                        "display_name": "Medium",
                        "desc": "Medium engagement",
                    },
                    <AllowedValueInfo>{
                        "value": "high",
                        "display_name": "High",
                        "desc": "High engagement",
                    },
                ),
            },
            "exclude": {
                "type": new ConstantContactBulkActivityExcludeDataType(AutoHashOrNothingType),
                "display_name": "Exclude",
                "short_desc": "Contacts to exclude from the activity",
                "desc": "An optional exclude object to filter out specific contacts when "
                    "using `all_active_contacts` or `list_ids`",
            },
        };
    }

    constructor() : HashDataType("ConstantContactBulkActivitySourceDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type,
            "ConstantContactBulkActivitySourceDataType") {
        addQoreFields(Fields);
    }
}

#! Constant Contact SMS channel consent sub-type
public class ConstantContactSmsConsentDataType inherits HashDataType {
    public {
        const Fields = {
            "sms_consent_permission": {
                "type": StringOrNothingType,
                "display_name": "Consent Permission",
                "short_desc": "SMS consent permission status",
                "desc": "The SMS consent permission status",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "pending_confirmation",
                        "display_name": "Pending Confirmation",
                        "desc": "Awaiting consent confirmation"},
                    <AllowedValueInfo>{"value": "explicit",
                        "display_name": "Explicit",
                        "desc": "Explicitly granted consent"},
                ),
            },
            "consent_type": {
                "type": StringOrNothingType,
                "display_name": "Consent Type",
                "short_desc": "The type of consent",
                "desc": "The type of SMS consent (e.g., `promotional_sms`)",
            },
            "advertised_frequency": {
                "type": IntOrNothingType,
                "display_name": "Advertised Frequency",
                "short_desc": "Message frequency count",
                "desc": "The advertised message frequency count",
            },
            "advertised_interval": {
                "type": StringOrNothingType,
                "display_name": "Advertised Interval",
                "short_desc": "Frequency interval",
                "desc": "The advertised frequency interval (e.g., `daily`, `weekly`, `monthly`)",
            },
            "confirmed": {
                "type": BoolOrNothingType,
                "display_name": "Confirmed",
                "short_desc": "Whether consent is confirmed",
                "desc": "Whether the SMS consent has been confirmed",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When consent was created",
                "desc": "The timestamp when the consent was created (ISO-8601 format)",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When consent was last updated",
                "desc": "The timestamp when the consent was last updated (ISO-8601 format)",
            },
        };
    }

    constructor() : HashDataType("ConstantContactSmsConsentDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type,
            "ConstantContactSmsConsentDataType") {
        addQoreFields(Fields);
    }
}

#! Constant Contact SMS channel sub-type
public class ConstantContactSmsChannelDataType inherits HashDataType {
    public {
        const Fields = {
            "sms_channel_id": {
                "type": StringOrNothingType,
                "display_name": "SMS Channel ID",
                "short_desc": "Unique SMS channel identifier",
                "desc": "The unique identifier for the SMS channel (UUID format)",
            },
            "sms_address": {
                "type": StringOrNothingType,
                "display_name": "SMS Address",
                "short_desc": "Phone number without dial code",
                "desc": "The phone number without the country dial code",
            },
            "dial_code": {
                "type": StringOrNothingType,
                "display_name": "Dial Code",
                "short_desc": "Country dialing code",
                "desc": "The country dialing code (e.g., `1` for US)",
            },
            "country_code": {
                "type": StringOrNothingType,
                "display_name": "Country Code",
                "short_desc": "ISO country code",
                "desc": "The two-letter ISO country code (e.g., `US`)",
            },
            "create_source": {
                "type": StringOrNothingType,
                "display_name": "Create Source",
                "short_desc": "Source of the SMS channel creation",
                "desc": "The source that created the SMS channel record",
            },
            "update_source": {
                "type": StringOrNothingType,
                "display_name": "Update Source",
                "short_desc": "Source of the last update",
                "desc": "The source that last updated the SMS channel record",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the SMS channel was created",
                "desc": "The timestamp when the SMS channel was created (ISO-8601 format)",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the SMS channel was last updated",
                "desc": "The timestamp when the SMS channel was last updated (ISO-8601 format)",
            },
            "sms_channel_consents": {
                "type": new ListDataType("SmsConsentList",
                    new ConstantContactSmsConsentDataType(), True),
                "display_name": "SMS Channel Consents",
                "short_desc": "SMS consent records",
                "desc": "A list of SMS consent records for this channel",
            },
        };
    }

    constructor() : HashDataType("ConstantContactSmsChannelDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type,
            "ConstantContactSmsChannelDataType") {
        addQoreFields(Fields);
    }
}

#! Constant Contact company logo sub-type
public class ConstantContactCompanyLogoDataType inherits HashDataType {
    public {
        const Fields = {
            "url": {
                "type": StringOrNothingType,
                "display_name": "URL",
                "short_desc": "Constant Contact hosted logo URL",
                "desc": "The Constant Contact hosted URL for the company logo",
            },
            "external_url": {
                "type": StringOrNothingType,
                "display_name": "External URL",
                "short_desc": "External logo URL",
                "desc": "An external URL for the company logo",
            },
            "internal_id": {
                "type": StringOrNothingType,
                "display_name": "Internal ID",
                "short_desc": "Internal image identifier",
                "desc": "The internal image identifier (e.g., `ACCOUNT.IMAGE.5`)",
            },
        };
    }

    constructor() : HashDataType("ConstantContactCompanyLogoDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type,
            "ConstantContactCompanyLogoDataType") {
        addQoreFields(Fields);
    }
}

} # namespace ConstantContactDataProvider
