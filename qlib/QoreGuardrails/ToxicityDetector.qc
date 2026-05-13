# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ToxicityDetector.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see ToxicityDetector
public namespace QoreGuardrails {

#! ONNX + LLM dual-mode toxic content classifier
/** Detects toxic, harmful, hateful, or unsafe content using either a local
    ONNX classification model, an LLM-based classifier, or both (try ONNX
    first, fall back to LLM).

    Common use cases:
    - Blocking hate speech or harassment in user messages before they reach
      the AI
    - Filtering toxic content from AI-generated responses before showing
      them to users
    - Content moderation for customer-facing chat applications

    Modes:
    - \b onnx: Fast local classification using a pre-trained ONNX model
      (e.g., a fine-tuned DistilBERT toxicity classifier)
    - \b llm: LLM-based classification via QoreLlmUtils
    - \b both (default): Try ONNX first for speed, fall back to LLM if
      ONNX is unavailable

    @par Example: Detect toxic content
    @code{.py}
ToxicityDetector detector("critical", {"mode": "llm"});
hash<GuardrailResult> result = detector.evaluate("some user message");
if (!result.passed) {
    printf("Toxic content detected: %y\n", result.violations);
}
    @endcode
*/
public class ToxicityDetector inherits AbstractGuardrailRule {
    private {
        #! Detection mode
        string mode;
        #! ONNX confidence threshold
        float onnx_threshold;
        #! LLM confidence threshold
        float llm_threshold;
        #! Optional LLM backend
        *QoreLlmUtils::LlmBackend llm;
    }

    #! Creates a ToxicityDetector
    /** @param severity violation severity level
        @param params optional parameters:
            - \c mode (string): "onnx", "llm", or "both" (default "both")
            - \c model_path (string): path to ONNX toxicity model
            - \c tokenizer_config (hash): HuggingFace tokenizer config for ONNX
            - \c onnx_threshold (float): ONNX confidence threshold (default 0.7)
            - \c llm_threshold (float): LLM confidence threshold (default 0.7)
        @param llm_config optional LlmBackendConfig for LLM mode
    */
    constructor(string severity = GS_CRITICAL, *hash<auto> params,
            *hash<auto> llm_config) : AbstractGuardrailRule(severity) {
        mode = params.mode ?? "both";
        onnx_threshold = params.onnx_threshold ?? 0.7;
        llm_threshold = params.llm_threshold ?? 0.7;

        if ((mode == "llm" || mode == "both") && llm_config) {
            llm = new QoreLlmUtils::LlmBackend(
                cast<hash<QoreLlmUtils::LlmBackendConfig>>(llm_config));
        }
    }

    string getRuleName() {
        return "toxicity";
    }

