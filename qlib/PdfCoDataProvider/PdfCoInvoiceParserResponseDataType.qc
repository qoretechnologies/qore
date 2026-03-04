# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoInvoiceParserResponseDataType.qc defines the response data type for PDF.co AI invoice parser

/*  PdfCoInvoiceParserResponseDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Response data type for PDF.co AI invoice parser operations
public class PdfCoInvoiceParserResponseDataType inherits HashDataType {
    public {
        #! Field definitions for the invoice parser response
        const Fields = {
            "body": {
                "type": AutoHashOrNothingType,
                "display_name": "Body",
                "short_desc": "Structured invoice data with vendor, customer, invoice, payment, and line item sections",
                "desc": "Structured invoice data containing `vendor`, `customer`, `invoice`, "
                    "`paymentDetails`, `lineItems`, and `others` sections extracted by the AI parser",
            },
            "pageCount": {
                "type": SoftIntOrNothingType,
                "display_name": "Page Count",
                "short_desc": "Number of pages processed",
                "desc": "The number of pages processed by the invoice parser",
            },
            "status": {
                "type": SoftIntOrNothingType,
                "display_name": "Status",
                "short_desc": "HTTP status code",
                "desc": "The HTTP status code of the response (e.g. `200` for success)",
            },
            "message": {
                "type": StringOrNothingType,
                "display_name": "Message",
                "short_desc": "Response message",
                "desc": "A message from the API describing the result of the operation",
            },
            "jobId": {
                "type": StringOrNothingType,
                "display_name": "Job ID",
                "short_desc": "The job identifier for async operations",
                "desc": "The unique job identifier that can be used to check the status of "
                    "asynchronous operations via the job check endpoint",
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
            "duration": {
                "type": SoftIntOrNothingType,
                "display_name": "Duration",
                "short_desc": "Processing time in milliseconds",
                "desc": "The processing time for the operation in milliseconds",
            },
            "error": {
                "type": SoftBoolOrNothingType,
                "display_name": "Error",
                "short_desc": "Indicates if the request resulted in an error",
                "desc": "`True` if the request resulted in an error, `False` otherwise",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace PdfCoDataProvider
