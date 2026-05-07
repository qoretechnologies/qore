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

    #! Allowed values for the barcode `type` parameter shared by generate-barcode and read-barcode
    /** Sourced from the PDF.co barcode types documentation. Common formats appear first; less
        common formats follow.
    */
    public const PdfCoBarcodeTypeAllowedValues = (
        <AllowedValueInfo>{
            "value": "QRCode", "display_name": "QR Code",
            "desc": "Two-dimensional QR code",
        },
        <AllowedValueInfo>{
            "value": "DataMatrix", "display_name": "Data Matrix",
            "desc": "Two-dimensional Data Matrix barcode",
        },
        <AllowedValueInfo>{
            "value": "Aztec", "display_name": "Aztec",
            "desc": "Two-dimensional Aztec barcode",
        },
        <AllowedValueInfo>{
            "value": "PDF417", "display_name": "PDF417",
            "desc": "Stacked linear PDF417 barcode",
        },
        <AllowedValueInfo>{
            "value": "MicroPDF", "display_name": "Micro PDF417",
            "desc": "Micro PDF417 barcode (compact PDF417)",
        },
        <AllowedValueInfo>{
            "value": "MaxiCode", "display_name": "MaxiCode",
            "desc": "Two-dimensional MaxiCode barcode used for shipping",
        },
        <AllowedValueInfo>{
            "value": "Code128", "display_name": "Code 128",
            "desc": "High-density linear Code 128 barcode",
        },
        <AllowedValueInfo>{
            "value": "Code39", "display_name": "Code 39",
            "desc": "Linear Code 39 barcode",
        },
        <AllowedValueInfo>{
            "value": "Code39Extended", "display_name": "Code 39 Extended",
            "desc": "Code 39 with full ASCII support",
        },
        <AllowedValueInfo>{
            "value": "Code93", "display_name": "Code 93",
            "desc": "Linear Code 93 barcode",
        },
        <AllowedValueInfo>{
            "value": "Codabar", "display_name": "Codabar",
            "desc": "Linear Codabar barcode",
        },
        <AllowedValueInfo>{
            "value": "CodablockF", "display_name": "Codablock F",
            "desc": "Stacked Codablock F barcode",
        },
        <AllowedValueInfo>{
            "value": "Interleaved2of5", "display_name": "Interleaved 2 of 5",
            "desc": "Interleaved 2 of 5 numeric barcode",
        },
        <AllowedValueInfo>{
            "value": "ITF14", "display_name": "ITF-14",
            "desc": "ITF-14 GTIN barcode for shipping",
        },
        <AllowedValueInfo>{
            "value": "MSI", "display_name": "MSI",
            "desc": "MSI Plessey barcode",
        },
        <AllowedValueInfo>{
            "value": "Pharmacode", "display_name": "Pharmacode",
            "desc": "Pharmacode pharmaceutical barcode",
        },
        <AllowedValueInfo>{
            "value": "PZN", "display_name": "PZN",
            "desc": "German Pharmazentralnummer barcode",
        },
        <AllowedValueInfo>{
            "value": "EAN13", "display_name": "EAN-13",
            "desc": "European Article Number (13-digit)",
        },
        <AllowedValueInfo>{
            "value": "EAN8", "display_name": "EAN-8",
            "desc": "European Article Number (8-digit)",
        },
        <AllowedValueInfo>{
            "value": "EAN5", "display_name": "EAN-5",
            "desc": "European Article Number 5-digit add-on",
        },
        <AllowedValueInfo>{
            "value": "EAN2", "display_name": "EAN-2",
            "desc": "European Article Number 2-digit add-on",
        },
        <AllowedValueInfo>{
            "value": "UPCA", "display_name": "UPC-A",
            "desc": "Universal Product Code (12-digit)",
        },
        <AllowedValueInfo>{
            "value": "UPCE", "display_name": "UPC-E",
            "desc": "Universal Product Code (compressed)",
        },
        <AllowedValueInfo>{
            "value": "GTIN8", "display_name": "GTIN-8",
            "desc": "Global Trade Item Number (8-digit)",
        },
        <AllowedValueInfo>{
            "value": "GTIN12", "display_name": "GTIN-12",
            "desc": "Global Trade Item Number (12-digit)",
        },
        <AllowedValueInfo>{
            "value": "GTIN13", "display_name": "GTIN-13",
            "desc": "Global Trade Item Number (13-digit)",
        },
        <AllowedValueInfo>{
            "value": "GTIN14", "display_name": "GTIN-14",
            "desc": "Global Trade Item Number (14-digit)",
        },
        <AllowedValueInfo>{
            "value": "GS1-128", "display_name": "GS1-128",
            "desc": "GS1-128 barcode (formerly UCC/EAN-128)",
        },
        <AllowedValueInfo>{
            "value": "GS1DataBarOmnidirectional", "display_name": "GS1 DataBar Omnidirectional",
            "desc": "GS1 DataBar omnidirectional barcode",
        },
        <AllowedValueInfo>{
            "value": "GS1DataBarStacked", "display_name": "GS1 DataBar Stacked",
            "desc": "GS1 DataBar stacked barcode",
        },
        <AllowedValueInfo>{
            "value": "GS1DataBarLimited", "display_name": "GS1 DataBar Limited",
            "desc": "GS1 DataBar limited barcode",
        },
        <AllowedValueInfo>{
            "value": "GS1DataBarExpanded", "display_name": "GS1 DataBar Expanded",
            "desc": "GS1 DataBar expanded barcode",
        },
        <AllowedValueInfo>{
            "value": "GS1DataBarExpandedStacked", "display_name": "GS1 DataBar Expanded Stacked",
            "desc": "GS1 DataBar expanded stacked barcode",
        },
        <AllowedValueInfo>{
            "value": "IntelligentMail", "display_name": "Intelligent Mail",
            "desc": "USPS Intelligent Mail barcode (IMb)",
        },
        <AllowedValueInfo>{
            "value": "PostNet", "display_name": "PostNet",
            "desc": "USPS PostNet ZIP-code barcode",
        },
        <AllowedValueInfo>{
            "value": "RoyalMail", "display_name": "Royal Mail",
            "desc": "Royal Mail 4-state barcode (RM4SCC)",
        },
        <AllowedValueInfo>{
            "value": "RoyalMailKIX", "display_name": "Royal Mail KIX",
            "desc": "Royal Mail Klant Index barcode",
        },
        <AllowedValueInfo>{
            "value": "AustralianPostCode", "display_name": "Australia Post",
            "desc": "Australia Post 4-state barcode",
        },
        <AllowedValueInfo>{
            "value": "UPU", "display_name": "UPU S10",
            "desc": "Universal Postal Union S10 barcode",
        },
    );

    #! Common fields supported by every endpoint that fetches the source from a URL
    /** PDF.co's URL-based endpoints uniformly accept HTTP basic-auth credentials for the
        source URL, an async webhook URL, and free-form processing profiles. Mix these into
        request type \c Fields when the action sends a remote URL to PDF.co.
    */
    public const PdfCoRemoteSourceFields = {
        "httpusername": {
            "type": StringOrNothingType,
            "display_name": "HTTP Username",
            "short_desc": "HTTP basic-auth username for the source URL",
            "desc": "HTTP basic-auth username if the source `url` requires authentication",
        },
        "httppassword": {
            "type": StringOrNothingType,
            "display_name": "HTTP Password",
            "short_desc": "HTTP basic-auth password for the source URL",
            "desc": "HTTP basic-auth password if the source `url` requires authentication",
            "sensitive": True,
        },
        "callback": {
            "type": StringOrNothingType,
            "display_name": "Callback URL",
            "short_desc": "Webhook URL invoked when the async job completes",
            "desc": "Webhook URL that PDF.co will invoke when an async job completes. "
                "Only applies when `async` is set to `True`.",
        },
        "profiles": {
            "type": StringOrNothingType,
            "display_name": "Profiles",
            "short_desc": "Advanced processing profiles as a JSON string",
            "desc": "Advanced processing profiles as a JSON string for fine-grained control "
                "over OCR, encryption, and other internal options.\n\n"
                "**Example**: `{\"OCRMode\": \"TextFromImagesAndVectorsLowRes\", "
                "\"OCRResolution\": 300}`",
        },
    };
}
