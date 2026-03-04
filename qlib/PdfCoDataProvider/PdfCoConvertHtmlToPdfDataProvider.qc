# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoConvertHtmlToPdfDataProvider.qc PDF.co convert HTML to PDF action data provider

/*  PdfCoConvertHtmlToPdfDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the PdfCoDataProvider module
public namespace PdfCoDataProvider {

#! Request type for converting HTML to PDF
public class PdfCoConvertHtmlToPdfRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file": {
                "type": FileOrNothingDataType,
                "display_name": "File",
                "short_desc": "HTML file to convert",
                "desc": "HTML file to convert to PDF. When provided, the file is uploaded and "
                    "converted via URL.",
                "required_groups": ("source",),
                "preselected": True,
            },
            "url": {
                "type": StringOrNothingType,
                "display_name": "URL",
                "short_desc": "URL of the web page to convert",
                "desc": "URL of the web page to convert (requires `source_type` set to `url`)",
                "required_groups": ("source",),
            },
            "html": {
                "type": StringOrNothingType,
                "display_name": "HTML",
                "short_desc": "Raw HTML code to convert",
                "desc": "Raw HTML code to convert (requires `source_type` set to `html`)",
                "required_groups": ("source",),
            },
            "source_type": {
                "type": StringOrNothingType,
                "display_name": "Source Type",
                "short_desc": "Source type for conversion",
                "desc": "Source type for conversion. Not required when a file is provided.",
                "preselected": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "html", "desc": "Convert raw HTML code to PDF"},
                    <AllowedValueInfo>{"value": "url", "desc": "Convert a web page URL to PDF"},
                ),
            },
            "margins": {
                "type": StringOrNothingType,
                "display_name": "Margins",
                "short_desc": "Custom page margins",
                "desc": "Custom page margins (e.g., `5mm`, supports `px`, `mm`, `cm`, `in`)",
            },
            "paperSize": {
                "type": StringOrNothingType,
                "display_name": "Paper Size",
                "short_desc": "Page size",
                "desc": "Page size.\n\n"
                    "Select a standard size or enter custom dimensions (e.g., `200 300`).",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Letter", "desc": "US Letter (8.5 x 11 in)"},
                    <AllowedValueInfo>{"value": "Legal", "desc": "US Legal (8.5 x 14 in)"},
                    <AllowedValueInfo>{"value": "Tabloid", "desc": "US Tabloid (11 x 17 in)"},
                    <AllowedValueInfo>{"value": "Ledger", "desc": "US Ledger (17 x 11 in)"},
                    <AllowedValueInfo>{"value": "A0", "desc": "ISO A0 (841 x 1189 mm)"},
                    <AllowedValueInfo>{"value": "A1", "desc": "ISO A1 (594 x 841 mm)"},
                    <AllowedValueInfo>{"value": "A2", "desc": "ISO A2 (420 x 594 mm)"},
                    <AllowedValueInfo>{"value": "A3", "desc": "ISO A3 (297 x 420 mm)"},
                    <AllowedValueInfo>{"value": "A4", "desc": "ISO A4 (210 x 297 mm)"},
                    <AllowedValueInfo>{"value": "A5", "desc": "ISO A5 (148 x 210 mm)"},
                ),
            },
            "orientation": {
                "type": StringOrNothingType,
                "display_name": "Orientation",
                "short_desc": "Page orientation",
                "desc": "Page orientation",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Portrait", "desc": "Portrait orientation"},
                    <AllowedValueInfo>{"value": "Landscape", "desc": "Landscape orientation"},
                ),
            },
            "header": {
                "type": StringOrNothingType,
                "display_name": "Header",
                "short_desc": "Custom HTML for page headers",
                "desc": "Custom HTML for page headers.\n\n"
                    "Supports special CSS classes: `pageNumber` (current page) and "
                    "`totalPages` (total pages).\n\n"
                    "**Example**: `<div style=\"text-align:center\">"
                    "<span class=\"pageNumber\"></span> of "
                    "<span class=\"totalPages\"></span></div>`",
            },
            "footer": {
                "type": StringOrNothingType,
                "display_name": "Footer",
                "short_desc": "Custom HTML for page footers",
                "desc": "Custom HTML for page footers.\n\n"
                    "Supports special CSS classes: `pageNumber` (current page) and "
                    "`totalPages` (total pages).\n\n"
                    "**Example**: `<div style=\"text-align:center\">"
                    "<span class=\"pageNumber\"></span> of "
                    "<span class=\"totalPages\"></span></div>`",
            },
            "printBackground": {
                "type": SoftBoolOrNothingType,
                "display_name": "Print Background",
                "short_desc": "Include background colors and images",
                "desc": "Set to `True` to include background colors and images",
            },
            "mediaType": {
                "type": StringOrNothingType,
                "display_name": "Media Type",
                "short_desc": "CSS media type",
                "desc": "CSS media type",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "print", "desc": "Print media type"},
                    <AllowedValueInfo>{"value": "screen", "desc": "Screen media type"},
                    <AllowedValueInfo>{"value": "none", "desc": "No media type"},
                ),
            },
            "async": {
                "type": SoftBoolOrNothingType,
                "display_name": "Async",
                "short_desc": "Run as a background job",
                "desc": "Set to `True` to run as a background job",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Output Filename",
                "short_desc": "Output filename",
                "desc": "Output filename",
            },
            "expiration": {
                "type": SoftIntOrNothingType,
                "display_name": "Expiration",
                "short_desc": "Output link expiration time in minutes",
                "desc": "Output link expiration time in minutes (default: `60`)",
            },
            "profiles": {
                "type": StringOrNothingType,
                "display_name": "Profiles",
                "short_desc": "Advanced processing profiles",
                "desc": "Advanced processing profiles as a JSON string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! PDF.co convert HTML to PDF action data provider
public class PdfCoConvertHtmlToPdfDataProvider inherits PdfCoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "convert-html-to-pdf",
            "desc": "Converts HTML code, HTML templates, or URLs to PDF",
            "type": "PdfCoConvertHtmlToPdfDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(PdfCoRestClient::PdfCoRestClient rest) : PdfCoDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "convert-html-to-pdf";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Converts HTML code, HTML templates, or URLs to PDF";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new PdfCoConvertHtmlToPdfRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new PdfCoUrlResponseDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # If file is provided, upload it and use URL source type
        if (req.file) {
            req = uploadFileIfNeeded(req);
            req.source_type = "url";
        }
        string source_type = remove req.source_type ?? "html";
        *string path = HtmlToPdfSourceMap{source_type};
        if (!path) {
            throw "PDFCO-ERROR", sprintf("unsupported source type: %y; supported types: %y",
                source_type, keys HtmlToPdfSourceMap);
        }
        return doPost(path, req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace PdfCoDataProvider
