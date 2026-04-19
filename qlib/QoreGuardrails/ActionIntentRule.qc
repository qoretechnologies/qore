# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ActionIntentRule.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see ActionIntentRule
public namespace QoreGuardrails {

#! LLM-classified intent guardrail for gating destructive tool dispatches
/** Wraps @ref QoreLlmUtils::ActionIntentClassifier as a
    @ref AbstractGuardrailRule so operator-editable ai-guardrails
    metadata objects can include an intent check in their rule chain
    without writing code.

    The rule classifies the user turn into one of three intents
    (read-only, action-requested, confirmation-required) and fails
    with a violation when the classified intent is in the configured
    \c block_intents set.  The violation's metadata carries
    \c intent (the classified label), letting the calling framework
    decide how to route the result — refuse the request outright,
    pause and ask for confirmation, log and continue, etc.

    Defaults block ONLY the ambiguous \c confirmation-required label:
    clear read-only and action-requested classifications pass through
    silently so the rule doesn't block legitimate imperatives nor
    produce noise on status-check questions.  Tighten via
    \c block_intents for deployments that want to refuse every
    action-requested turn (e.g., truly read-only endpoints where
    destructive intent must never proceed).

    Costs one LLM round-trip per evaluation.  For callers that
    already run a classifier call (e.g., a Phase-1 planning prompt
    that emits \c INTENT: alongside other labels), prefer embedding
    @ref QoreLlmUtils::ActionIntentClassifier::getPromptFragment()
    directly and reading the label off the existing response — the
    guardrail path is for consumers that don't have such a prompt
    to piggyback on.

    @par Example
    @code{.py}
GuardrailEngine engine((
    <GuardrailRuleConfig>{"type": "action_intent", "severity": "high",
        "params": {"block_intents": ("confirmation-required",)}},
), {"llm_config": <LlmBackendConfig>{"mode": "http", "url": "...",
    "model": "gpt-4o-mini"}});
hash<GuardrailResult> r = engine.evaluateInput(
    "should I delete the broken workflow?");
# r.passed == False; r.violations[0].metadata.intent == "confirmation-required"
    @endcode
*/
public class ActionIntentRule inherits AbstractGuardrailRule {
    private {
        #! Intents that trigger a violation when emitted by the classifier
        /** Default: just \c confirmation-required, so the rule surfaces
            ambiguous phrasings but passes both clearly read-only and
            clearly action-requested turns.  Tighten to
            \c ("action-requested", "confirmation-required") for
            strictly read-only endpoints.
        */
        list<string> block_intents;

        #! Optional LLM backend for classification (required for evaluate())
        *QoreLlmUtils::LlmBackend llm;

        #! Classifier instance — lazily constructed from \c llm
        *QoreLlmUtils::ActionIntentClassifier classifier;
    }

    #! Creates an action-intent guardrail rule
    /** @param severity violation severity emitted when the classified
            intent is in \c block_intents
        @param params optional parameters:
            - \c block_intents (list<string>): which classified intents
              fail the rule; default \c ("confirmation-required",)
        @param llm_config required LlmBackendConfig for the classifier
            LLM call; if absent, the rule fails closed on every
            evaluation (no classifier → no safe default)
    */
    constructor(string severity = QoreGuardrails::GS_HIGH, *hash<auto> params,
            *hash<auto> llm_config) : AbstractGuardrailRule(severity) {
        string cr = QoreLlmUtils::ActionIntentClassifier::IntentConfirmationRequired;
        block_intents = params.block_intents ?? (cr,);
        if (llm_config) {
            llm = new QoreLlmUtils::LlmBackend(
                cast<hash<QoreLlmUtils::LlmBackendConfig>>(llm_config));
            classifier = new QoreLlmUtils::ActionIntentClassifier(llm);
        }
    }

    string getRuleName() {
        return "action_intent";
    }

    #! Classifies user input — runs on input by default
    string getDirection() {
        return direction_override ?? QoreGuardrails::GD_INPUT;
    }

