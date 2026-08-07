# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlMetadataDataType.qc page metadata type for Firecrawl scrape responses

/*  FirecrawlMetadataDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Page metadata type for Firecrawl scrape responses
/** Contains metadata extracted from the scraped page including title, description,
    Open Graph tags, and HTTP response information. Additional metadata fields from
    the page's HTML meta tags may also be present.
*/
public class FirecrawlMetadataDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "title": {
                "display_name": "Title",
                "short_desc": "Page title",
                "desc": "The page title from the `<title>` tag or meta tags",
                "type": StringOrNothingType,
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Meta description",
                "desc": "The page meta description",
                "type": StringOrNothingType,
            },
            "language": {
                "display_name": "Language",
                "short_desc": "Detected language code",
                "desc": "The detected language code for the page (e.g., `en`, `de`)",
                "type": StringOrNothingType,
            },
            "sourceURL": {
                "display_name": "Source URL",
                "short_desc": "The original requested URL",
                "desc": "The original URL that was requested for scraping",
                "type": StringOrNothingType,
            },
            "url": {
                "display_name": "URL",
                "short_desc": "Final URL after redirects",
                "desc": "The final URL after following any redirects",
                "type": UriOrNothingType,
            },
            "keywords": {
                "display_name": "Keywords",
                "short_desc": "Meta keywords",
                "desc": "Keywords from the page's meta tags",
                "type": StringOrNothingType,
            },
            "robots": {
                "display_name": "Robots",
                "short_desc": "Robots meta directive",
                "desc": "The robots meta tag directive (e.g., `index, follow`)",
                "type": StringOrNothingType,
            },
            "ogTitle": {
                "display_name": "OG Title",
                "short_desc": "Open Graph title",
                "desc": "The Open Graph title (`og:title`) meta tag value",
                "type": StringOrNothingType,
            },
            "ogDescription": {
                "display_name": "OG Description",
                "short_desc": "Open Graph description",
                "desc": "The Open Graph description (`og:description`) meta tag value",
                "type": StringOrNothingType,
            },
            "ogUrl": {
                "display_name": "OG URL",
                "short_desc": "Open Graph URL",
                "desc": "The Open Graph URL (`og:url`) meta tag value",
                "type": StringOrNothingType,
            },
            "ogImage": {
                "display_name": "OG Image",
                "short_desc": "Open Graph image URL",
                "desc": "The Open Graph image URL (`og:image`) meta tag value",
                "type": StringOrNothingType,
            },
            "ogSiteName": {
                "display_name": "OG Site Name",
                "short_desc": "Open Graph site name",
                "desc": "The Open Graph site name (`og:site_name`) meta tag value",
                "type": StringOrNothingType,
            },
            "ogLocaleAlternate": {
                "display_name": "OG Locale Alternate",
                "short_desc": "Alternate locale variants",
                "desc": "List of alternate locale variants from Open Graph tags",
                "type": SoftListOrNothingType,
            },
            "statusCode": {
                "display_name": "Status Code",
                "short_desc": "HTTP response status code",
                "desc": "The HTTP response status code from the scraped page",
                "type": SoftIntOrNothingType,
            },
            "contentType": {
                "display_name": "Content Type",
                "short_desc": "Response content type",
                "desc": "The MIME content type of the HTTP response",
                "type": StringOrNothingType,
            },
            "error": {
                "display_name": "Error",
                "short_desc": "Error message",
                "desc": "Error message if the scrape partially failed",
                "type": StringOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

} # namespace FirecrawlDataProvider
