# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistDataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Lemlist app name constant
public const AppName = "Lemlist";

#! Default page size for pagination
public const DefaultPageSize = 100;

#! Maximum page size for Lemlist API
public const MaxPageSize = 100;

#! Campaign status: running
public const CAMPAIGN_STATUS_RUNNING = "running";

#! Campaign status: draft
public const CAMPAIGN_STATUS_DRAFT = "draft";

#! Campaign status: archived
public const CAMPAIGN_STATUS_ARCHIVED = "archived";

#! Campaign status: ended
public const CAMPAIGN_STATUS_ENDED = "ended";

#! Campaign status: paused
public const CAMPAIGN_STATUS_PAUSED = "paused";

#! Campaign status: errors
public const CAMPAIGN_STATUS_ERRORS = "errors";

#! All supported campaign statuses
public const AllCampaignStatuses = (
    CAMPAIGN_STATUS_RUNNING,
    CAMPAIGN_STATUS_DRAFT,
    CAMPAIGN_STATUS_ARCHIVED,
    CAMPAIGN_STATUS_ENDED,
    CAMPAIGN_STATUS_PAUSED,
    CAMPAIGN_STATUS_ERRORS,
);

#! Event type for email sent
public const EVENT_EMAIL_SENT = "email-sent";

#! Event type for email opened
public const EVENT_EMAIL_OPENED = "email-opened";

#! Event type for email clicked
public const EVENT_EMAIL_CLICKED = "email-clicked";

#! Event type for email replied
public const EVENT_EMAIL_REPLIED = "email-replied";

#! Event type for email bounced
public const EVENT_EMAIL_BOUNCED = "email-bounced";

#! Event type for lead interested
public const EVENT_LEAD_INTERESTED = "lead-interested";

#! Event type for lead not interested
public const EVENT_LEAD_NOT_INTERESTED = "lead-not-interested";

#! Lemlist webhook type for emails sent
public const WEBHOOK_EMAILS_SENT = "emailsSent";

#! Lemlist webhook type for emails opened
public const WEBHOOK_EMAILS_OPENED = "emailsOpened";

#! Lemlist webhook type for emails clicked
public const WEBHOOK_EMAILS_CLICKED = "emailsClicked";

#! Lemlist webhook type for emails replied
public const WEBHOOK_EMAILS_REPLIED = "emailsReplied";

#! Lemlist webhook type for emails bounced
public const WEBHOOK_EMAILS_BOUNCED = "emailsBounced";

#! Lemlist webhook type for lead interested
public const WEBHOOK_INTERESTED = "interested";

#! Lemlist webhook type for lead not interested
public const WEBHOOK_NOT_INTERESTED = "notInterested";

#! Map of event types to webhook event types
public const EventToWebhookMap = {
    EVENT_EMAIL_SENT: WEBHOOK_EMAILS_SENT,
    EVENT_EMAIL_OPENED: WEBHOOK_EMAILS_OPENED,
    EVENT_EMAIL_CLICKED: WEBHOOK_EMAILS_CLICKED,
    EVENT_EMAIL_REPLIED: WEBHOOK_EMAILS_REPLIED,
    EVENT_EMAIL_BOUNCED: WEBHOOK_EMAILS_BOUNCED,
    EVENT_LEAD_INTERESTED: WEBHOOK_INTERESTED,
    EVENT_LEAD_NOT_INTERESTED: WEBHOOK_NOT_INTERESTED,
};

#! All supported event types
public const AllEventTypes = (
    EVENT_EMAIL_SENT,
    EVENT_EMAIL_OPENED,
    EVENT_EMAIL_CLICKED,
    EVENT_EMAIL_REPLIED,
    EVENT_EMAIL_BOUNCED,
    EVENT_LEAD_INTERESTED,
    EVENT_LEAD_NOT_INTERESTED,
);

#! Lemlist logo in SVG format (loaded from file)
public const LemlistLogo = File::readTextFile(get_script_dir() + "/lemlist-logo.svg");

} # namespace LemlistDataProvider
