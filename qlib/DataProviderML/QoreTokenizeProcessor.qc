# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreTokenizeProcessor class definition

/** QoreTokenizeProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that enriches records with token count from a HuggingFace tokenizer
/** Loads a HuggingFace tokenizer configuration from a JSON file and uses
    @ref QoreTokenizerUtils::TokenCounter "TokenCounter" to count tokens in a
    specified text field. Each input record is enriched with a token count field.

    @par Example:
    @code{.py}
QoreTokenizeProcessor proc({
    "text_field": "body",
    "tokenizer_config_path": "/path/to/tokenizer.json",
    "add_special_tokens": True,
});
    @endcode

*/
public class QoreTokenizeProcessor inherits AbstractDataProviderMLProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreTokenizeProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "text_field": <DataProviderOptionInfo>{
                "display_name": "Text Field",
                "short_desc": "Which text field to tokenize",
                "desc": "The name of the text field in each record to tokenize. The token count "
                    "is stored in a new field named `{text_field}_token_count`.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "tokenizer_config_path": <DataProviderOptionInfo>{
                "display_name": "Tokenizer Config Path",
                "short_desc": "Path to HuggingFace tokenizer.json file",
                "desc": "The filesystem path to a HuggingFace `tokenizer.json` configuration "
                    "file. This file defines the vocabulary and tokenization rules used to "
                    "count tokens accurately for a specific model. Provide either this option "
                    "or `tokenizer_model_name` to load the tokenizer from QoreModelRegistry.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "tokenizer_model_name": <DataProviderOptionInfo>{
                "display_name": "Tokenizer Model Name",
                "short_desc": "Registry model containing tokenizer.json",
                "desc": "The QoreModelRegistry model/package name that contains a "
                    "`tokenizer.json` artifact. Use this when tokenizer files are registered "
                    "with the model package instead of deployed separately on the filesystem.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "tokenizer_model_version": <DataProviderOptionInfo>{
                "display_name": "Tokenizer Model Version",
                "short_desc": "Registry model version or latest",
                "desc": "Optional model version ID. Omit or use `latest` to load the most "
                    "recent registered tokenizer package.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "tokenizer_artifact_name": <DataProviderOptionInfo>{
                "display_name": "Tokenizer Artifact Name",
                "short_desc": "Named tokenizer artifact in the registry package",
                "desc": "Optional named artifact to load. Defaults to the package manifest's "
                    "`tokenizer.json` artifact.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "add_special_tokens": <DataProviderOptionInfo>{
                "display_name": "Add Special Tokens",
                "short_desc": "Include special tokens like [CLS], [SEP]",
                "desc": "Whether to include special tokens (such as `[CLS]` and `[SEP]`) in "
                    "the token count. Set to `True` to match the count that a model would "
                    "actually see, or `False` for the raw content token count.",
                "type": AbstractDataProviderTypeMap."bool",
                "default_value": True,
            },
        } + AbstractDataProviderMLProcessor::ModelRegistryConstructorOptions;
    }

    private {
        #! Configured text field name
        string text_field;

        #! Path to the tokenizer config
        *string tokenizer_config_path;

        #! Registry model/package containing tokenizer.json
        *string tokenizer_model_name;

        #! Registry model version
        *string tokenizer_model_version;

        #! Registry tokenizer artifact name
        *string tokenizer_artifact_name;

        #! Registry backend options
        hash<auto> registry_options;

        #! Whether to add special tokens
        bool add_special_tokens;

        #! Lazy-initialized TokenCounter
        *QoreTokenizerUtils::TokenCounter token_counter;

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
        tokenizer_config_path = copts.tokenizer_config_path;
        tokenizer_model_name = copts.tokenizer_model_name;
        tokenizer_model_version = copts.tokenizer_model_version;
        tokenizer_artifact_name = copts.tokenizer_artifact_name;
        if (!tokenizer_config_path.val() && !tokenizer_model_name.val()) {
            throw "CONSTRUCTOR-ERROR",
                "tokenizer_config_path or tokenizer_model_name option is required";
        }
        registry_options = AbstractDataProviderMLProcessor::buildModelRegistryOptions(copts);
        add_special_tokens = copts.add_special_tokens ?? True;
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "tokenize";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Measures how many tokens a text field contains for a given AI model — useful "
            "for estimating API costs, checking whether text fits in a model's context "
            "window, or filtering out records that are too short or too long";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record: counts tokens and enriches the record
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreTokenizeProcessor: not configured; provide constructor options";
        }

        ensureTokenCounter();

        # start with the original record
        hash<auto> result = {} + record;

        string text =
            DataProvider::AbstractDataProvider::getFieldValue(record, text_field).toString();

        int count = token_counter.countTokens(text, add_special_tokens);
        result{text_field + "_token_count"} = count;

        return result;
    }

    #! Processes records in bulk hash-of-lists format without row materialization when possible
    private *hash<auto> processRecordsBulkImpl(hash<auto> records) {
        updatePendingOutputShape(DataProvider::DataProviderShape::HashOfLists);
        *auto result = processRecordsShapeImpl(records, DataProvider::DataProviderShape::HashOfLists);
        return exists result ? cast<hash>(result) : NOTHING;
    }

    #! Processes an explicitly negotiated pipeline shape without row materialization when possible
    private *auto processRecordsShapeImpl(auto records, DataProvider::DataProviderShape shape) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreTokenizeProcessor: not configured; provide constructor options";
        }

        updatePendingOutputShape(shape);
        if (shape == DataProvider::DataProviderShape::RowRecords) {
            return processRecordImpl(records);
        }

        if (text_field.find(".") >= 0) {
            return processShapeRowsFallback(records, shape);
        }

        int count = DataProvider::AbstractDataProcessor::shapeRecordCount(records, shape);
        if (!count) {
            return;
        }

        *list<string> texts = textColumnFromShape(records, shape, count);
        if (!exists texts) {
            return processShapeRowsFallback(records, shape);
        }

        ensureTokenCounter();
        list<int> token_counts;
        foreach string text in (texts) {
            push token_counts, token_counter.countTokens(text, add_special_tokens);
        }
        return addTokenCountColumn(records, shape, token_counts);
    }

    #! Uses the row-oriented implementation when a column fast path is not safe
    private *auto processShapeRowsFallback(auto records, DataProvider::DataProviderShape shape) {
        list<auto> rows = DataProvider::AbstractDataProcessor::convertShape(
            records, shape, DataProvider::DataProviderShape::RowRecords);
        *list<auto> results;
        foreach auto row in (rows) {
            *auto row_result = processRecordImpl(row);
            if (exists row_result) {
                if (row_result.typeCode() == NT_LIST) {
                    results += row_result;
                } else {
                    push results, row_result;
                }
            }
        }
        if (!results) {
            return;
        }
        hash<auto> bulk = DataProvider::AbstractDataProcessor::recordsToHashList(results);
        return AbstractDataProviderMLProcessor::convertBulkOutputShape(bulk, shape);
    }

    #! Reads the configured text field directly from a shaped column block
    private *list<string> textColumnFromShape(auto records, DataProvider::DataProviderShape shape, int count) {
        *auto column;
        switch (shape) {
            case DataProvider::DataProviderShape::DataFrameBlock:
                if (records.columnNames().contains(text_field)) {
                    column = records.getColumn(text_field);
                }
                break;
            case DataProvider::DataProviderShape::BufferColumns:
                if (DataProvider::AbstractDataProcessor::isColumnarResult(records)) {
                    if (records.columnNames().contains(text_field)) {
                        column = records.getColumn(text_field);
                    }
                } else if (records.hasKey(text_field)) {
                    column = records{text_field};
                }
                break;
            case DataProvider::DataProviderShape::HashOfLists:
                if (records.hasKey(text_field)) {
                    column = records{text_field};
                }
                break;
            case DataProvider::DataProviderShape::RowRecords:
                break;
        }
        if (!exists column) {
            return;
        }
        return QoreTokenizeProcessor::columnToStrings(column, count, text_field);
    }

    #! Converts a shaped text column to string values using row-mode value semantics
    private static list<string> columnToStrings(auto column, int count, string field) {
        list<auto> values = column.typeCode() == NT_BUFFER ? column.toList() : column;
        if (values.lsize() != count) {
            throw "PROCESSOR-SHAPE-ERROR",
                sprintf("text field %y has %d values but the shaped block has %d rows",
                    field, values.lsize(), count);
        }

        list<string> texts;
        foreach auto value in (values) {
            push texts, value.toString();
        }
        return texts;
    }

    #! Appends token counts to a shaped block while preserving the input shape
    private auto addTokenCountColumn(auto records, DataProvider::DataProviderShape shape,
            list<int> token_counts) {
        string output_field = text_field + "_token_count";
        switch (shape) {
            case DataProvider::DataProviderShape::DataFrameBlock:
                return records.withColumn(output_field, token_counts);
            case DataProvider::DataProviderShape::BufferColumns: {
                hash<auto> columns = DataProvider::AbstractDataProcessor::isColumnarResult(records)
                    ? records.toColumnHash()
                    : records + {};
                columns{output_field} = token_counts;
                return ColumnarResult::fromColumnHash(columns);
            }
            case DataProvider::DataProviderShape::HashOfLists: {
                hash<auto> columns = records + {};
                columns{output_field} = token_counts;
                return columns;
            }
            case DataProvider::DataProviderShape::RowRecords:
                break;
        }
        return records;
    }

    #! Ensures the TokenCounter is initialized (lazy creation to avoid file I/O in constructor)
    private ensureTokenCounter() {
        if (!token_counter) {
%ifndef NoJson
            hash<auto> tokenizer_config;
            *hash<auto> tokenizer_runtime_config;
            if (tokenizer_model_name.val()) {
                QoreModelRegistry::ModelRegistry registry(registry_options);
                hash<auto> tokenizer_package = registry.loadTokenizerPackageConfig(
                    tokenizer_model_name, tokenizer_model_version, tokenizer_artifact_name);
                tokenizer_config = tokenizer_package.tokenizer_json;
                tokenizer_runtime_config = tokenizer_package.tokenizer_config;
            } else {
                tokenizer_config = parse_json(ReadOnlyFile::readTextFile(tokenizer_config_path));
            }
            token_counter = new QoreTokenizerUtils::TokenCounter(tokenizer_config,
                tokenizer_runtime_config);
%else
            throw "JSON-MODULE-MISSING", "json module is required for tokenizer config parsing";
%endif
        }
    }

    #! Supports shaped bulk API
    private bool supportsBulkApiImpl() {
        return True;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_TRANSFORM;
    }

    #! Returns NOTHING — output type is dynamic (input record enriched with token count)
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return NOTHING;
    }
}
}
