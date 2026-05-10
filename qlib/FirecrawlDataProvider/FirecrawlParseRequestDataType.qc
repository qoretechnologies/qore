# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlParseRequestDataType.qc request type for the Firecrawl parse action

/*  FirecrawlParseRequestDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for the Firecrawl parse action
/** Uploads a local document and converts it to clean, LLM-ready data.
    Supports `.html`, `.htm`, `.pdf`, `.docx`, `.doc`, `.odt`, `.rtf`, `.xlsx`, and `.xls`
    files up to 50 MB. The wire format is `multipart/form-data` with a `file` part
    (binary file content) and an `options` part (JSON-encoded options); the data
    provider performs the multipart packaging automatically.
*/
public class FirecrawlParseRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "file": {
                "display_name": "File",
                "short_desc": "The document to parse",
                "desc": "The document to upload and parse. Supported file types: `.html`, "
                    "`.htm`, `.pdf`, `.docx`, `.doc`, `.odt`, `.rtf`, `.xlsx`, `.xls`. "
                    "Maximum size: 50 MB",
                "type": FileDataType,
                "required": True,
            },
            "formats": {
                "display_name": "Formats",
                "short_desc": "Output formats to return",
                "desc": "List of output formats to return from the parse. Defaults to "
                    "`markdown`",
                "type": SoftListOrNothingType,
                "attr": {"preselected": True},
                "example_value": ("markdown",),
                "element_allowed_values": (
                    <AllowedValueInfo>{"value": "markdown", "display_name": "Markdown"},
                    <AllowedValueInfo>{"value": "summary", "display_name": "Summary"},
                    <AllowedValueInfo>{"value": "html", "display_name": "HTML"},
                    <AllowedValueInfo>{"value": "rawHtml", "display_name": "Raw HTML"},
                    <AllowedValueInfo>{"value": "links", "display_name": "Links"},
                    <AllowedValueInfo>{"value": "images", "display_name": "Images"},
                    <AllowedValueInfo>{"value": "json", "display_name": "JSON"},
                ),
            },
            "onlyMainContent": {
                "display_name": "Only Main Content",
                "short_desc": "Extract only the main content",
                "desc": "If `True`, only the main content of the document is returned, "
                    "excluding headers, navigation, footers, etc. Defaults to `True`",
                "type": SoftBoolOrNothingType,
            },
            "includeTags": {
                "display_name": "Include Tags",
                "short_desc": "HTML tags to include",
                "desc": "List of HTML tags to include in the output (e.g., `article`, `main`). "
                    "Only content within these tags is returned",
                "type": SoftListOrNothingType,
            },
            "excludeTags": {
                "display_name": "Exclude Tags",
                "short_desc": "HTML tags to exclude",
                "desc": "List of HTML tags to exclude from the output (e.g., `nav`, `footer`)",
                "type": SoftListOrNothingType,
            },
            "headers": {
                "display_name": "Headers",
                "short_desc": "Custom HTTP headers",
                "desc": "Custom HTTP headers to use for any embedded resource fetches "
                    "performed during parsing",
                "type": AutoHashOrNothingType,
            },
            "timeout": {
                "display_name": "Timeout",
                "short_desc": "Timeout in milliseconds",
                "desc": "Maximum time in milliseconds to wait for the parse to complete. "
                    "Defaults to `30000` (30 seconds); maximum `300000` (5 minutes)",
                "type": SoftIntOrNothingType,
            },
            "parsers": {
                "display_name": "Parsers",
                "short_desc": "PDF parser configuration",
                "desc": "List of parser configurations for PDF processing. Each entry "
                    "specifies a parser type and processing options",
                "type": new ListDataType("parsers",
                    new FirecrawlParsersDataType(AutoHashOrNothingType), True),
            },
            "skipTlsVerification": {
                "display_name": "Skip TLS Verification",
                "short_desc": "Skip TLS certificate verification",
                "desc": "If `True`, TLS certificate verification is skipped for any embedded "
                    "resource fetches performed during parsing. Defaults to `True`",
                "type": SoftBoolOrNothingType,
            },
            "removeBase64Images": {
                "display_name": "Remove Base64 Images",
                "short_desc": "Remove base64-encoded images",
                "desc": "If `True`, base64-encoded images are stripped from the output (alt "
                    "text is preserved). Defaults to `True`",
                "type": SoftBoolOrNothingType,
            },
            "blockAds": {
                "display_name": "Block Ads",
                "short_desc": "Block advertisements",
                "desc": "If `True`, advertisements and popups are blocked. Defaults to `True`",
                "type": SoftBoolOrNothingType,
            },
            "proxy": {
                "display_name": "Proxy",
                "short_desc": "Proxy configuration",
                "desc": "The proxy configuration to use for any embedded resource fetches "
                    "performed during parsing",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "basic", "display_name": "Basic"},
                    <AllowedValueInfo>{"value": "auto", "display_name": "Auto"},
                ),
            },
            "zeroDataRetention": {
                "display_name": "Zero Data Retention",
                "short_desc": "Disable Firecrawl-side data retention",
                "desc": "If `True`, Firecrawl does not retain any parsed data on its "
                    "servers after returning the response. Enterprise feature — contact "
                    "Firecrawl support to enable",
                "type": SoftBoolOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FirecrawlDataProvider
