# -*- mode: qore; indent-tabs-mode: nil -*-

/*  LlmGrounding.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see GroundedAnswerVerifier
public namespace QoreLlmUtils {

#! Source snippet supplied to grounding and citation checks
public hashdecl LlmSourceRef {
    #! Stable source id used in answer citations
    string source_id;
    #! Human-readable title
    string title;
    #! Optional source URI
    *string uri;
    #! Optional visibility/scope label supplied by the caller
    *string scope;
    #! Optional content hash for external content-reference workflows
    *string content_hash;
    #! Source snippet visible to the verifier
    string snippet;
    #! Optional caller metadata; must not contain secrets
    *hash<auto> metadata;
}

#! Result from deterministic citation validation
public hashdecl CitationValidationResult {
    #! True when citation requirements pass
    bool passed;
    #! Source ids cited by the answer and present in the supplied source bundle
    list<string> cited_source_ids;
    #! Citation-like markers that do not match a supplied source id
    list<string> invalid_citations;
    #! Required citation ids that were not found
    list<string> missing_citations;
    #! Coarse citation coverage score from 0.0 to 1.0
    float citation_coverage = 0.0;
}

#! Result from LLM-backed grounding verification
public hashdecl GroundingVerificationResult {
    #! True when the answer satisfies grounding and citation policy
    bool passed;
    #! LLM-assessed grounding score from 0.0 to 1.0
    float grounding_score = 0.0;
    #! LLM-extracted supported factual claims
    list<string> supported_claims;
    #! LLM-extracted unsupported factual claims
    list<string> unsupported_claims;
    #! Missing citation ids or citation requirement failures
    list<string> missing_citations;
    #! Invalid citation markers
    list<string> invalid_citations;
    #! Coarse citation coverage score from 0.0 to 1.0
    float citation_coverage = 0.0;
    #! True when a caller should retry/regenerate before refusing
    bool requires_regeneration = False;
    #! Short human-readable explanation
    *string reason;
    #! Raw parsed verifier payload
    hash<auto> raw = {};
    #! Provider usage returned by the verifier LLM call
    hash<auto> usage = {};
}

#! Deterministic helper for citation marker validation
/** The validator intentionally does not know any product-specific citation
    syntax.  It accepts a citation when the answer contains a supplied
    \c source_id either directly in square brackets (for example
    \c [source-1]) or in a typed marker (for example \c [source:source-1],
    \c [doc:source-1], or \c [artifact:source-1]).

    @since QoreLlmUtils 1.1
*/
public class CitationValidator {
    #! Validates citations in an answer against a supplied source bundle
    /** @param answer answer text to validate
        @param sources authorized source references
        @param opts optional settings:
            - \c require_citations (bool, default false)
            - \c required_source_ids (list<string>)

        @return citation validation result
    */
    public static hash<CitationValidationResult> validate(string answer,
            list<hash<LlmSourceRef>> sources, *hash<auto> opts = NOTHING) {
        hash<auto> options = opts ?? {};
        hash<string, bool> allowed;
        foreach hash<LlmSourceRef> source in (sources) {
            allowed{source.source_id} = True;
        }

        list<string> cited = ();
        foreach string source_id in (keys allowed) {
            if (containsCitation(answer, source_id)) {
                cited += source_id;
            }
        }

        list<string> required = normalizeStringList(
            options.required_source_ids);
        list<string> missing = ();
        foreach string source_id in (required) {
            if (!inlist(source_id, cited)) {
                missing += source_id;
            }
        }

        bool require_citations = options.require_citations ?? False;
        if (require_citations && sources && !cited && !missing) {
            missing += "citation-required";
        }

        float coverage;
        if (required) {
            coverage = required.size() == 0 ? 1.0
                : ((required.size() - missing.size()).toFloat()
                    / required.size().toFloat());
        } else if (require_citations && sources) {
            coverage = cited ? 1.0 : 0.0;
        } else {
            coverage = 1.0;
        }

        return <CitationValidationResult>{
            "passed": !missing,
            "cited_source_ids": cited,
            "invalid_citations": (),
            "missing_citations": missing,
            "citation_coverage": coverage,
        };
    }

    private static bool containsCitation(string answer, string source_id) {
        if (!source_id.val()) {
            return False;
        }
        foreach string marker in ((
                "[" + source_id + "]",
                "[source:" + source_id + "]",
                "[doc:" + source_id + "]",
                "[artifact:" + source_id + "]",
                "[tool:" + source_id + "]")) {
            if (answer.find(marker) >= 0) {
                return True;
            }
        }
        return False;
    }

    private static list<string> normalizeStringList(auto value) {
        list<string> rv = ();
        if (value.typeCode() != NT_LIST) {
            return rv;
        }
        foreach auto item in (cast<list<auto>>(value)) {
            if (item.typeCode() == NT_NOTHING || item.typeCode() == NT_NULL) {
                continue;
            }
            string s = trim(item.toString());
            if (s.val()) {
                rv += s;
            }
        }
        return rv;
    }
}

