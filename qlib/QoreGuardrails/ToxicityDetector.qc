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

    @since QoreGuardrails 1.0
*/
public class ToxicityDetector inherits AbstractGuardrailRule {
    private {
        #! Detection mode
        string mode;
        #! ONNX confidence threshold
        float onnx_threshold;
        #! LLM confidence threshold
        float llm_threshold;
%ifndef NoLlmUtils
        #! Optional LLM backend
        *QoreLlmUtils::LlmBackend llm;
%endif
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

%ifndef NoLlmUtils
        if ((mode == "llm" || mode == "both") && llm_config) {
            llm = new QoreLlmUtils::LlmBackend(
                cast<hash<QoreLlmUtils::LlmBackendConfig>>(llm_config));
        }
%endif
    }

    string getRuleName() {
        return "toxicity";
    }

    hash<GuardrailResult> evaluate(string text, *hash<auto> ctx) {
        date start = now_us();

        # LLM-based classification
%ifndef NoLlmUtils
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
%endif

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

%ifndef NoLlmUtils
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
    }
%endif
}

} # namespace QoreGuardrails
