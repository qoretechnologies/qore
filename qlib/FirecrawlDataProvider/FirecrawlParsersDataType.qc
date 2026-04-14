# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlParsersDataType.qc parser configuration type for Firecrawl requests

/*  FirecrawlParsersDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Parser configuration type for Firecrawl requests
/** Configures PDF processing behavior. Each parser entry specifies a type and
    processing options.
*/
public class FirecrawlParsersDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "type": {
                "display_name": "Type",
                "short_desc": "Parser type",
                "desc": "The parser type. Currently only `pdf` is supported",
                "type": StringType,
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "pdf", "display_name": "PDF"},
                ),
            },
            "mode": {
                "display_name": "Mode",
                "short_desc": "PDF processing mode",
                "desc": "The PDF processing strategy to use.\n\n"
                    "- `fast` — text extraction only, no OCR\n"
                    "- `auto` — fast extraction with OCR fallback for scanned pages\n"
                    "- `ocr` — force OCR on all pages",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "fast", "display_name": "Fast"},
                    <AllowedValueInfo>{"value": "auto", "display_name": "Auto"},
                    <AllowedValueInfo>{"value": "ocr", "display_name": "OCR"},
                ),
            },
            "maxPages": {
                "display_name": "Max Pages",
                "short_desc": "Maximum number of pages to parse",
                "desc": "Maximum number of PDF pages to parse. Range: `1`-`10000`",
                "type": SoftIntOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

} # namespace FirecrawlDataProvider
