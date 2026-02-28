# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoBarcodeReadResponseDataType.qc defines the response data type for PDF.co barcode read operations

/*  PdfCoBarcodeReadResponseDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Response data type for PDF.co barcode read operations
public class PdfCoBarcodeReadResponseDataType inherits HashDataType {
    public {
        #! Field definitions for the barcode read response
        const Fields = {
            "barcodes": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Barcodes",
                "short_desc": "List of detected barcodes with value, type, page, confidence, and position data",
                "desc": "List of detected barcodes, each containing `Value`, `Type`, `Page`, "
                    "`Confidence`, and position data (`Rect`)",
            },
            "pageCount": {
                "type": SoftIntOrNothingType,
                "display_name": "Page Count",
                "short_desc": "Number of pages scanned",
                "desc": "The number of pages scanned for barcodes",
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
