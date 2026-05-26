# -*- mode: qore; indent-tabs-mode: nil -*-

/*  OnnxProcessingSpecs.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see QoreOnnxTools
public namespace QoreOnnxTools {

/** Builds and normalizes standard ONNX preprocessing and postprocessing
    contract metadata for package manifests and registry execution plans.

    @since QoreOnnxTools 1.1
*/
public class OnnxProcessingSpecs {
    /** Builds standard preprocessing and postprocessing contracts for an ONNX
        model task.

        @param task normalized ONNX task such as \c "raw", \c "regression",
            \c "embedding", \c "reranker", or \c "sequence-classification"
        @param inputs ONNX input tensor metadata
        @param outputs ONNX output tensor metadata
        @param ctx package context such as tokenizer/config artifact names

        @return a hash with \c preprocess and \c postprocess contracts
    */
    static hash<auto> buildStandardContracts(string task, *list<hash<auto>> inputs,
            *list<hash<auto>> outputs, *hash<auto> ctx) {
        string normalized_task = normalizeTask(task);
        return {
            "preprocess": buildStandardPreprocess(normalized_task, inputs, outputs, ctx),
            "postprocess": buildStandardPostprocess(normalized_task, inputs, outputs, ctx),
        };
    }

    /** Builds a standard preprocessing contract.

        @param task normalized ONNX task such as \c "raw", \c "regression",
            \c "embedding", \c "reranker", or \c "sequence-classification"
        @param inputs ONNX input tensor metadata
        @param outputs ONNX output tensor metadata
        @param ctx package context such as tokenizer/config artifact names

        @return preprocessing contract metadata
    */
    static hash<auto> buildStandardPreprocess(string task, *list<hash<auto>> inputs,
            *list<hash<auto>> outputs, *hash<auto> ctx) {
        string normalized_task = normalizeTask(task);
        string input_mode = inferInputMode(normalized_task, inputs, ctx);
        hash<auto> rv = {
            "schema_version": 1,
            "kind": "onnx-preprocess",
            "task": normalized_task,
            "input_mode": input_mode,
            "tensor_inputs": tensorNames(inputs),
            "missing_values": "reject",
        };

        hash<auto> dtypes = tensorFieldMap(inputs, "type");
        if (dtypes) {
            rv.input_dtypes = dtypes;
        }
        hash<auto> shapes = tensorFieldMap(inputs, "shape");
        if (shapes) {
            rv.input_shapes = shapes;
        }

        if (input_mode == "feature-matrix") {
            rv.input_layout = "row-major";
        } else if (input_mode == "tokenized-text") {
            rv.tokenizer = buildTokenizerPreprocess(ctx, inputs);
        }

        return rv;
    }

    /** Builds a standard postprocessing contract.

        @param task normalized ONNX task such as \c "raw", \c "regression",
            \c "embedding", \c "reranker", or \c "sequence-classification"
        @param inputs ONNX input tensor metadata
        @param outputs ONNX output tensor metadata
        @param ctx package context such as tokenizer/config artifact names

        @return postprocessing contract metadata
    */
    static hash<auto> buildStandardPostprocess(string task, *list<hash<auto>> inputs,
            *list<hash<auto>> outputs, *hash<auto> ctx) {
        string normalized_task = normalizeTask(task);
        string primary_output = firstTensorName(outputs) ?? "output";
        hash<auto> rv = {
            "schema_version": 1,
            "kind": "onnx-postprocess",
            "task": normalized_task,
            "primary_output": primary_output,
            "tensor_outputs": tensorNames(outputs),
        };

        switch (normalized_task) {
            case "regression":
                rv.output_mode = "scalar";
                rv.output_name = primary_output;
                rv.output_field = primary_output;
                rv.value_output = primary_output;
                break;
            case "classification":
            case "sequence-classification": {
                string label_output = findOutput(outputs, ("label", "labels", "class", "classes"))
                    ?? primary_output;
                *string scores_output = findOutput(outputs,
                    ("probabilities", "probability", "scores", "score", "logits"));
                rv.output_mode = "classification";
                rv.label_output = label_output;
                if (scores_output.val()) {
                    rv.scores_output = scores_output;
                    rv.activation = scores_output == "logits" ? "softmax" : "none";
                }
                break;
            }
            case "embedding": {
                string embedding_output = findOutput(outputs,
                    ("sentence_embedding", "embeddings", "embedding", "last_hidden_state", "output"))
                    ?? primary_output;
                rv.output_mode = "embedding";
                rv.embedding_output = embedding_output;
                rv.pooling = isRank(outputs, embedding_output, 3) ? "mean" : "none";
                rv.normalize = False;
                break;
            }
            case "reranker": {
                string score_output = findOutput(outputs, ("score", "scores", "logits", "output"))
                    ?? primary_output;
                rv.output_mode = "score";
                rv.score_output = score_output;
                rv.activation = score_output == "logits" ? "sigmoid" : "none";
                break;
            }
            case "causal-lm":
                rv.output_mode = "token-generation";
                rv.logits_output = findOutput(outputs, ("logits",)) ?? primary_output;
                rv.kv_cache = "auto";
                break;
            case "token-classification":
                rv.output_mode = "token-classification";
                rv.logits_output = findOutput(outputs, ("logits", "scores", "probabilities"))
                    ?? primary_output;
                rv.activation = rv.logits_output == "logits" ? "softmax" : "none";
                break;
            case "encoder-decoder":
                rv.output_mode = "sequence";
                rv.logits_output = findOutput(outputs, ("logits",)) ?? primary_output;
                break;
            default:
                rv.output_mode = "raw";
                rv.output_names = tensorNames(outputs);
                break;
        }

        return rv;
    }

