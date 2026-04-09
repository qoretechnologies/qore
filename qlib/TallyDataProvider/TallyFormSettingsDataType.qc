# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyFormSettingsDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the TallyDataProvider module
public namespace TallyDataProvider {
#! Form settings data type
public class TallyFormSettingsDataType inherits HashDataType {
    public {
        const Fields = {
            "language": {
                "display_name": "Language",
                "short_desc": "Form display language",
                "desc": "The language code for the form (max 12 characters)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "isClosed": {
                "display_name": "Is Closed",
                "short_desc": "Whether the form is closed",
                "desc": "`True` if the form is closed and no longer accepting submissions",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "closeMessageTitle": {
                "display_name": "Close Message Title",
                "short_desc": "Title shown when form is closed",
                "desc": "The title displayed when a user tries to access a closed form",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "closeMessageDescription": {
                "display_name": "Close Message Description",
                "short_desc": "Description shown when form is closed",
                "desc": "The description displayed when a user tries to access a closed form",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "closeTimezone": {
                "display_name": "Close Timezone",
                "short_desc": "Timezone for scheduled close",
                "desc": "The timezone for the scheduled form close date/time",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "closeDate": {
                "display_name": "Close Date",
                "short_desc": "Scheduled close date",
                "desc": "The date when the form will automatically close",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "closeTime": {
                "display_name": "Close Time",
                "short_desc": "Scheduled close time",
                "desc": "The time when the form will automatically close",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "submissionsLimit": {
                "display_name": "Submissions Limit",
                "short_desc": "Maximum number of submissions",
                "desc": "The maximum number of submissions allowed before the form closes automatically",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "uniqueSubmissionKey": {
                "display_name": "Unique Submission Key",
                "short_desc": "Field for unique submission enforcement",
                "desc": "The field key used to enforce unique submissions",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "redirectOnCompletion": {
                "display_name": "Redirect on Completion",
                "short_desc": "URL to redirect to after submission",
                "desc": "The URL to redirect respondents to after form submission",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "hasSelfEmailNotifications": {
                "display_name": "Self Email Notifications",
                "short_desc": "Enable email notifications to form owner",
                "desc": "`True` to send email notifications to the form owner on new submissions",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "selfEmailTo": {
                "display_name": "Self Email To",
                "short_desc": "Owner notification email address",
                "desc": "The email address to send form owner notifications to",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "selfEmailReplyTo": {
                "display_name": "Self Email Reply-To",
                "short_desc": "Owner notification reply-to address",
                "desc": "The reply-to email address for owner notification emails",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "selfEmailSubject": {
                "display_name": "Self Email Subject",
                "short_desc": "Owner notification email subject",
                "desc": "The subject line for form owner notification emails",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "selfEmailFromName": {
                "display_name": "Self Email From Name",
                "short_desc": "Owner notification sender name",
                "desc": "The sender display name for form owner notification emails",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "selfEmailBody": {
                "display_name": "Self Email Body",
                "short_desc": "Owner notification email body",
                "desc": "The body content for form owner notification emails",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "hasRespondentEmailNotifications": {
                "display_name": "Respondent Email Notifications",
                "short_desc": "Enable email notifications to respondents",
                "desc": "`True` to send confirmation emails to respondents after submission",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "respondentEmailTo": {
                "display_name": "Respondent Email To",
                "short_desc": "Respondent notification email address",
                "desc": "The email address to send respondent confirmation emails to",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "respondentEmailReplyTo": {
                "display_name": "Respondent Email Reply-To",
                "short_desc": "Respondent notification reply-to address",
                "desc": "The reply-to email address for respondent confirmation emails",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "respondentEmailSubject": {
                "display_name": "Respondent Email Subject",
                "short_desc": "Respondent notification email subject",
                "desc": "The subject line for respondent confirmation emails",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "respondentEmailFromName": {
                "display_name": "Respondent Email From Name",
                "short_desc": "Respondent notification sender name",
                "desc": "The sender display name for respondent confirmation emails",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "respondentEmailBody": {
                "display_name": "Respondent Email Body",
                "short_desc": "Respondent notification email body",
                "desc": "The body content for respondent confirmation emails",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "hasProgressBar": {
                "display_name": "Has Progress Bar",
                "short_desc": "Show progress bar",
                "desc": "`True` to display a progress bar on multi-page forms",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "hasPartialSubmissions": {
                "display_name": "Has Partial Submissions",
                "short_desc": "Allow partial submissions",
                "desc": "`True` to save partial submissions when respondents don't complete the form",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "pageAutoJump": {
                "display_name": "Page Auto Jump",
                "short_desc": "Auto-advance pages",
                "desc": "`True` to automatically advance to the next page when all fields are filled",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "saveForLater": {
                "display_name": "Save for Later",
                "short_desc": "Allow saving form for later",
                "desc": "`True` to allow respondents to save their progress and return later",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "styles": {
                "display_name": "Styles",
                "short_desc": "Custom CSS styles",
                "desc": "Custom CSS styles applied to the form",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "password": {
                "display_name": "Password",
                "short_desc": "Form access password",
                "desc": "Optional password required to access the form",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "submissionsDataRetentionDuration": {
                "display_name": "Data Retention Duration",
                "short_desc": "Submission data retention duration",
                "desc": "The duration value for how long submission data is retained",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "submissionsDataRetentionUnit": {
                "display_name": "Data Retention Unit",
                "short_desc": "Submission data retention time unit",
                "desc": "The time unit for the data retention duration",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "minutes", "display_name": "Minutes"},
                    <AllowedValueInfo>{"value": "hours", "display_name": "Hours"},
                    <AllowedValueInfo>{"value": "days", "display_name": "Days"},
                    <AllowedValueInfo>{"value": "weeks", "display_name": "Weeks"},
                    <AllowedValueInfo>{"value": "months", "display_name": "Months"},
                    <AllowedValueInfo>{"value": "years", "display_name": "Years"},
                ),
            },
        };
    }

    #! Creates the object
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}
}
