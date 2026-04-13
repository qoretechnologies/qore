# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CircleDataProviderDefs.qc defines constants and definitions for the CircleDataProvider module

/*  CircleDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CircleDataProvider module
public namespace CircleDataProvider {
    #! Application name constant
    public const AppName = "Circle";

    #! Default page size for list operations
    public const DefaultPageSize = 30;

    #! Default polling interval in seconds (conservative due to Circle rate limits)
    public const DefaultPollInterval = 120;

    #! Minimum polling interval in seconds
    public const MinPollInterval = 60;

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

    #! Event name for new comment
    public const EVENT_NEW_COMMENT = "new-comment";

    #! Event name for new community
    public const EVENT_NEW_COMMUNITY = "new-community";

    #! Event name for member accepted invitation
    public const EVENT_MEMBER_ACCEPTED = "member-accepted-invitation";

    #! Event name for new post
    public const EVENT_NEW_POST = "new-post";

    #! Event name for new space
    public const EVENT_NEW_SPACE = "new-space";

    #! Event name for member tagged
    public const EVENT_MEMBER_TAGGED = "member-tagged";

    #! Circle logo in SVG format (loaded from file)
    public const CircleLogo = File::readTextFile(get_script_dir() + "/circle-logo.svg");
}
