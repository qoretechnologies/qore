# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlWebhookDataType.qc webhook configuration type for Firecrawl requests

/*  FirecrawlWebhookDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Webhook configuration type for Firecrawl async operations
public class FirecrawlWebhookDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "url": {
                "display_name": "URL",
                "short_desc": "Webhook endpoint URL",
                "desc": "The URL to send webhook notifications to. Receives events for "
                    "operation lifecycle (started, page scraped, completed, failed)",
                "type": UriType,
                "required": True,
                "example_value": "https://example.com/webhook",
            },
            "headers": {
                "display_name": "Headers",
                "short_desc": "Custom webhook headers",
                "desc": "Custom HTTP headers sent with each webhook request",
                "type": AutoHashOrNothingType,
            },
            "metadata": {
                "display_name": "Metadata",
                "short_desc": "Custom metadata",
                "desc": "Custom metadata included in all webhook payloads",
                "type": AutoHashOrNothingType,
            },
            "events": {
                "display_name": "Events",
                "short_desc": "Event types to receive",
                "desc": "List of event types to receive notifications for. "
                    "Defaults to all events if not specified",
                "type": SoftListOrNothingType,
                "element_allowed_values": (
                    <AllowedValueInfo>{"value": "completed", "display_name": "Completed"},
                    <AllowedValueInfo>{"value": "page", "display_name": "Page"},
                    <AllowedValueInfo>{"value": "failed", "display_name": "Failed"},
                    <AllowedValueInfo>{"value": "started", "display_name": "Started"},
                ),
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

} # namespace FirecrawlDataProvider
