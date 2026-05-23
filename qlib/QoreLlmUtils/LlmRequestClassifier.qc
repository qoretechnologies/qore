# -*- mode: qore; indent-tabs-mode: nil -*-

/*  LlmRequestClassifier.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see LlmRequestClassifier
public namespace QoreLlmUtils {

#! Structured semantic request classification returned by @ref LlmRequestClassifier
public hashdecl LlmRequestClassification {
    #! True when the request is allowed by the supplied policy
    bool allowed;
    #! Detected language code or human-readable language name; "und" if unknown
    string detected_language = "und";
    #! Semantic domain chosen by the classifier, for example "product-support"
    string domain = "unknown";
    #! Semantic task category chosen by the classifier
    string task_category = "unknown";
    #! Classifier confidence from 0.0 to 1.0
    float confidence = 0.0;
    #! Block category when @ref allowed is false
    *string blocked_category;
    #! Stable policy reason code
    *string reason_code;
    #! Short human-readable explanation
    *string reason;
    #! Raw parsed classifier payload for callers that need extra fields
    hash<auto> raw = {};
    #! Provider usage returned by the classifier LLM call
    hash<auto> usage = {};
}

#! Language-independent semantic request classifier
/** This helper class gives applications a reusable way to classify user
    requests against an application-supplied policy without relying on
    English keyword lists.  The policy is opaque to the module: callers
    provide allowed domains, allowed tasks, blocked task categories, and
    examples; the classifier asks the configured LLM backend to classify the
    request semantically and returns a normalized result.

    The implementation deliberately avoids product-specific policy.  Products
    such as Qorus should define their own allowed domains and blocked
    categories in the caller, then use this class through @ref QoreGuardrails
    or directly in their request pipeline.

    @par Example: classify a support request
    @code{.py}
%modern
%requires QoreLlmUtils

LlmBackend backend(<LlmBackendConfig>{"mode": "callback",
    "api_callback": my_callback});
LlmRequestClassifier classifier(backend, {
    "policy_label": "support-bot",
    "allowed_domains": ("billing", "technical support"),
    "blocked_task_categories": ("creative-writing", "general-world-knowledge"),
});
hash<LlmRequestClassification> r = classifier.classify(
    "Can you check why my integration failed?");
if (!r.allowed) {
    printf("blocked: %s\n", r.blocked_category);
}
    @endcode

    @since QoreLlmUtils 1.1
*/
public class LlmRequestClassifier {
    private {
        #! Backend used for the classification round-trip
        LlmBackend backend;
        #! Application-supplied policy hash
        hash<auto> policy;
    }

    #! Creates a classifier using the supplied backend and policy
    /** @param backend LLM backend used for semantic classification
        @param policy application-supplied policy hash; recommended keys are
            \c policy_label, \c allowed_domains, \c allowed_tasks,
            \c blocked_task_categories, \c allow_smalltalk, and
            \c examples
    */
    constructor(LlmBackend backend, *hash<auto> policy = NOTHING) {
        self.backend = backend;
        self.policy = policy ?? {};
    }

    #! Classifies a user message against the configured policy
    /** @param user_message user-supplied text to classify
        @param ctx optional per-request context such as attachment metadata
            or conversation summary; raw large content should not be supplied
            unless the caller has already applied its own privacy policy

        @return normalized semantic classification

        @throw LLM-REQUEST-CLASSIFIER-ERROR if the LLM response cannot be
            parsed or validated
    */
    hash<LlmRequestClassification> classify(string user_message,
            *hash<auto> ctx = NOTHING) {
        hash<auto> request = {
            "policy": policy,
            "request": user_message,
        };
        if (ctx) {
            request.context = ctx;
        }

        hash<LlmCompletionResult> llm_result = backend.complete(
            getSystemPrompt(), make_json(request, JGF_ADD_FORMATTING),
            getOutputSchema());
        hash<LlmStructuredRepairResult> repaired =
            LlmStructuredRepair::parseJsonObject(llm_result.content);
        if (!repaired.valid) {
            throw "LLM-REQUEST-CLASSIFIER-ERROR",
                sprintf("classifier response was not a JSON object: %s",
                    repaired.error_detail ?? repaired.error_code);
        }

        hash<auto> parsed = cast<hash<auto>>(repaired.value);
        hash<LlmRequestClassification> rv =
            normalizeClassification(parsed);
        rv.raw = parsed;
        rv.usage = llm_result.usage;
        return rv;
    }

    #! Returns the JSON schema requested from structured-output capable backends
    static hash<auto> getOutputSchema() {
        return {
            "type": "object",
            "name": "request_classification",
            "properties": {
                "allowed": {"type": "boolean"},
                "detected_language": {"type": "string"},
                "domain": {"type": "string"},
                "task_category": {"type": "string"},
                "confidence": {"type": "number"},
                "blocked_category": {"type": "string"},
                "reason_code": {"type": "string"},
                "reason": {"type": "string"},
            },
            "required": ("allowed", "detected_language", "domain",
                "task_category", "confidence", "reason_code", "reason"),
        };
    }

    private static string getSystemPrompt() {
        return "You are a semantic request-policy classifier. Classify the "
            "user request in whatever language it is written. Do not answer "
            "the request. Do not translate it. Do not rely on English "
            "keyword matching; classify by meaning, task, and domain. Use "
            "only the supplied policy to decide whether the request is "
            "allowed. A request is allowed only when it is within an allowed "
            "domain and task and is not in a blocked task category. If the "
            "policy is unclear or the request cannot be classified with "
            "confidence, set allowed=false with reason_code "
            "`classification-uncertain`. Return only the requested JSON "
            "object.";
    }

    private static hash<LlmRequestClassification> normalizeClassification(
            hash<auto> parsed) {
        hash<LlmRequestClassification> rv = <LlmRequestClassification>{
            "allowed": parsed.allowed.toBool(),
            "detected_language": normalizeString(parsed.detected_language,
                "und"),
            "domain": normalizeString(parsed.domain, "unknown"),
            "task_category": normalizeString(parsed.task_category, "unknown"),
            "confidence": clampConfidence(parsed.confidence),
            "reason_code": normalizeString(parsed.reason_code,
                parsed.allowed.toBool() ? "allowed" : "blocked"),
            "reason": normalizeString(parsed.reason, ""),
        };
        if (parsed.blocked_category.typeCode() != NT_NOTHING
                && parsed.blocked_category.typeCode() != NT_NULL
                && parsed.blocked_category.toString().val()) {
            rv.blocked_category = parsed.blocked_category.toString();
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

    private static float clampConfidence(auto value) {
        float confidence = 0.0;
        if (value.typeCode() != NT_NOTHING && value.typeCode() != NT_NULL) {
            confidence = value.toFloat();
        }
        if (confidence < 0.0) {
            return 0.0;
        }
        if (confidence > 1.0) {
            return 1.0;
        }
        return confidence;
    }
}

} # namespace QoreLlmUtils
