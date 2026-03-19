# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoDataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CustomerIoDataProvider module
public namespace CustomerIoDataProvider {

#! Customer.io app name constant
public const AppName = "CustomerIo";

#! Default page size for pagination
public const DefaultPageSize = 100;

#! Event type: email sent
public const EVENT_EMAIL_SENT = "email-sent";

#! Event type: email delivered
public const EVENT_EMAIL_DELIVERED = "email-delivered";

#! Event type: email opened
public const EVENT_EMAIL_OPENED = "email-opened";

#! Event type: email clicked
public const EVENT_EMAIL_CLICKED = "email-clicked";

#! Event type: email bounced
public const EVENT_EMAIL_BOUNCED = "email-bounced";

#! Event type: email dropped
public const EVENT_EMAIL_DROPPED = "email-dropped";

#! Event type: email spammed
public const EVENT_EMAIL_SPAMMED = "email-spammed";

#! Event type: email unsubscribed
public const EVENT_EMAIL_UNSUBSCRIBED = "email-unsubscribed";

#! Event type: SMS sent
public const EVENT_SMS_SENT = "sms-sent";

#! Event type: SMS delivered
public const EVENT_SMS_DELIVERED = "sms-delivered";

#! Event type: SMS failed
public const EVENT_SMS_FAILED = "sms-failed";

#! Event type: SMS clicked
public const EVENT_SMS_CLICKED = "sms-clicked";

#! Event type: push sent
public const EVENT_PUSH_SENT = "push-sent";

#! Event type: push delivered
public const EVENT_PUSH_DELIVERED = "push-delivered";

#! Event type: push opened
public const EVENT_PUSH_OPENED = "push-opened";

#! Event type: push failed
public const EVENT_PUSH_FAILED = "push-failed";

#! Event type: Slack sent
public const EVENT_SLACK_SENT = "slack-sent";

#! Event type: Slack clicked
public const EVENT_SLACK_CLICKED = "slack-clicked";

#! Event type: Slack failed
public const EVENT_SLACK_FAILED = "slack-failed";

#! Event type: subscribed
public const EVENT_SUBSCRIBED = "subscribed";

#! Event type: unsubscribed
public const EVENT_UNSUBSCRIBED = "unsubscribed";

#! Map of event types to Customer.io webhook metric names
public const EventToMetricMap = {
    EVENT_EMAIL_SENT: "email_sent",
    EVENT_EMAIL_DELIVERED: "email_delivered",
    EVENT_EMAIL_OPENED: "email_opened",
    EVENT_EMAIL_CLICKED: "email_clicked",
    EVENT_EMAIL_BOUNCED: "email_bounced",
    EVENT_EMAIL_DROPPED: "email_dropped",
    EVENT_EMAIL_SPAMMED: "email_spammed",
    EVENT_EMAIL_UNSUBSCRIBED: "email_unsubscribed",
    EVENT_SMS_SENT: "sms_sent",
    EVENT_SMS_DELIVERED: "sms_delivered",
    EVENT_SMS_FAILED: "sms_failed",
    EVENT_SMS_CLICKED: "sms_clicked",
    EVENT_PUSH_SENT: "push_sent",
    EVENT_PUSH_DELIVERED: "push_delivered",
    EVENT_PUSH_OPENED: "push_opened",
    EVENT_PUSH_FAILED: "push_failed",
    EVENT_SLACK_SENT: "slack_sent",
    EVENT_SLACK_CLICKED: "slack_clicked",
    EVENT_SLACK_FAILED: "slack_failed",
    EVENT_SUBSCRIBED: "subscribed",
    EVENT_UNSUBSCRIBED: "unsubscribed",
};

#! All supported event types
public const AllEventTypes = (
    EVENT_EMAIL_SENT,
    EVENT_EMAIL_DELIVERED,
    EVENT_EMAIL_OPENED,
    EVENT_EMAIL_CLICKED,
    EVENT_EMAIL_BOUNCED,
    EVENT_EMAIL_DROPPED,
    EVENT_EMAIL_SPAMMED,
    EVENT_EMAIL_UNSUBSCRIBED,
    EVENT_SMS_SENT,
    EVENT_SMS_DELIVERED,
    EVENT_SMS_FAILED,
    EVENT_SMS_CLICKED,
    EVENT_PUSH_SENT,
    EVENT_PUSH_DELIVERED,
    EVENT_PUSH_OPENED,
    EVENT_PUSH_FAILED,
    EVENT_SLACK_SENT,
    EVENT_SLACK_CLICKED,
    EVENT_SLACK_FAILED,
    EVENT_SUBSCRIBED,
    EVENT_UNSUBSCRIBED,
);

#! Customer.io logo in SVG format (loaded from file)
public const CustomerIoLogo = File::readTextFile(get_script_dir() + "/customerio-logo.svg");

} # namespace CustomerIoDataProvider