#! Language-independent factual grounding verifier
/** Verifies that an answer is supported by an authorized source bundle.  The
    LLM is asked to judge claim support semantically in the answer's language,
    while @ref CitationValidator performs deterministic citation-id checks.

    The verifier is generic: callers decide which sources are authoritative,
    which users may see them, and whether lack of sources should fail closed.

    @since QoreLlmUtils 1.1
*/
public class GroundedAnswerVerifier {
    private {
        #! Backend used for the verifier round-trip
        LlmBackend backend;
        #! Default verifier options
        hash<auto> options;
    }

    #! Creates a verifier with default options
    /** @param backend LLM backend used for grounding verification
        @param options optional defaults; recognized keys are
            \c min_grounding_score, \c min_citation_coverage,
            \c require_sources, and \c require_citations
    */
    constructor(LlmBackend backend, *hash<auto> options = NOTHING) {
        self.backend = backend;
        self.options = options ?? {};
    }

    #! Builds a normalized source bundle from strings or source hashes
    /** @param raw_sources list of strings or hashes.  Hash inputs may use
        \c source_id or \c id, \c snippet or \c content, and optional
        \c title, \c uri, \c scope, \c content_hash, and \c metadata.

        @return normalized source bundle
    */
    public static list<hash<LlmSourceRef>> buildSourceBundle(
            list<auto> raw_sources) {
        list<hash<LlmSourceRef>> rv = ();
        int idx = 1;
        foreach auto raw in (raw_sources) {
            string source_id = sprintf("source-%d", idx);
            string title = source_id;
            string snippet;
            *string uri;
            *string scope;
            *string content_hash;
            *hash<auto> metadata;

            if (raw.typeCode() == NT_HASH) {
                hash<auto> h = cast<hash<auto>>(raw);
                source_id = normalizeString(h.source_id ?? h.id, source_id);
                title = normalizeString(h.title, source_id);
                snippet = normalizeString(h.snippet ?? h.content ?? h.text, "");
                uri = optionalString(h.uri);
                scope = optionalString(h.scope);
                content_hash = optionalString(h.content_hash);
                if (h.metadata.typeCode() == NT_HASH) {
                    metadata = cast<hash<auto>>(h.metadata);
                }
            } else if (raw.typeCode() != NT_NOTHING
                    && raw.typeCode() != NT_NULL) {
                snippet = raw.toString();
            }

            if (!snippet.val()) {
                ++idx;
                continue;
            }

            hash<LlmSourceRef> ref = <LlmSourceRef>{
                "source_id": source_id,
                "title": title,
                "snippet": snippet,
            };
            if (uri.val()) {
                ref.uri = uri;
            }
            if (scope.val()) {
                ref.scope = scope;
            }
            if (content_hash.val()) {
                ref.content_hash = content_hash;
            }
            if (metadata) {
                ref.metadata = metadata;
            }
            rv += ref;
            ++idx;
        }
        return rv;
    }

    #! Verifies an answer against authorized source snippets
    /** @param answer answer text to verify
        @param sources authorized source bundle
        @param opts optional per-call settings overriding constructor options

        @return grounding verification result

        @throw LLM-GROUNDING-VERIFIER-ERROR if the LLM response cannot be
            parsed or validated
    */
    hash<GroundingVerificationResult> verify(string answer,
            list<hash<LlmSourceRef>> sources, *hash<auto> opts = NOTHING) {
        hash<auto> effective = options;
        if (opts) {
            effective += opts;
        }
        bool require_sources = effective.require_sources ?? False;
        bool require_citations = effective.require_citations ?? False;
        float min_grounding_score = clampScore(
            effective.min_grounding_score ?? 0.7);
        float min_citation_coverage = clampScore(
            effective.min_citation_coverage ?? 0.0);

        if (!sources) {
            return makeNoSourcesResult(require_sources);
        }

        hash<CitationValidationResult> citation =
            CitationValidator::validate(answer, sources, {
                "require_citations": require_citations,
                "required_source_ids": effective.required_source_ids,
            });

        hash<auto> request = {
            "answer": answer,
            "sources": sources,
            "policy": {
                "min_grounding_score": min_grounding_score,
                "require_citations": require_citations,
                "min_citation_coverage": min_citation_coverage,
            },
        };

        # Grounding verification is a compact structured judgement that does
        # not benefit from extended hidden reasoning; disabling the reasoning
        # budget keeps the (small) output budget available for the JSON
        # verdict.  This matters on "thinking" models (e.g. Gemini 2.5) where
        # reasoning tokens are drawn from the same output budget and can
        # otherwise truncate the response to empty/unparseable JSON.
        # Providers without a reasoning budget ignore the option.
        hash<LlmCompletionResult> llm_result = backend.complete(
            getSystemPrompt(), make_json(request, JGF_ADD_FORMATTING),
            getOutputSchema(),
            <LlmRequestOptions>{"reasoning_token_budget": 0});
        hash<LlmStructuredRepairResult> repaired =
            LlmStructuredRepair::parseJsonObject(llm_result.content);
        if (!repaired.valid) {
            throw "LLM-GROUNDING-VERIFIER-ERROR",
                sprintf("grounding verifier response was not a JSON object: %s",
                    repaired.error_detail ?? repaired.error_code);
        }
        hash<auto> parsed = cast<hash<auto>>(repaired.value);

        hash<GroundingVerificationResult> rv =
            normalizeVerification(parsed, citation, min_grounding_score,
                min_citation_coverage);
        rv.raw = parsed;
        rv.usage = llm_result.usage;
        return rv;
    }