    /** Normalizes a caller-supplied preprocessing contract by adding the
        standard metadata fields that are safe to infer.

        @param contract optional caller-supplied preprocessing contract
        @param task normalized ONNX task such as \c "raw", \c "regression",
            \c "embedding", \c "reranker", or \c "sequence-classification"
        @param inputs ONNX input tensor metadata
        @param outputs ONNX output tensor metadata
        @param ctx package context such as tokenizer/config artifact names

        @return normalized preprocessing contract
    */
    static hash<auto> normalizePreprocess(*hash<auto> contract, string task,
            *list<hash<auto>> inputs, *list<hash<auto>> outputs, *hash<auto> ctx) {
        hash<auto> standard = buildStandardPreprocess(task, inputs, outputs, ctx);
        if (!contract) {
            return standard;
        }
        hash<auto> rv = standard + contract;
        if (!rv.schema_version) {
            rv.schema_version = 1;
        }
        if (!rv.kind.val()) {
            rv.kind = "onnx-preprocess";
        }
        if (!rv.task.val()) {
            rv.task = normalizeTask(task);
        }
        return rv;
    }

    /** Normalizes a caller-supplied postprocessing contract by adding the
        standard metadata fields that are safe to infer.

        @param contract optional caller-supplied postprocessing contract
        @param task normalized ONNX task such as \c "raw", \c "regression",
            \c "embedding", \c "reranker", or \c "sequence-classification"
        @param inputs ONNX input tensor metadata
        @param outputs ONNX output tensor metadata
        @param ctx package context such as tokenizer/config artifact names

        @return normalized postprocessing contract
    */
    static hash<auto> normalizePostprocess(*hash<auto> contract, string task,
            *list<hash<auto>> inputs, *list<hash<auto>> outputs, *hash<auto> ctx) {
        hash<auto> standard = buildStandardPostprocess(task, inputs, outputs, ctx);
        if (!contract) {
            return standard;
        }
        hash<auto> rv = standard + contract;
        if (!rv.schema_version) {
            rv.schema_version = 1;
        }
        if (!rv.kind.val()) {
            rv.kind = "onnx-postprocess";
        }
        if (!rv.task.val()) {
            rv.task = normalizeTask(task);
        }
        return rv;
    }

    private static string normalizeTask(string task) {
        string t = task.val() ? task.lwr() : "raw";
        t =~ s/^onnx[\/:]//;
        t =~ s/_/-/g;
        switch (t) {
            case "classification":
            case "regression":
            case "embedding":
            case "reranker":
            case "token-classification":
            case "sequence-classification":
            case "encoder-decoder":
            case "causal-lm":
            case "raw":
                return t;
            case "text-classification":
                return "sequence-classification";
            case "feature-extraction":
            case "sentence-embedding":
            case "sentence-transformer":
                return "embedding";
            case "ranking":
            case "cross-encoder":
                return "reranker";
            case "text-generation":
            case "causal-language-modeling":
                return "causal-lm";
            case "seq2seq":
                return "encoder-decoder";
        }
        return t;
    }

