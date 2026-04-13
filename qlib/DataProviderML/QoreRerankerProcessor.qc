# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRerankerProcessor class definition

/** QoreRerankerProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that re-ranks accumulated records by query relevance
/** Accumulates records, then re-ranks the text field against a query using
    a cross-encoder model and emits the top-k records with relevance scores.

    This is a @ref DPC_AGGREGATE processor because it needs to see all
    candidate records before it can produce ranked output.

    @par Example:
    @code{.py}
QoreRerankerProcessor proc({
    "query_field": "search_query",
    "passage_field": "chunk_text",
    "top_k": 5,
    "reranker_mode": "http",
    "reranker_url": "http://localhost:8080",
});
    @endcode

*/
public class QoreRerankerProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreRerankerProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "query_field": <DataProviderOptionInfo>{
                "display_name": "Query Field",
                "short_desc": "Field containing the search query",
                "desc": "The name of the field in each record containing the search query "
                    "to rank against. All records in a batch should have the same query.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "passage_field": <DataProviderOptionInfo>{
                "display_name": "Passage Field",
                "short_desc": "Field containing the text to rank",
                "desc": "The name of the field in each record containing the text passage "
                    "to score against the query.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "top_k": <DataProviderOptionInfo>{
                "display_name": "Top K",
                "short_desc": "Number of top results to return",
                "desc": "The number of highest-scoring records to emit after re-ranking. "
                    "Records below this threshold are dropped.",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "default_value": 5,
            },
            "reranker_mode": <DataProviderOptionInfo>{
                "display_name": "Reranker Mode",
                "short_desc": "How to connect to the reranker",
                "desc": "The reranker backend mode:\n"
                    "- `http` — Call an HTTP reranker endpoint\n"
                    "- `onnx` — Run a local ONNX cross-encoder model",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "http",
                        "display_name": "HTTP Endpoint",
                        "desc": "Call an HTTP reranker API endpoint",
                    },
                    <AllowedValueInfo>{
                        "value": "onnx",
                        "display_name": "Local ONNX Model",
                        "desc": "Run a local ONNX cross-encoder model",
                    },
                ),
            },
            "reranker_url": <DataProviderOptionInfo>{
                "display_name": "Reranker URL",
                "short_desc": "URL of the HTTP reranker endpoint",
                "desc": "URL of the reranker endpoint. Required for `http` mode.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
            },
            "reranker_api_key": <DataProviderOptionInfo>{
                "display_name": "Reranker API Key",
                "short_desc": "API key for reranker authentication",
                "desc": "Optional API key for authenticating with the reranker endpoint.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
                "sensitive": True,
            },
            "reranker_model_path": <DataProviderOptionInfo>{
                "display_name": "ONNX Model Path",
                "short_desc": "Path to the ONNX cross-encoder model",
                "desc": "Filesystem path to the ONNX cross-encoder model file. "
                    "Required for `onnx` mode.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
            },
            "tokenizer_config_path": <DataProviderOptionInfo>{
                "display_name": "Tokenizer Config Path",
                "short_desc": "Path to tokenizer.json for ONNX mode",
                "desc": "Filesystem path to a HuggingFace `tokenizer.json` file. "
                    "Required for `onnx` mode.",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
            },
            "max_length": <DataProviderOptionInfo>{
                "display_name": "Max Sequence Length",
                "short_desc": "Maximum tokens for query+passage pairs",
                "desc": "Maximum sequence length for query+passage tokenization. "
                    "Longer pairs are truncated. Default is 512.",
                "type": AbstractDataProviderTypeMap."int",
                "required": False,
                "default_value": 512,
            },
        };
    }

    private {
        #! Query field name
        string query_field;

        #! Passage field name
        string passage_field;

        #! Number of top results to return
        int top_k;

        #! The reranker instance
        QoreTokenizerUtils::CrossEncoderReranker reranker;

        #! Accumulated records
        list<auto> buffer = ();

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        query_field = copts.query_field;
        passage_field = copts.passage_field;
        top_k = copts.top_k ?? 5;

        if (!query_field.val()) {
            throw "CONSTRUCTOR-ERROR", "query_field option is required";
        }
        if (!passage_field.val()) {
            throw "CONSTRUCTOR-ERROR", "passage_field option is required";
        }

        string reranker_mode = copts.reranker_mode;
        if (!reranker_mode.val()) {
            throw "CONSTRUCTOR-ERROR", "reranker_mode option is required";
        }

        hash<QoreTokenizerUtils::RerankerConfig> rconfig(<QoreTokenizerUtils::RerankerConfig>{
            "mode": reranker_mode,
            "max_length": copts.max_length ?? 512,
        });

        if (reranker_mode == "http") {
            if (!copts.reranker_url.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "reranker_url is required for HTTP mode";
            }
            rconfig.url = copts.reranker_url;
            rconfig.api_key = copts.reranker_api_key;
        } else if (reranker_mode == "onnx") {
            if (!copts.reranker_model_path.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "reranker_model_path is required for ONNX mode";
            }
            if (!copts.tokenizer_config_path.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "tokenizer_config_path is required for ONNX mode";
            }
            rconfig.model_path = copts.reranker_model_path;
            rconfig.tokenizer_config =
                parse_json(ReadOnlyFile::readTextFile(copts.tokenizer_config_path));
        }

        reranker = new QoreTokenizerUtils::CrossEncoderReranker(rconfig);
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "rerank-text";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Re-ranks accumulated text records by relevance to a search query using a "
            "cross-encoder model — improves search precision by scoring each candidate "
            "against the query; emits the top-k most relevant records with scores";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records for re-ranking
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreRerankerProcessor: not configured; provide constructor options";
        }
        push buffer, record;
        return;
    }

    #! Flushes accumulated records: re-ranks and emits top-k
    private *list<auto> flushRecordsImpl() {
        if (!configured || !buffer) {
            return;
        }

        # Extract query (use first record's query) and passages
        string query = DataProvider::AbstractDataProvider::getFieldValue(
            buffer[0], query_field).toString();
        list<string> passages = ();
        foreach auto record in (buffer) {
            push passages, DataProvider::AbstractDataProvider::getFieldValue(
                record, passage_field).toString();
        }

        # Score all passages
        list<float> scores = reranker.score(query, passages);

        # Create scored pairs and sort by score descending
        list<hash<auto>> scored = ();
        for (int i = 0; i < scores.size(); ++i) {
            push scored, {"index": i, "score": scores[i]};
        }
        scored = sort(scored, int sub(hash<auto> a, hash<auto> b) {
            return b.score <=> a.score;
        });

        # Emit top-k records enriched with reranker_score and reranker_rank
        int limit = min(top_k, scored.size());
        list<auto> results = ();
        for (int i = 0; i < limit; ++i) {
            hash<auto> result = {} + buffer[scored[i]."index"];
            result.reranker_score = scored[i].score;
            result.reranker_rank = i + 1;
            push results, result;
        }

        buffer = ();
        return results;
    }

    #! Does not support bulk API
    private bool supportsBulkApiImpl() {
        return False;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_AGGREGATE;
    }

    #! Returns NOTHING — output type is dynamic
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return NOTHING;
    }
}
}
