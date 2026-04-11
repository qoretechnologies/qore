# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreExtractionProcessor class definition

/** QoreExtractionProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that extracts structured data from text using an LLM
/** Uses an LLM to extract typed structured data from a text field according
    to a schema template (NER, invoice, contact, sentiment) or a custom schema.
    Each record is enriched with extraction results.

    @par Example:
    @code{.py}
QoreExtractionProcessor proc({
    "text_field": "body",
    "schema_template": "ner",
    "llm_mode": "http",
    "llm_url": "http://localhost:8080",
    "llm_model": "gpt-4o",
});
    @endcode

*/
public class QoreExtractionProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreExtractionProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "text_field": <DataProviderOptionInfo>{
                "display_name": "Text Field",
                "short_desc": "Which text field to extract from",
                "desc": "The name of the text field in each record to run extraction on.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "schema_template": <DataProviderOptionInfo>{
                "display_name": "Schema Template",
                "short_desc": "Built-in extraction schema to use",
                "desc": "A built-in schema template name. One of `schema_template` or "
                    "`custom_schema` must be provided.\n\n"
                    "Available templates:\n"
                    "- `ner` — Named Entity Recognition (entities with type, text, position, "
                    "confidence)\n"
                    "- `invoice` — Invoice data (number, date, vendor, line items, total)\n"
                    "- `contact` — Contact information (name, email, phone, company)\n"
                    "- `sentiment` — Sentiment analysis (overall sentiment, score, aspects)",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "ner",
                        "display_name": "Named Entity Recognition",
                        "desc": "Extract entities with type, text, character offsets, "
                            "and confidence",
                    },
                    <AllowedValueInfo>{
                        "value": "invoice",
                        "display_name": "Invoice Data",
                        "desc": "Extract invoice number, date, vendor, line items, totals",
                    },
                    <AllowedValueInfo>{
                        "value": "contact",
                        "display_name": "Contact Information",
                        "desc": "Extract name, email, phone, company, title, address",
                    },
                    <AllowedValueInfo>{
                        "value": "sentiment",
                        "display_name": "Sentiment Analysis",
                        "desc": "Extract overall sentiment, score, per-aspect breakdown",
                    },
                ),
            },
            "custom_schema": <DataProviderOptionInfo>{
                "display_name": "Custom Schema",
                "short_desc": "Custom extraction schema as a hash",
                "desc": "A custom extraction schema hash with keys: `name` (string), "
                    "`description` (string), `fields` (list of field definitions). Each "
                    "field has: `name`, `type`, `description`, `required` (bool).\n\n"
                    "Use this instead of `schema_template` for custom extraction tasks.",
                "type": AbstractDataProviderTypeMap."hash",
                "required": False,
            },
            "llm_mode": <DataProviderOptionInfo>{
                "display_name": "LLM Mode",
                "short_desc": "How to connect to the LLM",
                "desc": "The LLM backend mode:\n"
                    "- `http` — OpenAI-compatible endpoint (SGLang, vLLM, OpenAI)\n"
                    "- `anthropic` — Anthropic Messages API\n"
                    "- `qorus` — Qorus AI Gateway",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "http",
                        "display_name": "HTTP (OpenAI-compatible)",
                        "desc": "Connect to an OpenAI-compatible chat completions endpoint",
                    },
                    <AllowedValueInfo>{
                        "value": "anthropic",
                        "display_name": "Anthropic",
                        "desc": "Connect to the Anthropic Messages API",
                    },
                    <AllowedValueInfo>{
                        "value": "qorus",
                        "display_name": "Qorus AI Gateway",
                        "desc": "Route through the Qorus AI Gateway",
                    },
                ),
            },
            "llm_url": <DataProviderOptionInfo>{
                "display_name": "LLM URL",
                "short_desc": "Base URL of the LLM service",
                "desc": "Base URL of the LLM service. Required for `http` and `anthropic` "
                    "modes (e.g., `http://localhost:8080` or `https://api.anthropic.com`).",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
            },
            "llm_model": <DataProviderOptionInfo>{
                "display_name": "LLM Model",
                "short_desc": "Model name for the LLM",
                "desc": "Model name to pass in the LLM request (e.g., `gpt-4o`, "
                    "`claude-sonnet-4-20250514`).",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
            },
            "llm_api_key": <DataProviderOptionInfo>{
                "display_name": "LLM API Key",
                "short_desc": "API key for LLM authentication",
                "desc": "API key for authenticating with the LLM service.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
                "sensitive": True,
            },
            "temperature": <DataProviderOptionInfo>{
                "display_name": "Temperature",
                "short_desc": "Sampling temperature for extraction",
                "desc": "LLM sampling temperature. Use 0.0 for deterministic extraction "
                    "(recommended). Higher values increase randomness.",
                "type": AbstractDataProviderTypeMap."float",
                "required": False,
                "default_value": 0.0,
            },
            "max_retries": <DataProviderOptionInfo>{
                "display_name": "Max Retries",
                "short_desc": "Maximum retry attempts on validation failure",
                "desc": "Maximum number of retry attempts when extraction output fails "
                    "schema validation. Default is 2.",
                "type": AbstractDataProviderTypeMap."int",
                "required": False,
                "default_value": 2,
            },
        };
    }

    private {
        #! Configured text field name
        string text_field;

        #! The extraction pipeline
        QoreStructuredExtraction::ExtractionPipeline pipeline;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        text_field = copts.text_field;
        if (!text_field.val()) {
            throw "CONSTRUCTOR-ERROR", "text_field option is required";
        }

        # Build the extraction schema
        hash<QoreStructuredExtraction::ExtractionSchemaInfo> schema;
        if (copts.schema_template.val()) {
            schema = QoreStructuredExtraction::ExtractionSchema::getTemplate(
                copts.schema_template);
        } else if (copts.custom_schema) {
            schema = buildCustomSchema(copts.custom_schema);
        } else {
            throw "CONSTRUCTOR-ERROR",
                "one of schema_template or custom_schema must be provided";
        }

        # Build the LLM backend
        string llm_mode = copts.llm_mode;
        if (!llm_mode.val()) {
            throw "CONSTRUCTOR-ERROR", "llm_mode option is required";
        }

        hash<QoreLlmUtils::LlmBackendConfig> llm_config(
            {"mode": llm_mode});
        if (copts.llm_url.val()) {
            llm_config.url = copts.llm_url;
        }
        if (copts.llm_model.val()) {
            llm_config.model = copts.llm_model;
        }
        if (copts.llm_api_key.val()) {
            llm_config.api_key = copts.llm_api_key;
        }
        if (exists copts.temperature) {
            llm_config.temperature = copts.temperature.toFloat();
        }

        QoreLlmUtils::LlmBackend llm(llm_config);

        # Build the pipeline
        *hash<QoreStructuredExtraction::ExtractionPipelineOptions> pipe_opts;
        if (exists copts.max_retries) {
            pipe_opts = <QoreStructuredExtraction::ExtractionPipelineOptions>{
                "max_retries": copts.max_retries.toInt(),
            };
        }
        pipeline = new QoreStructuredExtraction::ExtractionPipeline(llm, schema, pipe_opts);

        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "extract-structured";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Extracts structured data from text using an LLM — for example, pulling names "
            "and addresses from emails, line items from invoices, or sentiment from reviews; "
            "adds extracted fields to each record";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record: extracts structured data and enriches the record
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreExtractionProcessor: not configured; provide constructor options";
        }

        hash<auto> result = {} + record;

        string text =
            DataProvider::AbstractDataProvider::getFieldValue(record, text_field).toString();

        hash<QoreStructuredExtraction::ExtractionResult> extraction =
            pipeline.extractData(text);

        result._extracted = extraction.extracted;
        result._extraction_valid = extraction.valid;
        result._extraction_usage = extraction.usage;
        if (extraction.validation_errors) {
            result._extraction_errors = extraction.validation_errors;
        }

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

    #! Returns NOTHING — output type is dynamic (input record enriched with extraction fields)
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return NOTHING;
    }

    #! Builds a custom ExtractionSchemaInfo from a user-provided hash
    private hash<QoreStructuredExtraction::ExtractionSchemaInfo> buildCustomSchema(
            hash<auto> schema_hash) {
        if (!schema_hash.name.val()) {
            throw "CONSTRUCTOR-ERROR", "custom_schema must have a 'name' key";
        }
        if (!schema_hash.description.val()) {
            throw "CONSTRUCTOR-ERROR", "custom_schema must have a 'description' key";
        }
        if (!schema_hash.fields || schema_hash.fields.typeCode() != NT_LIST) {
            throw "CONSTRUCTOR-ERROR",
                "custom_schema must have a 'fields' key with a list of field definitions";
        }

        list<hash<QoreStructuredExtraction::ExtractionFieldDef>> fields = ();
        foreach hash<auto> field_hash in (schema_hash.fields) {
            if (!field_hash.name.val() || !field_hash.type.val()
                    || !field_hash.description.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "each field in custom_schema.fields must have 'name', 'type', "
                    "and 'description' keys";
            }
            push fields, <QoreStructuredExtraction::ExtractionFieldDef>{
                "name": field_hash.name,
                "type": field_hash.type,
                "description": field_hash.description,
                "required": field_hash.required ?? True,
            };
        }

        return <QoreStructuredExtraction::ExtractionSchemaInfo>{
            "name": schema_hash.name,
            "description": schema_hash.description,
            "fields": fields,
        };
    }
}
}
