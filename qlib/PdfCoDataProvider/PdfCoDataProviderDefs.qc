# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoDataProviderDefs.qc defines constants and definitions for the PdfCoDataProvider module

/*  PdfCoDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
    #! Application name constant
    public const AppName = "PdfCo";

    #! Maximum number of retries for transient errors
    public const MaxRetries = 3;

    #! Maximum number of job polling attempts before giving up
    public const MaxJobPollAttempts = 60;

    #! Initial polling interval
    public const JobPollInterval = 2s;

    #! Set of retriable socket errors
    public const RetrySet = {
        "SOCKET-CONNECT-ERROR": True,
        "SOCKET-SSL-ERROR": True,
        "SOCKET-CLOSED": True,
        "SOCKET-TIMEOUT": True,
    };

    #! Map of output format to PDF.co conversion endpoint path
    public const ConvertFromPdfFormatMap = {
        "csv": "pdf/convert/to/csv",
        "json": "pdf/convert/to/json2",
        "json-ai": "pdf/convert/to/json-meta",
        "text": "pdf/convert/to/text",
        "xls": "pdf/convert/to/xls",
        "xlsx": "pdf/convert/to/xlsx",
        "xml": "pdf/convert/to/xml",
        "html": "pdf/convert/to/html",
        "jpg": "pdf/convert/to/jpg",
        "png": "pdf/convert/to/png",
        "tiff": "pdf/convert/to/tiff",
    };

    #! Map of image output format to endpoint path
    public const ConvertToImageFormatMap = {
        "jpg": "pdf/convert/to/jpg",
        "png": "pdf/convert/to/png",
    };

    #! Map of HTML-to-PDF source type to endpoint path
    public const HtmlToPdfSourceMap = {
        "html": "pdf/convert/from/html",
        "url": "pdf/convert/from/url",
    };

    #! PDF.co logo in SVG format (loaded from file)
    public const PdfCoLogo = File::readTextFile(get_script_dir() + "/pdfco-logo.svg");
}
