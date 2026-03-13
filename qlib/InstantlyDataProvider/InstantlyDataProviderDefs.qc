# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyDataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Instantly app name constant
public const AppName = "Instantly";

#! Default page size for pagination
public const DefaultPageSize = 100;

#! Account status: active
public const ACCOUNT_STATUS_ACTIVE = 1;

#! Account status: paused
public const ACCOUNT_STATUS_PAUSED = 2;

#! Account status: maintenance
public const ACCOUNT_STATUS_MAINTENANCE = 3;

#! Account status: connection error
public const ACCOUNT_STATUS_CONNECTION_ERROR = -1;

#! Account status: soft bounce
public const ACCOUNT_STATUS_SOFT_BOUNCE = -2;

#! Account status: sending error
public const ACCOUNT_STATUS_SENDING_ERROR = -3;

#! Campaign status: draft
public const CAMPAIGN_STATUS_DRAFT = 0;

#! Campaign status: active
public const CAMPAIGN_STATUS_ACTIVE = 1;

#! Campaign status: paused
public const CAMPAIGN_STATUS_PAUSED = 2;

#! Campaign status: completed
public const CAMPAIGN_STATUS_COMPLETED = 3;

#! Campaign status: running subsequences
public const CAMPAIGN_STATUS_RUNNING_SUBSEQUENCES = 4;

#! Lead status: active
public const LEAD_STATUS_ACTIVE = 1;

#! Lead status: paused
public const LEAD_STATUS_PAUSED = 2;

#! Lead status: completed
public const LEAD_STATUS_COMPLETED = 3;

#! Lead status: bounced
public const LEAD_STATUS_BOUNCED = -1;

#! Lead status: unsubscribed
public const LEAD_STATUS_UNSUBSCRIBED = -2;

#! Lead status: skipped
public const LEAD_STATUS_SKIPPED = -3;

#! Warmup status: paused
public const WARMUP_STATUS_PAUSED = 0;

#! Warmup status: active
public const WARMUP_STATUS_ACTIVE = 1;

#! Warmup status: banned
public const WARMUP_STATUS_BANNED = -1;

#! Warmup status: spam unknown
public const WARMUP_STATUS_SPAM_UNKNOWN = -2;

#! Warmup status: suspended
public const WARMUP_STATUS_SUSPENDED = -3;

#! Email type: campaign
public const EMAIL_TYPE_CAMPAIGN = 1;

#! Email type: received
public const EMAIL_TYPE_RECEIVED = 2;

#! Email type: sent
public const EMAIL_TYPE_SENT = 3;

#! Email type: scheduled
public const EMAIL_TYPE_SCHEDULED = 4;

#! Email provider code: Custom IMAP/SMTP
public const PROVIDER_CUSTOM = 1;

#! Email provider code: Google
public const PROVIDER_GOOGLE = 2;

#! Email provider code: Microsoft
public const PROVIDER_MICROSOFT = 3;

#! Email provider code: AWS
public const PROVIDER_AWS = 4;

#! Email provider code: AirMail
public const PROVIDER_AIRMAIL = 8;

#! Event type for reply received
public const EVENT_REPLY_RECEIVED = "reply_received";

#! Event type for email bounced
public const EVENT_EMAIL_BOUNCED = "email_bounced";

#! Event type for lead interested
public const EVENT_LEAD_INTERESTED = "lead_interested";

#! Event type for campaign completed
public const EVENT_CAMPAIGN_COMPLETED = "campaign_completed";

#! Instantly webhook event type for reply_received
public const WEBHOOK_REPLY_RECEIVED = "reply_received";

#! Instantly webhook event type for email_bounced
public const WEBHOOK_EMAIL_BOUNCED = "email_bounced";

#! Instantly webhook event type for lead_interested
public const WEBHOOK_LEAD_INTERESTED = "lead_interested";

#! Instantly webhook event type for campaign_completed
public const WEBHOOK_CAMPAIGN_COMPLETED = "campaign_completed";

#! Map of event types to webhook event types
public const EventToWebhookMap = {
    EVENT_REPLY_RECEIVED: WEBHOOK_REPLY_RECEIVED,
    EVENT_EMAIL_BOUNCED: WEBHOOK_EMAIL_BOUNCED,
    EVENT_LEAD_INTERESTED: WEBHOOK_LEAD_INTERESTED,
    EVENT_CAMPAIGN_COMPLETED: WEBHOOK_CAMPAIGN_COMPLETED,
};

#! All supported event types
public const AllEventTypes = (
    EVENT_REPLY_RECEIVED,
    EVENT_EMAIL_BOUNCED,
    EVENT_LEAD_INTERESTED,
    EVENT_CAMPAIGN_COMPLETED,
);

#! Instantly logo in SVG format (loaded from file)
public const InstantlyLogo = File::readTextFile(get_script_dir() + "/instantly-logo.svg");

} # namespace InstantlyDataProvider
