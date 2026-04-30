# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoPdfInfoResponseDataType.qc defines the response data type for PDF.co PDF info operations

/*  PdfCoPdfInfoResponseDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Point sub-structure used inside PdfCoPageRectangleDataType
public class PdfCoPagePointDataType inherits HashDataType {
    public {
        const Fields = {
            "IsEmpty": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Empty",
                "short_desc": "Whether the point is empty (zero-valued)",
                "desc": "`True` if the point is empty (zero-valued), `False` otherwise",
            },
            "X": {
                "type": SoftFloatOrNothingType,
                "display_name": "X",
                "short_desc": "X coordinate",
                "desc": "X coordinate",
            },
            "Y": {
                "type": SoftFloatOrNothingType,
                "display_name": "Y",
                "short_desc": "Y coordinate",
                "desc": "Y coordinate",
            },
        };
    }

    constructor() : HashDataType("PagePoint") {
        addQoreFields(Fields);
    }
}

#! Size sub-structure used inside PdfCoPageRectangleDataType
public class PdfCoPageSizeDataType inherits HashDataType {
    public {
        const Fields = {
            "IsEmpty": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Empty",
                "short_desc": "Whether the size is empty",
                "desc": "`True` if the size is empty, `False` otherwise",
            },
            "Width": {
                "type": SoftFloatOrNothingType,
                "display_name": "Width",
                "short_desc": "Width",
                "desc": "Width",
            },
            "Height": {
                "type": SoftFloatOrNothingType,
                "display_name": "Height",
                "short_desc": "Height",
                "desc": "Height",
            },
        };
    }

    constructor() : HashDataType("PageSize") {
        addQoreFields(Fields);
    }
}

#! Page rectangle dimensions
public class PdfCoPageRectangleDataType inherits HashDataType {
    public {
        const Fields = {
            "Location": {
                "type": (new PdfCoPagePointDataType()).getOrNothingType(),
                "display_name": "Location",
                "short_desc": "Top-left location of the rectangle",
                "desc": "Top-left location of the rectangle",
            },
            "Size": {
                "type": (new PdfCoPageSizeDataType()).getOrNothingType(),
                "display_name": "Size",
                "short_desc": "Width and height of the rectangle",
                "desc": "Width and height of the rectangle",
            },
            "X": {
                "type": SoftFloatOrNothingType,
                "display_name": "X",
                "short_desc": "X coordinate",
                "desc": "X coordinate of the rectangle",
            },
            "Y": {
                "type": SoftFloatOrNothingType,
                "display_name": "Y",
                "short_desc": "Y coordinate",
                "desc": "Y coordinate of the rectangle",
            },
            "Width": {
                "type": SoftFloatOrNothingType,
                "display_name": "Width",
                "short_desc": "Rectangle width",
                "desc": "Rectangle width",
            },
            "Height": {
                "type": SoftFloatOrNothingType,
                "display_name": "Height",
                "short_desc": "Rectangle height",
                "desc": "Rectangle height",
            },
            "Left": {
                "type": SoftFloatOrNothingType,
                "display_name": "Left",
                "short_desc": "Left edge X coordinate",
                "desc": "Left edge X coordinate",
            },
            "Top": {
                "type": SoftFloatOrNothingType,
                "display_name": "Top",
                "short_desc": "Top edge Y coordinate",
                "desc": "Top edge Y coordinate",
            },
            "Right": {
                "type": SoftFloatOrNothingType,
                "display_name": "Right",
                "short_desc": "Right edge X coordinate",
                "desc": "Right edge X coordinate",
            },
            "Bottom": {
                "type": SoftFloatOrNothingType,
                "display_name": "Bottom",
                "short_desc": "Bottom edge Y coordinate",
                "desc": "Bottom edge Y coordinate",
            },
            "IsEmpty": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Empty",
                "short_desc": "Whether the rectangle is empty",
                "desc": "`True` if the rectangle is empty, `False` otherwise",
            },
        };
    }

    constructor() : HashDataType("PageRectangle") {
        addQoreFields(Fields);
    }
}

#! Element type for the PDF info hash returned by /pdf/info
public class PdfCoPdfInfoDataType inherits HashDataType {
    public {
        const Fields = {
            "PageCount": {
                "type": SoftIntOrNothingType,
                "display_name": "Page Count",
                "short_desc": "Number of pages in the document",
                "desc": "Number of pages in the document",
            },
            "Author": {
                "type": StringOrNothingType,
                "display_name": "Author",
                "short_desc": "Document author",
                "desc": "Document author from the PDF metadata",
            },
            "Title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "Document title",
                "desc": "Document title from the PDF metadata",
            },
            "Producer": {
                "type": StringOrNothingType,
                "display_name": "Producer",
                "short_desc": "PDF producer application",
                "desc": "Application that produced the PDF",
            },
            "Subject": {
                "type": StringOrNothingType,
                "display_name": "Subject",
                "short_desc": "Document subject",
                "desc": "Document subject from the PDF metadata",
            },
            "Creator": {
                "type": StringOrNothingType,
                "display_name": "Creator",
                "short_desc": "Application that created the document",
                "desc": "Application that created the document content (before conversion to PDF)",
            },
            "Keywords": {
                "type": StringOrNothingType,
                "display_name": "Keywords",
                "short_desc": "Associated keywords",
                "desc": "Keywords associated with the document",
            },
            "CreationDate": {
                "type": SoftDateOrNothingType,
                "display_name": "Creation Date",
                "short_desc": "When the document was created",
                "desc": "Timestamp when the document was created",
            },
            "ModificationDate": {
                "type": SoftDateOrNothingType,
                "display_name": "Modification Date",
                "short_desc": "Last modification timestamp",
                "desc": "Timestamp of the last modification to the document",
            },
            "Encrypted": {
                "type": SoftBoolOrNothingType,
                "display_name": "Encrypted",
                "short_desc": "Whether the document is encrypted",
                "desc": "`True` if the document is encrypted, `False` otherwise",
            },
            "EncryptionAlgorithm": {
                "type": StringOrNothingType,
                "display_name": "Encryption Algorithm",
                "short_desc": "Encryption algorithm used",
                "desc": "Encryption algorithm used by the document, if encrypted",
            },
            "PageRectangle": {
                "type": (new PdfCoPageRectangleDataType()).getOrNothingType(),
                "display_name": "Page Rectangle",
                "short_desc": "Page dimensions",
                "desc": "Page dimensions including `Location`, `Size`, `X`, `Y`, `Width`, `Height`, "
                    "`Left`, `Top`, `Right`, `Bottom`, and `IsEmpty`",
            },
            "Bookmarks": {
                "type": StringOrNothingType,
                "display_name": "Bookmarks",
                "short_desc": "Document bookmarks",
                "desc": "Document bookmarks as a string representation",
            },
            "PermissionPrinting": {
                "type": SoftBoolOrNothingType,
                "display_name": "Permission: Printing",
                "short_desc": "Whether printing is permitted",
                "desc": "`True` if printing is permitted, `False` otherwise",
            },
            "PermissionModifyDocument": {
                "type": SoftBoolOrNothingType,
                "display_name": "Permission: Modify Document",
                "short_desc": "Whether modifying the document is permitted",
                "desc": "`True` if modifying the document is permitted, `False` otherwise",
            },
            "PermissionContentExtraction": {
                "type": SoftBoolOrNothingType,
                "display_name": "Permission: Content Extraction",
                "short_desc": "Whether content extraction is permitted",
                "desc": "`True` if content extraction is permitted, `False` otherwise",
            },
            "PermissionModifyAnnotations": {
                "type": SoftBoolOrNothingType,
                "display_name": "Permission: Modify Annotations",
                "short_desc": "Whether modifying annotations is permitted",
                "desc": "`True` if modifying annotations is permitted, `False` otherwise",
            },
            "PermissionFillForms": {
                "type": SoftBoolOrNothingType,
                "display_name": "Permission: Fill Forms",
                "short_desc": "Whether filling forms is permitted",
                "desc": "`True` if filling form fields is permitted, `False` otherwise",
            },
            "PermissionAccessibility": {
                "type": SoftBoolOrNothingType,
                "display_name": "Permission: Accessibility",
                "short_desc": "Whether accessibility content extraction is permitted",
                "desc": "`True` if content extraction for accessibility is permitted, `False` otherwise",
            },
            "PermissionAssemble": {
                "type": SoftBoolOrNothingType,
                "display_name": "Permission: Assemble",
                "short_desc": "Whether document assembly is permitted",
                "desc": "`True` if assembling the document (inserting/rotating/deleting pages) "
                    "is permitted, `False` otherwise",
            },
            "PermissionHighQualityPrint": {
                "type": SoftBoolOrNothingType,
                "display_name": "Permission: High-Quality Print",
                "short_desc": "Whether high-quality printing is permitted",
                "desc": "`True` if high-quality printing is permitted, `False` otherwise",
            },
        };
    }

    constructor() : HashDataType("PdfCoPdfInfo") {
        addQoreFields(Fields);
    }
}

#! Response data type for PDF.co PDF info operations
public class PdfCoPdfInfoResponseDataType inherits HashDataType {
    public {
        #! Field definitions for the PDF info response
        const Fields = {
            "info": {
                "type": (new PdfCoPdfInfoDataType()).getOrNothingType(),
                "display_name": "Info",
                "short_desc": "PDF metadata including page count, author, title, and security permissions",
                "desc": "PDF metadata including `PageCount`, `Author`, `Title`, `Producer`, "
                    "`Encrypted`, and security permission fields",
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
