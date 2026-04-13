# -*- mode: qore; indent-tabs-mode: nil -*-

/*  HallucinationDetector.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see HallucinationDetector
public namespace QoreGuardrails {

#! LLM-based hallucination detector that cross-references output against RAG sources
/** Checks whether an AI-generated response is factually grounded in the
    source documents that were used to generate it. This prevents the AI from
    making up facts or citing information not present in the retrieved sources.

    The detector reads source documents from the evaluation context hash
    (key configurable, default \c "sources") and sends them along with the
    response to an LLM for grounding verification.

    Common use cases:
    - Verifying RAG pipeline outputs before showing them to users
    - Compliance: ensuring AI responses are traceable to approved source material
    - Quality control: catching "confident but wrong" AI answers

    @par Example: Check output against RAG sources
    @code{.py}
HallucinationDetector detector("high", {"min_grounding_score": 0.6});
hash<GuardrailResult> result = detector.evaluate(
    "The company revenue was $5M in Q1 2026.",
    {"sources": ("Q1 2026 revenue report: total revenue $5.2M...",)},
);
if (!result.passed) {
    printf("Hallucination detected: %y\n", result.violations);
}
    @endcode

    @since QoreGuardrails 1.0
*/
public class HallucinationDetector inherits AbstractGuardrailRule {
    private {
%ifndef NoLlmUtils
        #! LLM backend for grounding classification
        *QoreLlmUtils::LlmBackend llm;
%endif
        #! Key in the context hash holding the list of source texts
        string sources_ctx_key;
        #! Minimum grounding score to pass (0.0-1.0)
        float min_grounding_score;
    }

    #! Creates a HallucinationDetector
    /** @param severity violation severity level
        @param params optional parameters:
            - \c sources_ctx_key (string): key in context hash for source texts
              (default \c "sources")
            - \c min_grounding_score (float): minimum grounding score to pass
              (default 0.5)
        @param llm_config optional LlmBackendConfig for the grounding LLM
    */
    constructor(string severity = GS_HIGH, *hash<auto> params,
            *hash<auto> llm_config) : AbstractGuardrailRule(severity) {
        sources_ctx_key = params.sources_ctx_key ?? "sources";
        min_grounding_score = params.min_grounding_score ?? 0.5;

%ifndef NoLlmUtils
        if (llm_config) {
            llm = new QoreLlmUtils::LlmBackend(
                cast<hash<QoreLlmUtils::LlmBackendConfig>>(llm_config));
        }
%endif
    }

    string getRuleName() {
        return "hallucination";
    }

    #! Hallucination detection is an output-side concern by default
    string getDirection() {
        return direction_override ?? GD_OUTPUT;
    }

    hash<GuardrailResult> evaluate(string text, *hash<auto> ctx) {
        date start = now_us();

        # Get sources from context
        *list<string> sources;
        if (ctx) {
            sources = ctx{sources_ctx_key};
        }

        if (!sources) {
            # No sources to check against
            if (getFailClosed()) {
                hash<GuardrailResult> result = makeFailResult(
                    "no RAG sources provided for hallucination check (fail-closed)");
                result.duration_ms = getElapsedMs(start);
                return result;
            }
            hash<GuardrailResult> result = makePassResult();
            result.duration_ms = getElapsedMs(start);
            return result;
        }

%ifndef NoLlmUtils
        if (llm) {
            try {
                return checkGrounding(text, sources, start);
            } catch (hash<ExceptionInfo> ex) {
                if (getFailClosed()) {
                    hash<GuardrailResult> result = makeFailResult(
                        sprintf("hallucination check unavailable (fail-closed): %s",
                            ex.desc));
                    result.duration_ms = getElapsedMs(start);
                    return result;
                }
            }
        }
%endif

        if (getFailClosed()) {
            hash<GuardrailResult> result = makeFailResult(
                "LLM unavailable for hallucination check (fail-closed)");
            result.duration_ms = getElapsedMs(start);
            return result;
        }
        hash<GuardrailResult> result = makePassResult();
        result.duration_ms = getElapsedMs(start);
        return result;
    }

%ifndef NoLlmUtils
    #! Uses LLM to check if the response is grounded in sources
    private hash<GuardrailResult> checkGrounding(string text, list<string> sources,
            date start) {
        # Build sources summary
        string sources_text = "";
        int idx = 1;
        foreach string src in (sources) {
            sources_text += sprintf("Source %d:\n%s\n\n", idx, src);
            ++idx;
        }

        string system_prompt =
            "You are a fact-checking assistant. Given source documents and an "
            "AI-generated response, determine if the response is factually "
            "grounded in the sources.\n\n"
            "For each factual claim in the response, check if it is supported "
            "by the sources. A response is grounded if all its factual claims "
            "can be traced to the source material.";

        string user_prompt = sprintf(
            "Sources:\n%s\nResponse to check:\n%s\n\n"
            "Respond with JSON: {\"grounded\": true/false, "
            "\"grounding_score\": 0.0-1.0, \"unsupported_claims\": "
            "[\"claim text\", ...], \"reason\": \"explanation\"}",
            sources_text, text);

        hash<auto> output_schema = {
            "type": "object",
            "name": "grounding_check",
            "properties": {
                "grounded": {"type": "boolean"},
                "grounding_score": {"type": "number"},
                "unsupported_claims": {
                    "type": "array",
                    "items": {"type": "string"},
                },
                "reason": {"type": "string"},
            },
            "required": ("grounded", "grounding_score", "reason"),
        };

        hash<QoreLlmUtils::LlmCompletionResult> llm_result =
            llm.complete(system_prompt, user_prompt, output_schema);
        hash<auto> parsed = parse_json(llm_result.content);

        if (parsed.grounding_score < min_grounding_score) {
            list<string> claims = parsed.unsupported_claims ?? ();
            hash<GuardrailResult> result = makeFailResult(
                sprintf("response may contain hallucinated content "
                    "(grounding score: %.0f%%, threshold: %.0f%%): %s",
                    parsed.grounding_score * 100,
                    min_grounding_score * 100,
                    parsed.reason),
                NOTHING,
                {
                    "grounding_score": parsed.grounding_score,
                    "unsupported_claims": claims,
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
