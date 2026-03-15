# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitDataProviderDefs.qc defines constants and definitions for the KitDataProvider module

/*  KitDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KitDataProvider module
public namespace KitDataProvider {
    #! Application name constant
    public const AppName = "Kit";

    #! Default page size for list operations
    public const DefaultPageSize = 500;

    #! Maximum page size for Kit API
    public const MaxPageSize = 1000;

    #! Default polling interval in seconds
    public const DefaultPollInterval = 60;

    #! Minimum polling interval in seconds
    public const MinPollInterval = 30;

    #! Maximum number of retries for rate-limited or transient error requests
    public const MaxRetries = 3;

    #! Set of retriable socket errors
    public const RetrySet = {
        "SOCKET-CONNECT-ERROR": True,
        "SOCKET-SSL-ERROR": True,
        "SOCKET-CLOSED": True,
        "SOCKET-TIMEOUT": True,
    };

    # --- Event name constants (used in triggers) ---

    #! Event name for subscriber activated
    public const EVENT_SUBSCRIBER_ACTIVATE = "subscriber-activate";

    #! Event name for subscriber unsubscribed
    public const EVENT_SUBSCRIBER_UNSUBSCRIBE = "subscriber-unsubscribe";

    #! Event name for subscriber bounced
    public const EVENT_SUBSCRIBER_BOUNCE = "subscriber-bounce";

    #! Event name for subscriber complained
    public const EVENT_SUBSCRIBER_COMPLAIN = "subscriber-complain";

    #! Event name for form subscription
    public const EVENT_FORM_SUBSCRIBE = "form-subscribe";

    #! Event name for course subscription
    public const EVENT_COURSE_SUBSCRIBE = "course-subscribe";

    #! Kit webhook event name mapping (internal event name -> Kit API event name)
    public const WebhookEventMap = {
        EVENT_SUBSCRIBER_ACTIVATE: "subscriber.subscriber_activate",
        EVENT_SUBSCRIBER_UNSUBSCRIBE: "subscriber.subscriber_unsubscribe",
        EVENT_SUBSCRIBER_BOUNCE: "subscriber.subscriber_bounce",
        EVENT_SUBSCRIBER_COMPLAIN: "subscriber.subscriber_complain",
        EVENT_FORM_SUBSCRIBE: "subscriber.form_subscribe",
        EVENT_COURSE_SUBSCRIBE: "subscriber.course_subscribe",
    };

    #! Kit logo in SVG format (loaded from file)
    public const KitLogo = File::readTextFile(get_script_dir() + "/kit-logo.svg");
}
