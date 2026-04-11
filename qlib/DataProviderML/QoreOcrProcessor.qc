# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreOcrProcessor class definition

/** QoreOcrProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that extracts text from images via OCR
/** Uses the QoreOcrUtils module to run OCR on an image field in each record.
    Enriches records with extracted text and confidence.

    @par Example:
    @code{.py}
QoreOcrProcessor proc({
    "image_field": "image_data",
    "ocr_mode": "native",
    "language": "eng",
});
    @endcode
*/
public class QoreOcrProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreOcrProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "image_field": <DataProviderOptionInfo>{
                "display_name": "Image Field",
                "short_desc": "Which field contains image binary data",
                "desc": "The name of the field in each record containing the image "
                    "binary data (PNG, JPEG, TIFF, BMP, GIF) to OCR.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "ocr_mode": <DataProviderOptionInfo>{
                "display_name": "OCR Mode",
                "short_desc": "How to perform OCR",
                "desc": "The OCR backend mode:\n"
                    "- `native` — Use the local Tesseract OCR engine\n"
                    "- `http` — Call an HTTP OCR endpoint",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "native",
                        "display_name": "Native (Tesseract)",
                        "desc": "Use the local Tesseract OCR engine",
                    },
                    <AllowedValueInfo>{
                        "value": "http",
                        "display_name": "HTTP Endpoint",
                        "desc": "Call an HTTP OCR service",
                    },
                ),
            },
            "language": <DataProviderOptionInfo>{
                "display_name": "Language",
                "short_desc": "OCR language code(s)",
                "desc": "Tesseract language code(s) for native mode. Use `+` to "
                    "combine (e.g., `eng+fra` for English and French). Default `eng`.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
                "default_value": "eng",
            },
            "tessdata_path": <DataProviderOptionInfo>{
                "display_name": "Tessdata Path",
                "short_desc": "Path to tessdata directory",
                "desc": "Filesystem path to the tessdata directory containing Tesseract "
                    "trained models. Leave empty for the system default.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
            },
            "http_url": <DataProviderOptionInfo>{
                "display_name": "HTTP URL",
                "short_desc": "URL of the HTTP OCR service",
                "desc": "Base URL of the HTTP OCR service. Required for `http` mode.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
            },
        };
    }

    private {
        #! Image field name
        string image_field;

        #! OCR engine
        QoreOcrUtils::OcrEngine engine;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        image_field = copts.image_field;
        if (!image_field.val()) {
            throw "CONSTRUCTOR-ERROR", "image_field option is required";
        }

        string ocr_mode = copts.ocr_mode;
        if (!ocr_mode.val()) {
            throw "CONSTRUCTOR-ERROR", "ocr_mode option is required";
        }

        hash<QoreOcrUtils::OcrConfig> ocr_config(<QoreOcrUtils::OcrConfig>{
            "mode": ocr_mode,
            "language": copts.language ?? "eng",
            "datapath": copts.tessdata_path ?? "",
        });

        if (ocr_mode == "http") {
            if (!copts.http_url.val()) {
                throw "CONSTRUCTOR-ERROR", "http_url is required for HTTP mode";
            }
            ocr_config.url = copts.http_url;
        }

        engine = new QoreOcrUtils::OcrEngine(ocr_config);
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "ocr";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Extracts text from images using OCR — converts scanned documents, "
            "photos of whiteboards, or screenshots into searchable text; adds "
            "extracted text and confidence to each record";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record: OCRs the image and enriches the record
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreOcrProcessor: not configured; provide constructor options";
        }

        hash<auto> result = {} + record;

        binary image_data = DataProvider::AbstractDataProvider::getFieldValue(
            record, image_field);

        hash<QoreOcrUtils::OcrResult> ocr_result = engine.extractText(image_data);

        result.ocr_text = ocr_result.text;
        result.ocr_confidence = ocr_result.confidence;

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