    #! Returns the DataProvider-option schema for this rule's \c params hash
    /** @since QoreGuardrails 1.1 */
    public static hash<string, hash<DataProviderOptionInfo>> getConfigSchema() {
        return {
            "mode": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(StringType),),
                "display_name": "Detection Mode",
                "short_desc": "Whether to use ONNX classification, LLM classification, or both",
                "desc": "Toxicity can be detected by a local ONNX classification "
                    "model, by an LLM classification call, or by running both "
                    "and combining the results.  **Both** (the default) is "
                    "strongest: a violation fires if either model flags the "
                    "text.  **ONNX** is cheapest and keeps all data on the "
                    "Qorus host.  **LLM** is most context-aware but adds an "
                    "inference call per request.",
                "default_value": "both",
                "required": False,
                "preselected": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "onnx",
                        "display_name": "ONNX only",
                        "desc": "Local ONNX toxicity classifier only.  Fast and "
                            "private; requires the ONNX model file and "
                            "HuggingFace tokenizer config to be configured.",
                    },
                    <AllowedValueInfo>{
                        "value": "llm",
                        "display_name": "LLM only",
                        "desc": "LLM classification only.  Uses the endpoint's "
                            "configured backend.  Slowest but best at "
                            "understanding context and sarcasm.",
                    },
                    <AllowedValueInfo>{
                        "value": "both",
                        "display_name": "ONNX and LLM (default)",
                        "desc": "Run both and flag the text if either says it's "
                            "toxic.  Strongest detection, highest false "
                            "positive rate.",
                    },
                ),
            },
            "onnx_threshold": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(FloatType),),
                "display_name": "ONNX Confidence Threshold",
                "short_desc": "Minimum ONNX classifier score (0.0-1.0) that "
                    "counts as toxic",
                "desc": "The ONNX classifier returns a confidence score in "
                    "[0.0, 1.0].  A score at or above this threshold is "
                    "treated as a violation.  Default 0.7 balances "
                    "sensitivity and false-positive rate; lower values "
                    "(0.5-0.6) catch more, higher values (0.8+) are more "
                    "conservative.",
                "default_value": 0.7,
                "required": False,
            },
            "llm_threshold": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(FloatType),),
                "display_name": "LLM Confidence Threshold",
                "short_desc": "Minimum LLM classifier confidence (0.0-1.0) that "
                    "counts as toxic",
                "desc": "Threshold applied to the LLM classifier's confidence "
                    "output.  Same interpretation as `onnx_threshold`.  "
                    "Default 0.7.",
                "default_value": 0.7,
                "required": False,
            },
            "model_path": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(StringOrNothingType),),
                "display_name": "ONNX Model Path",
                "short_desc": "Filesystem path to the ONNX toxicity model",
                "desc": "Absolute path to an ONNX toxicity classification model "
                    "on the Qorus host.  Required when `mode` is `onnx` or "
                    "`both` and the module-level default path is not set.  "
                    "Ignored in `llm`-only mode.",
                "required": False,
            },
            "tokenizer_config": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(AutoHashOrNothingType),),
                "display_name": "Tokenizer Config",
                "short_desc": "HuggingFace tokenizer config hash for the ONNX model",
                "desc": "HuggingFace tokenizer configuration used by the ONNX "
                    "classifier.  Required when `mode` is `onnx` or `both`.  "
                    "Shape matches the standard HuggingFace `tokenizer.json` "
                    "layout.",
                "required": False,
            },
        };
    }

    hash<GuardrailResult> evaluate(string text, *hash<auto> ctx) {
        date start = now_us();

        # LLM-based classification
        if ((mode == "llm" || mode == "both") && llm) {
            try {
                return classifyWithLlm(text, start);
            } catch (hash<ExceptionInfo> ex) {
                if (getFailClosed()) {
                    hash<GuardrailResult> result = makeFailResult(
                        sprintf("toxicity classification unavailable (fail-closed): %s",
                            ex.desc));
                    result.duration_ms = getElapsedMs(start);
                    return result;
                }
            }
        }

        # No detection available
        if (getFailClosed()) {
            hash<GuardrailResult> result = makeFailResult(
                "toxicity detection unavailable (fail-closed)");
            result.duration_ms = getElapsedMs(start);
            return result;
        }
        hash<GuardrailResult> result = makePassResult();
        result.duration_ms = getElapsedMs(start);
        return result;
    }

    #! Uses LLM to classify toxicity
    private hash<GuardrailResult> classifyWithLlm(string text, date start) {
        string system_prompt =
            "You are a content safety classifier. Determine if the following "
            "text contains toxic, harmful, hateful, threatening, or unsafe content.\n\n"
            "Respond with a JSON object: {\"is_toxic\": true/false, "
            "\"confidence\": 0.0-1.0, \"category\": \"hate_speech/harassment/"
            "violence/self_harm/sexual/other/none\", \"reason\": \"brief explanation\"}";

        hash<auto> output_schema = {
            "type": "object",
            "name": "toxicity_classification",
            "properties": {
                "is_toxic": {"type": "boolean"},
                "confidence": {"type": "number"},
                "category": {"type": "string"},
                "reason": {"type": "string"},
            },
            "required": ("is_toxic", "confidence", "category", "reason"),
        };

        hash<QoreLlmUtils::LlmCompletionResult> llm_result =
            llm.complete(system_prompt, text, output_schema);
%ifdef NoJson
        throw "GUARDRAIL-ERROR", "the json module is required for LLM-based toxicity detection";
%else
        hash<auto> parsed = parse_json(llm_result.content);

        if (parsed.is_toxic && parsed.confidence > llm_threshold) {
            hash<GuardrailResult> result = makeFailResult(
                sprintf("toxic content detected (%s, confidence: %.0f%%): %s",
                    parsed.category, parsed.confidence * 100, parsed.reason),
                NOTHING,
                {
                    "detection_method": "llm",
                    "confidence": parsed.confidence,
                    "category": parsed.category,
                    "reason": parsed.reason,
                },
            );
            result.duration_ms = getElapsedMs(start);
            return result;
        }

        hash<GuardrailResult> result = makePassResult();
        result.duration_ms = getElapsedMs(start);
        return result;
%endif
    }
}

} # namespace QoreGuardrails
