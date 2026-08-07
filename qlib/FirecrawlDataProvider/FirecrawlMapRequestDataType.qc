# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlMapRequestDataType.qc request type for the Firecrawl map action

/*  FirecrawlMapRequestDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for the Firecrawl map action
public class FirecrawlMapRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "url": {
                "display_name": "URL",
                "short_desc": "The URL to map",
                "desc": "The starting URL of the website to map",
                "type": UriType,
                "required": True,
                "example_value": "https://example.com",
            },
            "search": {
                "display_name": "Search",
                "short_desc": "Search query to filter URLs",
                "desc": "An optional search query to filter the discovered URLs",
                "type": StringOrNothingType,
                "attr": {"preselected": True},
            },
            "limit": {
                "display_name": "Limit",
                "short_desc": "Maximum number of links to return",
                "desc": "Maximum number of links to return. Defaults to `5000`",
                "type": SoftIntOrNothingType,
            },
            "includeSubdomains": {
                "display_name": "Include Subdomains",
                "short_desc": "Include subdomain URLs",
                "desc": "If `True`, subdomain URLs are included in the results",
                "type": SoftBoolOrNothingType,
            },
            "sitemap": {
                "display_name": "Sitemap",
                "short_desc": "Sitemap handling mode",
                "desc": "How to handle the sitemap when mapping URLs",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "skip", "display_name": "Skip"},
                    <AllowedValueInfo>{"value": "include", "display_name": "Include"},
                    <AllowedValueInfo>{"value": "only", "display_name": "Only"},
                ),
            },
            "ignoreQueryParameters": {
                "display_name": "Ignore Query Parameters",
                "short_desc": "Ignore URL query parameters",
                "desc": "If `True`, URLs with different query parameters are treated as the same page",
                "type": SoftBoolOrNothingType,
            },
            "ignoreCache": {
                "display_name": "Ignore Cache",
                "short_desc": "Ignore cached results",
                "desc": "If `True`, cached results are ignored and a fresh map is generated",
                "type": SoftBoolOrNothingType,
            },
            "timeout": {
                "display_name": "Timeout",
                "short_desc": "Timeout in milliseconds",
                "desc": "Maximum time in milliseconds to wait for the map operation to complete",
                "type": SoftIntOrNothingType,
            },
            "location": {
                "display_name": "Location",
                "short_desc": "Geolocation settings",
                "desc": "Geolocation settings for the map request",
                "type": new FirecrawlLocationDataType(AutoHashOrNothingType),
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FirecrawlDataProvider
