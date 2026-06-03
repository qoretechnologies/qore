# -*- mode: qore; indent-tabs-mode: nil -*-

/*  GroundingRule.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see GroundingRule
public namespace QoreGuardrails {

#! Output guardrail enforcing source grounding and citation policy
/** Wraps @ref QoreLlmUtils::GroundedAnswerVerifier as a guardrail rule.  The
    caller supplies authorized source snippets in the evaluation context under
    the configured key.  No source authorization is performed by this generic
    module.

    @since QoreGuardrails 1.2
*/
public class GroundingRule inherits AbstractGuardrailRule {
    private {
        #! Key in the context hash holding raw sources or source refs
        string sources_ctx_key;
        #! Require sources to be present
        bool require_sources;
        #! Require answer citations
        bool require_citations;
        #! Minimum grounding score to pass
        float min_grounding_score;
        #! Minimum citation coverage to pass
        float min_citation_coverage;
        #! LLM backend for verification
        *QoreLlmUtils::LlmBackend llm;
    }

    #! Creates a grounding rule
    /** @param severity violation severity level
        @param params optional parameters:
            - \c sources_key (string): evaluation context key for sources, default "sources"
            - \c sources_ctx_key (string): compatibility alias for \c sources_key
            - \c require_sources (bool): fail when no sources are present
            - \c require_citations (bool): require citations in the answer
            - \c min_grounding_score (float): default 0.7
            - \c min_citation_coverage (float): default 0.0
        @param llm_config optional LlmBackendConfig for the verifier
    */
    constructor(string severity = GS_HIGH, *hash<auto> params,
            *hash<auto> llm_config) : AbstractGuardrailRule(severity) {
        hash<auto> p = params ?? {};
        sources_ctx_key = p.sources_key ?? p.sources_ctx_key ?? "sources";
        require_sources = p.require_sources ?? False;
        require_citations = p.require_citations ?? False;
        min_grounding_score = p.min_grounding_score ?? 0.7;
        min_citation_coverage = p.min_citation_coverage ?? 0.0;

        if (llm_config) {
            llm = new QoreLlmUtils::LlmBackend(
                cast<hash<QoreLlmUtils::LlmBackendConfig>>(llm_config));
        }
    }

    string getRuleName() {
        return "grounding";
    }

    #! Grounding is an output-side concern by default
    string getDirection() {
        return direction_override ?? GD_OUTPUT;
    }

    #! Returns the DataProvider-option schema for this rule's \c params hash
    /** @since QoreGuardrails 1.2 */
    public static hash<string, hash<DataProviderOptionInfo>> getConfigSchema() {
        return {
            "sources_key": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(StringType),),
                "display_name": "Sources Key",
                "short_desc": "Context hash key containing source snippets",
                "desc": "Name of the `evaluate()` context key containing "
                    "authorized source snippets or source-reference hashes.",
                "default_value": "sources",
                "required": False,
            },
            "require_sources": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(BoolType),),
                "display_name": "Require Sources",
                "short_desc": "Fail when no authorized sources are supplied",
                "desc": "When enabled, the rule blocks output if the caller "
                    "does not supply any sources under `sources_key`.",
                "default_value": False,
                "required": False,
                "preselected": True,
            },
            "require_citations": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(BoolType),),
                "display_name": "Require Citations",
                "short_desc": "Require answer citations to supplied source ids",
                "desc": "When enabled, the answer must cite at least one "
                    "supplied source id with a supported citation marker.",
                "default_value": False,
                "required": False,
            },
            "min_grounding_score": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(FloatType),),
                "display_name": "Minimum Grounding Score",
                "short_desc": "Minimum verifier grounding score required to pass",
                "desc": "The LLM verifier returns a score from 0.0 to 1.0. "
                    "Responses below this threshold are blocked.",
                "default_value": 0.7,
                "required": False,
                "preselected": True,
            },
            "min_citation_coverage": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(FloatType),),
                "display_name": "Minimum Citation Coverage",
                "short_desc": "Minimum deterministic citation coverage required",
                "desc": "Coverage is 1.0 when the configured citation "
                    "requirement is satisfied and 0.0 when it is not. "
                    "Future callers may provide required source ids for a "
                    "more granular score.",
                "default_value": 0.0,
                "required": False,
            },
        };
    }

    hash<GuardrailResult> evaluate(string text, *hash<auto> ctx) {
        date start = now_us();
        list<hash<QoreLlmUtils::LlmSourceRef>> sources =
            extractSources(ctx);
        if (!sources && require_sources) {
            hash<GuardrailResult> result = makeFailResult(
                "no authorized sources supplied for grounding check",
                NOTHING, {"reason_code": "source-required"});
            result.duration_ms = getElapsedMs(start);
            return result;
        }
        if (!sources) {
            hash<GuardrailResult> result = makePassResult();
            result.duration_ms = getElapsedMs(start);
            return result;
        }

        if (!llm) {
            if (getFailClosed()) {
                hash<GuardrailResult> result = makeFailResult(
                    "grounding verifier unavailable without LLM",
                    NOTHING, {"reason_code": "verifier-unavailable"});
                result.duration_ms = getElapsedMs(start);
                return result;
            }
            hash<GuardrailResult> result = makePassResult();
            result.duration_ms = getElapsedMs(start);
            return result;
        }

        try {
            QoreLlmUtils::GroundedAnswerVerifier verifier(llm, {
                "require_sources": require_sources,
                "require_citations": require_citations,
                "min_grounding_score": min_grounding_score,
                "min_citation_coverage": min_citation_coverage,
            });
            hash<QoreLlmUtils::GroundingVerificationResult> checked =
                verifier.verify(text, sources);
            if (!checked.passed) {
                hash<GuardrailResult> result = makeFailResult(
                    sprintf("response is not sufficiently grounded "
                        "(score %.0f%%, citation coverage %.0f%%): %s",
                        checked.grounding_score * 100,
                        checked.citation_coverage * 100,
                        checked.reason ?? "unsupported claims detected"),
                    NOTHING, makeMetadata(checked));
                result.duration_ms = getElapsedMs(start);
                return result;
            }
        } catch (hash<ExceptionInfo> ex) {
            if (getFailClosed()) {
                hash<GuardrailResult> result = makeFailResult(
                    sprintf("grounding verifier unavailable: %s", ex.desc),
                    NOTHING, {"reason_code": "verifier-error",
                        "error_type": ex.err});
                result.duration_ms = getElapsedMs(start);
                return result;
            }
        }

        hash<GuardrailResult> result = makePassResult();
        result.duration_ms = getElapsedMs(start);
        return result;
    }

    private list<hash<QoreLlmUtils::LlmSourceRef>> extractSources(
            *hash<auto> ctx) {
        if (!ctx || !exists ctx{sources_ctx_key}) {
            return ();
        }
        auto raw = ctx{sources_ctx_key};
        if (raw.typeCode() != NT_LIST) {
            return ();
        }
        return QoreLlmUtils::GroundedAnswerVerifier::buildSourceBundle(
            cast<list>(raw));
    }

    private static hash<auto> makeMetadata(
            hash<QoreLlmUtils::GroundingVerificationResult> checked) {
        return {
            "grounding_score": checked.grounding_score,
            "citation_coverage": checked.citation_coverage,
            "unsupported_claims": checked.unsupported_claims,
            "supported_claims": checked.supported_claims,
            "missing_citations": checked.missing_citations,
            "invalid_citations": checked.invalid_citations,
            "requires_regeneration": checked.requires_regeneration,
            "reason": checked.reason,
            "usage": checked.usage,
        };
    }
}

} # namespace QoreGuardrails
