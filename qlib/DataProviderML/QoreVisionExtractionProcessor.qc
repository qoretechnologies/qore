# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreVisionExtractionProcessor class definition

/** QoreVisionExtractionProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that extracts information from images using a vision LLM
/** Uses a multimodal LLM (GPT-4o, Claude) to describe images, extract tables,
    or extract text from images. Enriches each record with the extracted content.

    @par Example:
    @code{.py}
QoreVisionExtractionProcessor proc({
    "image_field": "image_data",
    "mime_type_field": "content_type",
    "extraction_type": "describe",
    "vision_mode": "openai",
    "vision_url": "https://api.openai.com",
    "vision_model": "gpt-4o",
    "vision_api_key": ENV.OPENAI_API_KEY,
});
    @endcode
*/
public class QoreVisionExtractionProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreVisionExtractionProcessor",
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
                    "binary data to analyze.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "mime_type_field": <DataProviderOptionInfo>{
                "display_name": "MIME Type Field",
                "short_desc": "Field containing the image MIME type",
                "desc": "The name of the field containing the MIME type of the image "
                    "(e.g., `image/png`, `image/jpeg`). If not provided, defaults "
                    "to `image/png`.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
                "input_field_names": True,
            },
            "extraction_type": <DataProviderOptionInfo>{
                "display_name": "Extraction Type",
                "short_desc": "What to extract from the image",
                "desc": "The type of extraction to perform:\n"
                    "- `describe` — Generate a detailed description of the image\n"
                    "- `extract_table` — Extract tables as markdown\n"
                    "- `extract_text` — Extract visible text preserving layout",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "describe",
                        "display_name": "Describe Image",
                        "desc": "Generate a detailed description of the image content",
                    },
                    <AllowedValueInfo>{
                        "value": "extract_table",
                        "display_name": "Extract Tables",
                        "desc": "Extract tables as markdown format",
                    },
                    <AllowedValueInfo>{
                        "value": "extract_text",
                        "display_name": "Extract Text",
                        "desc": "Extract all visible text preserving layout",
                    },
                ),
            },
            "vision_mode": <DataProviderOptionInfo>{
                "display_name": "Vision Mode",
                "short_desc": "Which vision LLM API to use",
                "desc": "The vision LLM backend:\n"
                    "- `openai` — OpenAI-compatible vision API (GPT-4o, etc.)\n"
                    "- `anthropic` — Anthropic Messages API with vision",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "openai",
                        "display_name": "OpenAI (GPT-4o)",
                        "desc": "Use an OpenAI-compatible vision endpoint",
                    },
                    <AllowedValueInfo>{
                        "value": "anthropic",
                        "display_name": "Anthropic (Claude)",
                        "desc": "Use the Anthropic Messages API with vision",
                    },
                ),
            },
            "vision_url": <DataProviderOptionInfo>{
                "display_name": "Vision URL",
                "short_desc": "Base URL of the vision LLM service",
                "desc": "Base URL of the vision LLM service (e.g., "
                    "`https://api.openai.com` or `https://api.anthropic.com`).",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "vision_model": <DataProviderOptionInfo>{
                "display_name": "Vision Model",
                "short_desc": "Model name for the vision LLM",
                "desc": "Model name (e.g., `gpt-4o`, `claude-sonnet-4-20250514`).",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "vision_api_key": <DataProviderOptionInfo>{
                "display_name": "Vision API Key",
                "short_desc": "API key for the vision LLM",
                "desc": "API key for authenticating with the vision LLM service.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
                "sensitive": True,
            },
        };
    }

    private {
        #! Image field name
        string image_field;

        #! MIME type field name (optional)
        *string mime_type_field;

        #! Extraction type
        string extraction_type;

        #! Vision LLM client
        QoreVisionExtraction::VisionLlmClient vision_client;

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
        mime_type_field = copts.mime_type_field;
        extraction_type = copts.extraction_type;
        if (!extraction_type.val()) {
            throw "CONSTRUCTOR-ERROR", "extraction_type option is required";
        }

        string vision_mode = copts.vision_mode;
        if (!vision_mode.val()) {
            throw "CONSTRUCTOR-ERROR", "vision_mode option is required";
        }
        if (!copts.vision_url.val()) {
            throw "CONSTRUCTOR-ERROR", "vision_url option is required";
        }
        if (!copts.vision_model.val()) {
            throw "CONSTRUCTOR-ERROR", "vision_model option is required";
        }

        vision_client = new QoreVisionExtraction::VisionLlmClient(
            <QoreVisionExtraction::VisionLlmConfig>{
                "mode": vision_mode,
                "url": copts.vision_url,
                "model": copts.vision_model,
                "api_key": copts.vision_api_key,
            });

        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "vision-extraction";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Analyzes images using a vision LLM — generates descriptions, extracts "
            "tables as markdown, or extracts text from screenshots and photos; useful "
            "when OCR alone cannot handle complex layouts or visual content";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record: extracts from image and enriches the record
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreVisionExtractionProcessor: not configured; provide constructor options";
        }

        hash<auto> result = {} + record;

        binary image_data = DataProvider::AbstractDataProvider::getFieldValue(
            record, image_field);

        string mime_type = "image/png";
        if (mime_type_field.val()) {
            mime_type = DataProvider::AbstractDataProvider::getFieldValue(
                record, mime_type_field).toString();
        }

        hash<QoreVisionExtraction::VisionExtractionResult> vr;
        switch (extraction_type) {
            case "describe":
                vr = vision_client.describeImage(image_data, mime_type);
                break;
            case "extract_table":
                vr = vision_client.extractTable(image_data, mime_type);
                break;
            case "extract_text":
                vr = vision_client.extractText(image_data, mime_type);
                break;
        }

        result.vision_text = vr.text;
        result.vision_format = vr.format;

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