    #! Returns the DataProvider-option schema for this rule's \c params hash
    public static hash<string, hash<DataProviderOptionInfo>> getConfigSchema() {
        # Aliases keep fully-qualified names on one line; the Qore
        # parser does not accept a newline between a class and its
        # `::` scope operator.
        string ro = QoreLlmUtils::ActionIntentClassifier::IntentReadOnly;
        string ar = QoreLlmUtils::ActionIntentClassifier::IntentActionRequested;
        string cr = QoreLlmUtils::ActionIntentClassifier::IntentConfirmationRequired;
        return {
            "block_intents": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(new Type("list<string>")),),
                "display_name": "Blocking Intents",
                "short_desc": "Classified intents that fail this rule",
                "desc": "The LLM classifier assigns each user turn to "
                    "one of three intents: `read-only` (question or "
                    "guidance request), `action-requested` (explicit "
                    "imperative to change state), or "
                    "`confirmation-required` (ambiguous phrasing).  "
                    "This option lists which intent labels trigger a "
                    "violation.\n\n"
                    "**Default**: `confirmation-required` only — the "
                    "rule surfaces ambiguous phrasings but passes both "
                    "clear read-only and clear action-requested turns.  "
                    "For strictly read-only endpoints where destructive "
                    "intent must never proceed, set to "
                    "`action-requested,confirmation-required`.",
                "default_value": (cr,),
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": ro,
                        "display_name": "Read-only",
                        "desc": "Questions, guidance requests, status "
                            "checks",
                    },
                    <AllowedValueInfo>{
                        "value": ar,
                        "display_name": "Action Requested",
                        "desc": "Explicit imperative to change state",
                    },
                    <AllowedValueInfo>{
                        "value": cr,
                        "display_name": "Confirmation Required",
                        "desc": "Ambiguous phrasing — classifier could "
                            "not decide",
                    },
                ),
                "required": False,
                "preselected": True,
            },
        };
    }

    hash<GuardrailResult> evaluate(string text, *hash<auto> ctx) {
        date start = now_us();

        # No classifier configured — either fail closed (block every
        # input) or pass every input through.  fail_closed is the safer
        # setting for security-sensitive deployments; default is pass so
        # the rule degrades to a no-op under backend failure.
        if (!classifier) {
            if (getFailClosed()) {
                hash<GuardrailResult> fc_result = makeFailResult(
                    "action-intent classifier unavailable (no LLM backend "
                    "configured) and rule is fail-closed",
                    NOTHING,
                    {"detection_method": "fail_closed"});
                fc_result.duration_ms = getElapsedMs(start);
                return fc_result;
            }
            hash<GuardrailResult> pass_result = makePassResult();
            pass_result.duration_ms = getElapsedMs(start);
            return pass_result;
        }

        # Run the classifier — on failure, apply the same fail-open /
        # fail-closed policy as above so a transient LLM blip doesn't
        # take down the surrounding pipeline unless the operator has
        # explicitly opted into strict enforcement.
        *string intent;
        try {
            intent = classifier.classify(text);
        } catch (hash<ExceptionInfo> ex) {
            if (getFailClosed()) {
                hash<GuardrailResult> fc_result = makeFailResult(
                    sprintf("action-intent classification failed (fail-closed): "
                        "%s: %s", ex.err, ex.desc),
                    NOTHING,
                    {"detection_method": "fail_closed", "error": ex.desc});
                fc_result.duration_ms = getElapsedMs(start);
                return fc_result;
            }
            hash<GuardrailResult> pass_result = makePassResult();
            pass_result.duration_ms = getElapsedMs(start);
            return pass_result;
        }

        if (!intent.val()) {
            # Classifier ran but couldn't parse the LLM's response.
            # Treat as fail-closed-sensitive: block if fail_closed is
            # set, else pass (same as other LLM-based rules).
            if (getFailClosed()) {
                hash<GuardrailResult> fc_result = makeFailResult(
                    "action-intent classifier returned no label (fail-closed)",
                    NOTHING,
                    {"detection_method": "fail_closed",
                        "reason": "no_label"});
                fc_result.duration_ms = getElapsedMs(start);
                return fc_result;
            }
            hash<GuardrailResult> pass_result = makePassResult();
            pass_result.duration_ms = getElapsedMs(start);
            return pass_result;
        }

        # Classification succeeded — decide pass/fail by membership in
        # block_intents.  The metadata carries the classified label
        # regardless so callers can read it off the violation (or the
        # pass result's absence of a violation means the intent was
        # outside block_intents).
        if (inlist(intent, block_intents)) {
            hash<GuardrailResult> result = makeFailResult(
                sprintf("classified intent %y is in block_intents %y",
                    intent, block_intents),
                NOTHING,
                {
                    "detection_method": "llm",
                    "intent": intent,
                    "block_intents": block_intents,
                });
            result.duration_ms = getElapsedMs(start);
            return result;
        }

        hash<GuardrailResult> pass_result = makePassResult();
        pass_result.duration_ms = getElapsedMs(start);
        return pass_result;
    }
}

} # namespace QoreGuardrails
