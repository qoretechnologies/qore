# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlScrapeDataItemDataType.qc data type for a single scraped page result

/*  FirecrawlScrapeDataItemDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Data type for a single scraped page result
/** Fields present in the response depend on the \c formats requested in the scrape action.
*/
public class FirecrawlScrapeDataItemDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "markdown": {
                "display_name": "Markdown",
                "short_desc": "Page content in markdown format",
                "desc": "The page content converted to markdown format",
                "type": StringOrNothingType,
            },
            "html": {
                "display_name": "HTML",
                "short_desc": "Cleaned HTML content",
                "desc": "Cleaned HTML content with script/style tags removed and relative "
                    "URLs converted to absolute",
                "type": StringOrNothingType,
            },
            "rawHtml": {
                "display_name": "Raw HTML",
                "short_desc": "Unmodified HTML from the page",
                "desc": "The exact, unmodified HTML as received from the page",
                "type": StringOrNothingType,
            },
            "screenshot": {
                "display_name": "Screenshot",
                "short_desc": "Screenshot URL",
                "desc": "URL of the page screenshot. Available when `screenshot` is in `formats`",
                "type": StringOrNothingType,
            },
            "audio": {
                "display_name": "Audio",
                "short_desc": "Audio file URL",
                "desc": "Signed URL to the extracted MP3 audio file. Available when `audio` "
                    "is in `formats`",
                "type": StringOrNothingType,
            },
            "links": {
                "display_name": "Links",
                "short_desc": "Links found on the page",
                "desc": "List of URLs found on the page. Available when `links` is in `formats`",
                "type": SoftListOrNothingType,
            },
            "images": {
                "display_name": "Images",
                "short_desc": "Images found on the page",
                "desc": "List of image URLs found on the page. Available when `images` is "
                    "in `formats`",
                "type": SoftListOrNothingType,
            },
            "metadata": {
                "display_name": "Metadata",
                "short_desc": "Page metadata",
                "desc": "Extracted page metadata including `title`, `description`, "
                    "`language`, `keywords`, `sourceURL`, `statusCode`, and Open Graph "
                    "fields (`ogTitle`, `ogDescription`, `ogUrl`, `ogImage`)",
                "type": new FirecrawlMetadataDataType(AutoHashOrNothingType),
            },
            "actions": {
                "display_name": "Actions",
                "short_desc": "Browser action results",
                "desc": "Results from browser actions, including screenshots taken, scrape "
                    "results, JavaScript return values, and generated PDFs",
                "type": new FirecrawlActionsResultDataType(AutoHashOrNothingType),
            },
            "summary": {
                "display_name": "Summary",
                "short_desc": "AI-generated page summary",
                "desc": "AI-generated summary of the page content. Available when `summary` "
                    "is in `formats`",
                "type": StringOrNothingType,
            },
            "changeTracking": {
                "display_name": "Change Tracking",
                "short_desc": "Change tracking information",
                "desc": "Change tracking information for the page. Available when "
                    "`changeTracking` is in `formats`",
                "type": new FirecrawlChangeTrackingDataType(AutoHashOrNothingType),
            },
            "branding": {
                "display_name": "Branding",
                "short_desc": "Extracted branding information",
                "desc": "Branding information extracted from the page. Available when "
                    "`branding` is in `formats`",
                "type": new FirecrawlBrandingDataType(AutoHashOrNothingType),
            },
            "warning": {
                "display_name": "Warning",
                "short_desc": "Warning message",
                "desc": "A warning message, if any, from the scrape operation",
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
