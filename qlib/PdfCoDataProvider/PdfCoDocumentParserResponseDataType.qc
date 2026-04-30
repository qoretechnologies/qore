# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoDocumentParserResponseDataType.qc defines the response data type for PDF.co document parser

/*  PdfCoDocumentParserResponseDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! A single parsed object (field or table) returned by the document parser
public class PdfCoParsedObjectDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Field identifier from the parser template",
                "desc": "Field identifier as defined in the document parser template",
            },
            "objectType": {
                "type": StringOrNothingType,
                "display_name": "Object Type",
                "short_desc": "Kind of parsed object (`field` or `table`)",
                "desc": "Kind of parsed object: `field` for scalar values, `table` for tabular data",
            },
            "value": {
                "type": AbstractDataProviderTypeMap."any",
                "display_name": "Value",
                "short_desc": "Extracted value (string, number, or empty)",
                "desc": "Extracted value for the object. Type depends on the template definition; "
                    "may be a string, number, or empty string.",
            },
            "pageIndex": {
                "type": SoftIntOrNothingType,
                "display_name": "Page Index",
                "short_desc": "Zero-based page index where the object was found",
                "desc": "Zero-based page index where the object was located",
            },
            "rectangle": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Rectangle",
                "short_desc": "Bounding rectangle as `[x1, y1, x2, y2]`",
                "desc": "Bounding rectangle as `[x1, y1, x2, y2]` four-element coordinate array",
            },
            "rows": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Rows",
                "short_desc": "Parsed table rows (only for `table` objects)",
                "desc": "Parsed table rows; populated only when `objectType` is `table`",
            },
        };
    }

    constructor() : HashDataType("ParsedObject") {
        addQoreFields(Fields);
    }
}

#! Body payload returned by the document parser
public class PdfCoDocumentParserBodyDataType inherits HashDataType {
    public {
        const Fields = {
            "objects": {
                "type": new SoftListDataType("ParsedObjects", new PdfCoParsedObjectDataType(), True),
                "display_name": "Objects",
                "short_desc": "List of parsed objects extracted from the document",
                "desc": "List of parsed objects extracted from the document using the configured "
                    "template",
            },
            "templateName": {
                "type": StringOrNothingType,
                "display_name": "Template Name",
                "short_desc": "Name of the parser template used",
                "desc": "Name of the document parser template that was used",
            },
            "templateVersion": {
                "type": StringOrNothingType,
                "display_name": "Template Version",
                "short_desc": "Version of the parser template used",
                "desc": "Version of the document parser template that was used",
            },
            "timestamp": {
                "type": StringOrNothingType,
                "display_name": "Timestamp",
                "short_desc": "ISO 8601 timestamp when parsing completed",
                "desc": "ISO 8601 timestamp indicating when parsing completed",
            },
            "elapsed": {
                "type": SoftFloatOrNothingType,
                "display_name": "Elapsed Seconds",
                "short_desc": "Parsing duration in seconds",
                "desc": "Parsing duration in seconds",
            },
        };
    }

    constructor() : HashDataType("DocumentParserBody") {
        addQoreFields(Fields);
    }
}

#! Response data type for PDF.co document parser operations
public class PdfCoDocumentParserResponseDataType inherits HashDataType {
    public {
        #! Field definitions for the document parser response
        const Fields = {
            "body": {
                "type": (new PdfCoDocumentParserBodyDataType()).getOrNothingType(),
                "display_name": "Body",
                "short_desc": "Parsed document data with objects, template name, version, and timestamp",
                "desc": "Parsed document data containing `objects`, `templateName`, `templateVersion`, "
                    "and `timestamp` extracted using the configured document parser template",
            },
            "pageCount": {
                "type": SoftIntOrNothingType,
                "display_name": "Page Count",
                "short_desc": "Number of pages processed",
                "desc": "The number of pages processed by the document parser",
            },
            "error": {
                "type": SoftBoolOrNothingType,
                "display_name": "Error",
                "short_desc": "Indicates if the request resulted in an error",
                "desc": "`True` if the request resulted in an error, `False` otherwise",
            },
            "status": {
                "type": SoftIntOrNothingType,
                "display_name": "Status",
                "short_desc": "HTTP status code",
                "desc": "The HTTP status code of the response (e.g. `200` for success)",
            },
            "credits": {
                "type": SoftIntOrNothingType,
                "display_name": "Credits",
                "short_desc": "Number of credits consumed",
                "desc": "The number of PDF.co credits consumed by this operation",
            },
            "remainingCredits": {
                "type": SoftIntOrNothingType,
                "display_name": "Remaining Credits",
                "short_desc": "Credits remaining in the account",
                "desc": "The number of PDF.co credits remaining in the account after this operation",
            },
            "message": {
                "type": StringOrNothingType,
                "display_name": "Message",
                "short_desc": "Status or error message",
                "desc": "Status or error message from the API",
            },
            "jobId": {
                "type": StringOrNothingType,
                "display_name": "Job ID",
                "short_desc": "Async job identifier",
                "desc": "The job identifier for async operations; use with the `job-check` action "
                    "to poll for results",
            },
            "duration": {
                "type": SoftIntOrNothingType,
                "display_name": "Duration",
                "short_desc": "Processing time in milliseconds",
                "desc": "The processing time for the operation in milliseconds",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace PdfCoDataProvider
