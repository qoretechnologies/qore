# -*- mode: qore; indent-tabs-mode: nil -*-

/*  TaskPolicyRule.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see TaskPolicyRule
public namespace QoreGuardrails {

#! Semantic task/domain policy guardrail
/** Uses @ref QoreLlmUtils::LlmRequestClassifier to block requests outside an
    application-defined semantic domain or task policy.  This rule is
    intentionally language-independent: the classifier prompt tells the model
    to classify by meaning in whatever language the user used.

    @since QoreGuardrails 1.2
*/
public class TaskPolicyRule inherits AbstractGuardrailRule {
    private {
        #! Policy hash forwarded to LlmRequestClassifier
        hash<auto> policy;
        #! Minimum confidence required for an allowed decision
        float min_confidence;
        #! Whether low-confidence classifications should fail the rule
        bool fail_uncertain;
        #! LLM backend for semantic classification
        *QoreLlmUtils::LlmBackend llm;
    }

    #! Creates a task policy rule
    /** @param severity violation severity level
        @param params rule parameters:
            - \c policy_label (string)
            - \c allowed_domains (list<string>)
            - \c allowed_tasks (list<string>)
            - \c blocked_task_categories (list<string>)
            - \c examples (list<hash>)
            - \c min_confidence (float, default 0.65)
            - \c fail_uncertain (bool, default true)
        @param llm_config LlmBackendConfig for classification
    */
    constructor(string severity = GS_HIGH, *hash<auto> params,
            *hash<auto> llm_config) : AbstractGuardrailRule(severity) {
        hash<auto> p = params ?? {};
        min_confidence = p.min_confidence ?? 0.65;
        fail_uncertain = p.fail_uncertain ?? True;
        policy = {};
        foreach string key in (("policy_label", "allowed_domains",
                "allowed_tasks", "blocked_task_categories", "examples",
                "allow_smalltalk", "description")) {
            if (exists p{key}) {
                policy{key} = p{key};
            }
        }

        if (llm_config) {
            llm = new QoreLlmUtils::LlmBackend(
                cast<hash<QoreLlmUtils::LlmBackendConfig>>(llm_config));
        }
    }

    string getRuleName() {
        return "task_policy";
    }

    #! Task policy is an input-side concern by default
    string getDirection() {
        return direction_override ?? GD_INPUT;
    }

    #! Returns the DataProvider-option schema for this rule's \c params hash
    /** @since QoreGuardrails 1.2 */
    public static hash<string, hash<DataProviderOptionInfo>> getConfigSchema() {
        return {
            "policy_label": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(StringType),),
                "display_name": "Policy Label",
                "short_desc": "Short label for the semantic policy",
                "desc": "Human-readable label included in the classifier "
                    "policy.  The label is diagnostic only; the actual "
                    "decision comes from the allowed domains, allowed tasks, "
                    "and blocked task categories.",
                "required": False,
                "default_value": "task-policy",
            },
            "allowed_domains": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(AutoListType),),
                "display_name": "Allowed Domains",
                "short_desc": "Semantic domains this endpoint is allowed to handle",
                "desc": "List of allowed subject domains.  The classifier "
                    "matches by meaning in the user's language, not by "
                    "literal keyword occurrence.",
                "required": False,
                "example_value": ("product support", "operations"),
            },
            "allowed_tasks": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(AutoListType),),
                "display_name": "Allowed Tasks",
                "short_desc": "Semantic task categories this endpoint may perform",
                "desc": "List of task types that are in scope for the "
                    "endpoint, for example `troubleshooting`, `how-to`, "
                    "`status lookup`, or `configuration guidance`.",
                "required": False,
            },
            "blocked_task_categories": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(AutoListType),),
                "display_name": "Blocked Task Categories",
                "short_desc": "Semantic task categories that must be refused",
                "desc": "List of task categories to refuse even if the "
                    "request mentions an allowed domain, for example "
                    "`creative-writing`, `roleplay`, or "
                    "`general-world-knowledge`.",
                "required": False,
            },
            "min_confidence": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(FloatType),),
                "display_name": "Minimum Confidence",
                "short_desc": "Minimum classifier confidence required to pass",
                "desc": "Requests classified below this confidence fail when "
                    "`fail_uncertain` is true.  Default 0.65 is conservative "
                    "for public endpoints.",
                "default_value": 0.65,
                "required": False,
                "preselected": True,
            },
            "fail_uncertain": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(BoolType),),
                "display_name": "Fail Uncertain",
                "short_desc": "Block low-confidence or unavailable classifications",
                "desc": "When true, the rule fails if classification is "
                    "unavailable or confidence is below `min_confidence`.",
                "default_value": True,
                "required": False,
                "preselected": True,
            },
        };
    }

    hash<GuardrailResult> evaluate(string text, *hash<auto> ctx) {
        date start = now_us();
        if (!llm) {
            if (getFailClosed() || fail_uncertain) {
                hash<GuardrailResult> result = makeFailResult(
                    "task policy classification unavailable without LLM",
                    NOTHING, {"reason_code": "classifier-unavailable"});
                result.duration_ms = getElapsedMs(start);
                return result;
            }
            hash<GuardrailResult> result = makePassResult();
            result.duration_ms = getElapsedMs(start);
            return result;
        }

        try {
            QoreLlmUtils::LlmRequestClassifier classifier(llm, policy);
            hash<QoreLlmUtils::LlmRequestClassification> classified =
                classifier.classify(text, ctx);
            if (!classified.allowed) {
                hash<GuardrailResult> result = makeFailResult(
                    sprintf("request is outside allowed task policy: %s",
                        classified.reason ?? classified.reason_code ?? "blocked"),
                    NOTHING, makeMetadata(classified));
                result.duration_ms = getElapsedMs(start);
                return result;
            }
            if (classified.confidence < min_confidence && fail_uncertain) {
                hash<GuardrailResult> result = makeFailResult(
                    sprintf("task policy classification confidence %.0f%% is "
                        "below %.0f%% threshold",
                        classified.confidence * 100, min_confidence * 100),
                    NOTHING, makeMetadata(classified)
                        + {"reason_code": "classification-uncertain"});
                result.duration_ms = getElapsedMs(start);
                return result;
            }
        } catch (hash<ExceptionInfo> ex) {
            if (getFailClosed() || fail_uncertain) {
                hash<GuardrailResult> result = makeFailResult(
                    sprintf("task policy classification unavailable: %s",
                        ex.desc), NOTHING, {"reason_code": "classifier-error",
                        "error_type": ex.err});
                result.duration_ms = getElapsedMs(start);
                return result;
            }
        }

        hash<GuardrailResult> result = makePassResult();
        result.duration_ms = getElapsedMs(start);
        return result;
    }

    private static hash<auto> makeMetadata(
            hash<QoreLlmUtils::LlmRequestClassification> classified) {
        return {
            "allowed": classified.allowed,
            "detected_language": classified.detected_language,
            "domain": classified.domain,
            "task_category": classified.task_category,
            "confidence": classified.confidence,
            "blocked_category": classified.blocked_category,
            "reason_code": classified.reason_code,
            "reason": classified.reason,
            "usage": classified.usage,
        };
    }
}

} # namespace QoreGuardrails
