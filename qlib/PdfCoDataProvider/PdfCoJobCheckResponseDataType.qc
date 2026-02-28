# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoJobCheckResponseDataType.qc defines the response data type for PDF.co job check operations

/*  PdfCoJobCheckResponseDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Response data type for PDF.co job check operations
public class PdfCoJobCheckResponseDataType inherits HashDataType {
    public {
        #! Field definitions for the job check response
        const Fields = {
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Job status: working, success, failed, aborted, or unknown",
                "desc": "The current job status: `working`, `success`, `failed`, `aborted`, "
                    "or `unknown`",
            },
            "message": {
                "type": StringOrNothingType,
                "display_name": "Message",
                "short_desc": "Response message",
                "desc": "A message from the API describing the current state of the job",
            },
            "url": {
                "type": StringOrNothingType,
                "display_name": "URL",
                "short_desc": "Direct URL to the output file (when status is success)",
                "desc": "Direct URL to the output file; only available when `status` is `success`",
            },
            "pageCount": {
                "type": SoftIntOrNothingType,
                "display_name": "Page Count",
                "short_desc": "Number of pages in the output",
                "desc": "The number of pages in the output document",
            },
            "outputLinkValidTill": {
                "type": StringOrNothingType,
                "display_name": "Output Link Valid Till",
                "short_desc": "Expiration timestamp for the output link",
                "desc": "The expiration timestamp for the output URL; the link will not be accessible "
                    "after this time",
            },
            "jobId": {
                "type": StringOrNothingType,
                "display_name": "Job ID",
                "short_desc": "The job identifier",
                "desc": "The unique job identifier for this asynchronous operation",
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
            "jobDuration": {
                "type": SoftIntOrNothingType,
                "display_name": "Job Duration",
                "short_desc": "Total job execution time in milliseconds",
                "desc": "The total job execution time in milliseconds from submission to completion",
            },
            "duration": {
                "type": SoftIntOrNothingType,
                "display_name": "Duration",
                "short_desc": "Operation duration in milliseconds",
                "desc": "The duration of the check operation in milliseconds",
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
