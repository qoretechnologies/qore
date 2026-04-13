# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QorePdfOcrProcessor class definition

/** QorePdfOcrProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProviderML module
public namespace DataProviderML {
#! Pipeline processor that extracts text from PDF documents, using OCR for scanned pages
/** Processes PDF documents by first extracting the digital text layer from each
    page. If a page has little or no text (indicating it is scanned), the page is
    rasterized and OCR is applied using Tesseract.

    This is useful for processing contracts, invoices, reports, and other business
    documents that may contain a mix of digital text and scanned images.

    @par Example:
    @code{.py}
QorePdfOcrProcessor proc({
    "pdf_field": "document_data",
    "language": "eng",
    "dpi": 300,
});
    @endcode

*/
public class QorePdfOcrProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QorePdfOcrProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "pdf_field": <DataProviderOptionInfo>{
                "display_name": "PDF Field",
                "short_desc": "Which field contains the PDF binary data",
                "desc": "The name of the field in each record containing the PDF document "
                    "as binary data. The processor will extract text from all pages, "
                    "automatically using OCR on any scanned pages.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "language": <DataProviderOptionInfo>{
                "display_name": "OCR Language",
                "short_desc": "Language for OCR on scanned pages",
                "desc": "Tesseract language code(s) for OCR on scanned pages. Use `+` "
                    "to combine languages (e.g., `eng+fra` for English and French). "
                    "Only used for pages that require OCR. Default: `eng`.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
                "default_value": "eng",
            },
            "dpi": <DataProviderOptionInfo>{
                "display_name": "Scan Resolution (DPI)",
                "short_desc": "Resolution for rasterizing scanned pages",
                "desc": "Resolution in dots per inch for rasterizing scanned PDF pages "
                    "before OCR. Higher values improve accuracy but use more memory.\n\n"
                    "- `150` — fast, good for large clear text\n"
                    "- `300` — recommended for most documents (default)\n"
                    "- `600` — best for small text or poor quality scans",
                "type": AbstractDataProviderTypeMap."int",
                "required": False,
                "default_value": 300,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": 150,
                        "display_name": "Fast (150 DPI)",
                        "desc": "Lower quality, faster processing — good for large, clear text",
                    },
                    <AllowedValueInfo>{
                        "value": 300,
                        "display_name": "Standard (300 DPI)",
                        "desc": "Recommended for most documents — good balance of speed and accuracy",
                    },
                    <AllowedValueInfo>{
                        "value": 600,
                        "display_name": "High Quality (600 DPI)",
                        "desc": "Best for small text or poor quality scans — slower processing",
                    },
                ),
                "allowed_values_creatable": True,
            },
            "scanned_threshold": <DataProviderOptionInfo>{
                "display_name": "Scanned Page Threshold",
                "short_desc": "Minimum characters for a page to be considered digital",
                "desc": "If a PDF page has fewer than this many characters of text, it "
                    "is treated as a scanned page and OCR is applied. Pages with more "
                    "text are assumed to be digital and their text is used directly.\n\n"
                    "Lower values mean fewer pages get OCR'd (only truly blank pages). "
                    "Higher values are more aggressive about applying OCR. Default: `50`.",
                "type": AbstractDataProviderTypeMap."int",
                "required": False,
                "default_value": 50,
            },
        };
    }

    private {
        #! PDF field name
        string pdf_field;

        #! PDF OCR engine
        QoreOcrUtils::PdfOcrEngine engine;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        pdf_field = copts.pdf_field;
        if (!pdf_field.val()) {
            throw "CONSTRUCTOR-ERROR", "pdf_field option is required";
        }

        engine = new QoreOcrUtils::PdfOcrEngine(<QoreOcrUtils::PdfOcrConfig>{
            "language": copts.language ?? "eng",
            "dpi": copts.dpi ?? 300,
            "scanned_threshold": copts.scanned_threshold ?? 50,
        });

        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "pdf-ocr";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Extracts text from PDF documents, automatically applying OCR to scanned "
            "pages — handles contracts, invoices, and reports that mix digital text with "
            "scanned images; each record is enriched with the full document text and "
            "per-page details";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record: extracts text from PDF and enriches the record
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QorePdfOcrProcessor: not configured; provide constructor options";
        }

        hash<auto> result = {} + record;

        binary pdf_data = DataProvider::AbstractDataProvider::getFieldValue(
            record, pdf_field);

        hash<QoreOcrUtils::PdfOcrResult> ocr_result = engine.processPdf(pdf_data);

        result.pdf_text = ocr_result.text;
        result.pdf_total_pages = ocr_result.total_pages;
        result.pdf_ocr_pages = ocr_result.ocr_page_count;
        result.pdf_page_details = ocr_result.pages;

        return result;
    }

    #! Does not support bulk API
    private bool supportsBulkApiImpl() {
        return False;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_TRANSFORM;
    }

    #! Returns NOTHING — output type is dynamic
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return NOTHING;
    }
}
}
