# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreDocumentOcrProcessor class definition

/** QoreDocumentOcrProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that extracts text from any document format
/** Accepts PDF documents, images (PNG, JPEG, TIFF, BMP, WebP, etc.), and SVG
    files. Automatically detects the format and applies the best text extraction
    strategy — digital text extraction for PDFs, OCR for images and scanned pages.

    Ideal for building document ingestion pipelines that need to handle any file
    format a user might upload.

    @par Example:
    @code{.py}
QoreDocumentOcrProcessor proc({
    "document_field": "file_data",
    "language": "eng",
});
    @endcode

*/
public class QoreDocumentOcrProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreDocumentOcrProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "document_field": <DataProviderOptionInfo>{
                "display_name": "Document Field",
                "short_desc": "Which field contains the document binary data",
                "desc": "The name of the field in each record containing the document "
                    "as binary data. Supported formats: PDF, PNG, JPEG, TIFF, BMP, GIF, "
                    "WebP, SVG, and any other format supported by ImageMagick.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "mime_type_field": <DataProviderOptionInfo>{
                "display_name": "MIME Type Field",
                "short_desc": "Optional field with the document MIME type",
                "desc": "The name of an optional field containing the document's MIME type "
                    "(e.g., `application/pdf`, `image/tiff`). If provided, helps with "
                    "format detection. If not provided, the format is detected from the "
                    "file content automatically.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
                "input_field_names": True,
            },
            "language": <DataProviderOptionInfo>{
                "display_name": "OCR Language",
                "short_desc": "Language for OCR text recognition",
                "desc": "Tesseract language code(s) for OCR. Use `+` to combine "
                    "languages (e.g., `eng+deu` for English and German). Default: `eng`.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
                "default_value": "eng",
            },
            "dpi": <DataProviderOptionInfo>{
                "display_name": "Scan Resolution (DPI)",
                "short_desc": "Resolution for rasterizing scanned PDF pages",
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
                        "desc": "Lower quality, faster — good for large, clear text",
                    },
                    <AllowedValueInfo>{
                        "value": 300,
                        "display_name": "Standard (300 DPI)",
                        "desc": "Recommended for most documents",
                    },
                    <AllowedValueInfo>{
                        "value": 600,
                        "display_name": "High Quality (600 DPI)",
                        "desc": "Best for small text or poor scans — slower",
                    },
                ),
                "allowed_values_creatable": True,
            },
            "scanned_threshold": <DataProviderOptionInfo>{
                "display_name": "Scanned Page Threshold",
                "short_desc": "Minimum characters for a PDF page to be considered digital",
                "desc": "If a PDF page has fewer than this many characters of extractable "
                    "text, it is treated as scanned and OCR is applied. Default: `50`.",
                "type": AbstractDataProviderTypeMap."int",
                "required": False,
                "default_value": 50,
            },
        };
    }

    private {
        #! Document field name
        string document_field;

        #! MIME type field name
        *string mime_type_field;

        #! Document OCR engine
        QoreOcrUtils::DocumentOcrEngine engine;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        document_field = copts.document_field;
        if (!document_field.val()) {
            throw "CONSTRUCTOR-ERROR", "document_field option is required";
        }
        mime_type_field = copts.mime_type_field;

        engine = new QoreOcrUtils::DocumentOcrEngine(<QoreOcrUtils::DocumentOcrConfig>{
            "language": copts.language ?? "eng",
            "dpi": copts.dpi ?? 300,
            "scanned_threshold": copts.scanned_threshold ?? 50,
        });

        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "document-ocr";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Extracts text from any document — PDFs, images, scanned pages, and more. "
            "Automatically detects the file format and uses the best strategy: digital "
            "text extraction for PDFs, OCR for images and scanned pages. Handles any "
            "format your users might upload.";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreDocumentOcrProcessor: not configured; provide constructor options";
        }

        hash<auto> result = {} + record;

        binary doc_data = DataProvider::AbstractDataProvider::getFieldValue(
            record, document_field);

        *string mime_type;
        if (mime_type_field.val()) {
            mime_type = DataProvider::AbstractDataProvider::getFieldValue(
                record, mime_type_field).toString();
        }

        hash<QoreOcrUtils::DocumentOcrResult> ocr_result =
            engine.processDocument(doc_data, mime_type);

        result.document_text = ocr_result.text;
        result.document_format = ocr_result.detected_format;
        result.document_pages = ocr_result.page_count;
        result.document_ocr_pages = ocr_result.ocr_page_count;
        result.document_confidence = ocr_result.avg_confidence;

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
