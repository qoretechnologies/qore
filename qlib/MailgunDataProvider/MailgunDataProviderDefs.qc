# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MailgunDataProvider module definition

/*  MailgunDataProviderDefs.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the MailgunDataProvider module
public namespace MailgunDataProvider {

#! Mailgun app name constant
public const AppName = "Mailgun";

#! Default page size for pagination
public const DefaultPageSize = 100;

#! Maximum page size for Mailgun API
public const MaxPageSize = 1000;

#! Event type for email clicked
public const EVENT_CLICKED = "clicked";

#! Event type for email delivered
public const EVENT_DELIVERED = "delivered";

#! Event type for email opened
public const EVENT_OPENED = "opened";

#! Event type for permanent failure (hard bounce)
public const EVENT_PERMANENT_FAIL = "permanent_fail";

#! Event type for spam complaint
public const EVENT_COMPLAINED = "complained";

#! Event type for temporary failure (soft bounce)
public const EVENT_TEMPORARY_FAIL = "temporary_fail";

#! Event type for unsubscribe
public const EVENT_UNSUBSCRIBED = "unsubscribed";

#! All supported event types
public const AllEventTypes = (
    EVENT_CLICKED,
    EVENT_DELIVERED,
    EVENT_OPENED,
    EVENT_PERMANENT_FAIL,
    EVENT_COMPLAINED,
    EVENT_TEMPORARY_FAIL,
    EVENT_UNSUBSCRIBED,
);

#! Mailgun logo in SVG format (loaded from file)
public const MailgunLogo = File::readTextFile(get_script_dir() + "/mailgun-logo.svg");

} # namespace MailgunDataProvider
