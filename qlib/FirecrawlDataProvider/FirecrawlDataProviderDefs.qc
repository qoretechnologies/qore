# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlDataProviderDefs.qc defines constants and definitions for the FirecrawlDataProvider module

/*  FirecrawlDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FirecrawlDataProvider module
public namespace FirecrawlDataProvider {
    #! Application name constant
    public const AppName = "Firecrawl";

    #! Maximum number of retries for transient errors
    public const MaxRetries = 5;

    #! Maximum number of job polling attempts before giving up
    public const MaxJobPollAttempts = 120;

    #! Initial polling interval in seconds
    public const InitialJobPollInterval = 2;

    #! Maximum polling interval in seconds
    public const MaxJobPollInterval = 30;

    #! Set of retriable socket errors
    public const RetrySet = {
        "SOCKET-CONNECT-ERROR": True,
        "SOCKET-SSL-ERROR": True,
        "SOCKET-CLOSED": True,
        "SOCKET-TIMEOUT": True,
    };

    #! Firecrawl logo in SVG format (loaded from file)
    public const FirecrawlLogo = File::readTextFile(get_script_dir() + "/firecrawl-logo.svg");
}