    private static string inferInputMode(string task, *list<hash<auto>> inputs,
            *hash<auto> ctx) {
        switch (task) {
            case "regression":
            case "classification":
                return "feature-matrix";
            case "sequence-classification":
            case "embedding":
            case "reranker":
            case "token-classification":
            case "encoder-decoder":
            case "causal-lm":
                return hasTensor(inputs, "input_ids") || hasTokenizer(ctx)
                    ? "tokenized-text" : "tensor-hash";
        }
        if (hasTensor(inputs, "input_ids")) {
            return "tokenized-text";
        }
        return "tensor-hash";
    }

    private static hash<auto> buildTokenizerPreprocess(*hash<auto> ctx,
            *list<hash<auto>> inputs) {
        hash<auto> rv = {
            "required": True,
            "padding": "batch",
            "truncation": True,
        };
        if (hasTensor(inputs, "input_ids")) {
            rv.input_ids = "input_ids";
        }
        if (hasTensor(inputs, "attention_mask")) {
            rv.attention_mask = "attention_mask";
        }
        if (hasTensor(inputs, "token_type_ids")) {
            rv.token_type_ids = "token_type_ids";
        }
        if (ctx && ctx.tokenizer) {
            hash<auto> tokenizer = cast<hash<auto>>(ctx.tokenizer);
            if (tokenizer.artifact_name.val()) {
                rv.artifact_name = tokenizer.artifact_name;
            }
            if (tokenizer.config_artifact_name.val()) {
                rv.config_artifact_name = tokenizer.config_artifact_name;
            }
            if (tokenizer.model_max_length) {
                rv.max_length = tokenizer.model_max_length;
            }
        }
        return rv;
    }

    private static bool hasTokenizer(*hash<auto> ctx) {
        return ctx && ctx.tokenizer;
    }

    private static bool hasTensor(*list<hash<auto>> tensors, string name) {
        if (!tensors) {
            return False;
        }
        foreach hash<auto> tensor in (tensors) {
            if (tensor.name == name) {
                return True;
            }
        }
        return False;
    }

    private static list<string> tensorNames(*list<hash<auto>> tensors) {
        list<string> rv();
        if (!tensors) {
            return rv;
        }
        foreach hash<auto> tensor in (tensors) {
            if (tensor.name.val()) {
                push rv, string(tensor.name);
            }
        }
        return rv;
    }

    private static hash<auto> tensorFieldMap(*list<hash<auto>> tensors, string field) {
        hash<auto> rv = {};
        if (!tensors) {
            return rv;
        }
        foreach hash<auto> tensor in (tensors) {
            if (tensor.name.val() && tensor.hasKey(field)) {
                rv{string(tensor.name)} = tensor{field};
            }
        }
        return rv;
    }

    private static *string firstTensorName(*list<hash<auto>> tensors) {
        if (!tensors) {
            return NOTHING;
        }
        foreach hash<auto> tensor in (tensors) {
            if (tensor.name.val()) {
                return string(tensor.name);
            }
        }
        return NOTHING;
    }

    private static *string findOutput(*list<hash<auto>> outputs, list<string> candidates) {
        if (!outputs) {
            return NOTHING;
        }
        foreach string candidate in (candidates) {
            foreach hash<auto> output in (outputs) {
                if (output.name == candidate) {
                    return candidate;
                }
            }
        }
        foreach string candidate in (candidates) {
            foreach hash<auto> output in (outputs) {
                if (output.name.val() && string(output.name).lwr() == candidate) {
                    return string(output.name);
                }
            }
        }
        return NOTHING;
    }

    private static bool isRank(*list<hash<auto>> outputs, string name, int rank) {
        if (!outputs) {
            return False;
        }
        foreach hash<auto> output in (outputs) {
            if (output.name == name && output.shape && output.shape.typeCode() == NT_LIST) {
                return output.shape.size() == rank;
            }
        }
        return False;
    }
}

} # namespace QoreOnnxTools