    #! Returns the JSON schema requested from structured-output capable backends
    /** A bare JSON Schema object with no top-level \c name (or other dialect
        metadata): such labelling keywords belong on the request wrapper, and
        embedding one makes providers with a stricter OpenAPI Schema subset
        (notably Google Gemini's \c responseSchema) reject the request. */
    static hash<auto> getOutputSchema() {
        return {
            "type": "object",
            "properties": {
                "grounded": {"type": "boolean"},
                "grounding_score": {"type": "number"},
                "supported_claims": {
                    "type": "array",
                    "items": {"type": "string"},
                },
                "unsupported_claims": {
                    "type": "array",
                    "items": {"type": "string"},
                },
                "missing_citations": {
                    "type": "array",
                    "items": {"type": "string"},
                },
                "reason": {"type": "string"},
            },
            "required": ("grounded", "grounding_score",
                "unsupported_claims", "reason"),
        };
    }

    private static string getSystemPrompt() {
        return "You are a factual grounding verifier. Verify the answer "
            "against the supplied source snippets, regardless of the answer "
            "language. Do not use outside knowledge. A claim is supported "
            "only when it is directly stated by the supplied sources or is a "
            "minor paraphrase of them. Identify unsupported factual claims. "
            "If the answer contains no factual claims, mark it grounded. "
            "Return only the requested JSON object.";
    }

    private static hash<GroundingVerificationResult> makeNoSourcesResult(
            bool require_sources) {
        list<string> unsupported = require_sources
            ? ("no authorized sources were supplied",) : ();
        return <GroundingVerificationResult>{
            "passed": !require_sources,
            "grounding_score": require_sources ? 0.0 : 1.0,
            "supported_claims": (),
            "unsupported_claims": unsupported,
            "missing_citations": require_sources
                ? ("source-required",) : (),
            "invalid_citations": (),
            "citation_coverage": require_sources ? 0.0 : 1.0,
            "requires_regeneration": require_sources,
            "reason": require_sources
                ? "authorized sources are required but none were supplied"
                : "no sources supplied and source-required mode is disabled",
        };
    }

    private static hash<GroundingVerificationResult> normalizeVerification(
            hash<auto> parsed, hash<CitationValidationResult> citation,
            float min_grounding_score, float min_citation_coverage) {
        float grounding_score = clampScore(parsed.grounding_score);
        list<string> unsupported =
            normalizeStringList(parsed.unsupported_claims);
        list<string> supported =
            normalizeStringList(parsed.supported_claims);
        list<string> missing = citation.missing_citations
            + normalizeStringList(parsed.missing_citations);
        float citation_coverage = citation.citation_coverage;
        bool pass = parsed.grounded.toBool()
            && grounding_score >= min_grounding_score
            && citation_coverage >= min_citation_coverage
            && !unsupported
            && !missing
            && !citation.invalid_citations;

        return <GroundingVerificationResult>{
            "passed": pass,
            "grounding_score": grounding_score,
            "supported_claims": supported,
            "unsupported_claims": unsupported,
            "missing_citations": missing,
            "invalid_citations": citation.invalid_citations,
            "citation_coverage": citation_coverage,
            "requires_regeneration": !pass,
            "reason": normalizeString(parsed.reason, ""),
        };
    }

    private static list<string> normalizeStringList(auto value) {
        list<string> rv = ();
        if (value.typeCode() != NT_LIST) {
            return rv;
        }
        foreach auto item in (cast<list<auto>>(value)) {
            if (item.typeCode() == NT_NOTHING || item.typeCode() == NT_NULL) {
                continue;
            }
            string s = trim(item.toString());
            if (s.val()) {
                rv += s;
            }
        }
        return rv;
    }

    private static string normalizeString(auto value, string default_value) {
        if (value.typeCode() == NT_NOTHING || value.typeCode() == NT_NULL) {
            return default_value;
        }
        string s = trim(value.toString());
        return s.val() ? s : default_value;
    }

    private static *string optionalString(auto value) {
        if (value.typeCode() == NT_NOTHING || value.typeCode() == NT_NULL) {
            return NOTHING;
        }
        string s = trim(value.toString());
        return s.val() ? s : NOTHING;
    }

    private static float clampScore(auto value) {
        float score = 0.0;
        if (value.typeCode() != NT_NOTHING && value.typeCode() != NT_NULL) {
            score = value.toFloat();
        }
        if (score < 0.0) {
            return 0.0;
        }
        if (score > 1.0) {
            return 1.0;
        }
        return score;
    }
}

} # namespace QoreLlmUtils
