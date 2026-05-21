# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore VertexAiDataProvider request/response data types

/** VertexAiDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the VertexAiDataProvider module
public namespace VertexAiDataProvider {
#! Request data type for the Vertex AI GenerateContent action
public class VertexAiGenerateContentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "Gemini publisher model name",
                "type": StringType,
                "desc": "The Gemini publisher model name (e.g. "
                    "`gemini-2.0-flash`, `gemini-2.5-pro`, "
                    "`gemini-1.5-pro-002`); becomes the `{model}` segment "
                    "in the Vertex URL path",
                "required": True,
                "example_value": "gemini-2.0-flash",
            },
            "contents": {
                "display_name": "Contents",
                "short_desc": "Gemini conversation contents",
                "type": AutoListType,
                "desc": "Gemini conversation history; each entry is a hash with "
                    "`role` (`user` / `model`) and `parts` (list of "
                    "`{text: \"...\"}`, `{inlineData: {mimeType, data}}`, or "
                    "`{functionCall: ...}` / `{functionResponse: ...}` parts)",
                "required": True,
                "example_value": (
                    {"role": "user",
                     "parts": ({"text": "Summarize the Q3 results."},)},
                ),
            },
            "systemInstruction": {
                "display_name": "System Instruction",
                "short_desc": "Optional system-instruction block",
                "type": AutoHashOrNothingType,
                "desc": "Optional system-instruction block: "
                    "`{parts: ({text: \"...\"},)}`",
            },
            "generationConfig": {
                "display_name": "Generation Config",
                "short_desc": "Generation knobs",
                "type": AutoHashOrNothingType,
                "desc": "Generation parameters: "
                    "`temperature`, `topP`, `topK`, `maxOutputTokens`, "
                    "`responseMimeType`, `responseSchema`, "
                    "`thinkingConfig.thinkingBudget`",
                "example_value": {"maxOutputTokens": 1024, "temperature": 0.0},
            },
            "tools": {
                "display_name": "Tools",
                "short_desc": "Tool / function-calling definitions",
                "type": AutoListOrNothingType,
                "desc": "Function-calling tools: list of "
                    "`{functionDeclarations: ({name, description, parameters},)}`",
            },
            "toolConfig": {
                "display_name": "Tool Config",
                "short_desc": "Tool-selection policy",
                "type": AutoHashOrNothingType,
                "desc": "Tool selection policy: "
                    "`{functionCallingConfig: {mode: \"AUTO\"|\"ANY\"|\"NONE\"}}`",
            },
            "safetySettings": {
                "display_name": "Safety Settings",
                "short_desc": "Safety-filter thresholds",
                "type": AutoListOrNothingType,
                "desc": "Per-category safety-filter thresholds; list of "
                    "`{category: HARM_CATEGORY_*, threshold: BLOCK_*}`",
            },
            "cachedContent": {
                "display_name": "Cached Content",
                "short_desc": "Vertex context-caching reference",
                "type": StringOrNothingType,
                "desc": "Reference to a previously-created cached-content "
                    "resource (e.g. "
                    "`projects/{project}/locations/{region}/cachedContents/{id}`); "
                    "major cost reduction for repeated system prompts",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response data type for the Vertex AI GenerateContent action
public class VertexAiGenerateContentResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "candidates": {
                "display_name": "Candidates",
                "short_desc": "Candidate completions",
                "type": AutoListOrNothingType,
                "desc": "Candidate completions; each has `content: "
                    "{role, parts: ({text|functionCall},)}`, `finishReason`, "
                    "`safetyRatings`",
            },
            "promptFeedback": {
                "display_name": "Prompt Feedback",
                "short_desc": "Safety-filter results on the prompt",
                "type": AutoHashOrNothingType,
                "desc": "Safety-filter results on the prompt side",
            },
            "usageMetadata": {
                "display_name": "Usage Metadata",
                "short_desc": "Token usage counters",
                "type": AutoHashOrNothingType,
                "desc": "Token usage: `promptTokenCount`, "
                    "`candidatesTokenCount`, `totalTokenCount`, "
                    "`cachedContentTokenCount`, `thoughtsTokenCount`",
            },
            "modelVersion": {
                "display_name": "Model Version",
                "short_desc": "Resolved model version",
                "type": StringOrNothingType,
                "desc": "The actual model version that served the request",
            },
            "responseId": {
                "display_name": "Response ID",
                "short_desc": "Unique response identifier",
                "type": StringOrNothingType,
                "desc": "Unique response identifier",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request data type for the Vertex AI Embed (predict) action
public class VertexAiEmbedRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "Vertex AI embedding publisher model",
                "type": StringType,
                "desc": "Vertex AI embedding publisher model (e.g. "
                    "`text-embedding-005`, `textembedding-gecko@003`, "
                    "`text-multilingual-embedding-002`)",
                "required": True,
                "example_value": "text-embedding-005",
            },
            "instances": {
                "display_name": "Instances",
                "short_desc": "Texts to embed",
                "type": AutoListType,
                "desc": "List of `{content: \"<text>\"}` hashes; one instance "
                    "per text to embed (Vertex predict accepts batches)",
                "required": True,
                "example_value": (
                    {"content": "The quick brown fox jumps over the lazy dog."},
                ),
            },
            "parameters": {
                "display_name": "Parameters",
                "short_desc": "Model-specific parameters",
                "type": AutoHashOrNothingType,
                "desc": "Model-specific parameters: "
                    "`autoTruncate` (bool), `outputDimensionality` (int)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response data type for the Vertex AI Embed (predict) action
public class VertexAiEmbedResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "predictions": {
                "display_name": "Predictions",
                "short_desc": "Per-instance embedding predictions",
                "type": AutoListOrNothingType,
                "desc": "Per-instance embedding predictions; each has "
                    "`embeddings: {values: (...), statistics: {...}}`",
            },
            "metadata": {
                "display_name": "Metadata",
                "short_desc": "Per-call metadata",
                "type": AutoHashOrNothingType,
                "desc": "Per-call metadata such as billable character count",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instances
public const VertexAiGenerateContentRequestDataType  = new VertexAiGenerateContentRequestDataType();
public const VertexAiGenerateContentResponseDataType = new VertexAiGenerateContentResponseDataType();
public const VertexAiEmbedRequestDataType            = new VertexAiEmbedRequestDataType();
public const VertexAiEmbedResponseDataType           = new VertexAiEmbedResponseDataType();
}
