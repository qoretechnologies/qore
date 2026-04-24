# -*- mode: qore; indent-tabs-mode: nil -*-

/*  GuardrailEngine.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see GuardrailEngine
public namespace QoreGuardrails {

#! Orchestrates a chain of guardrail rules
/** Evaluates rules in order, collecting violations. By default, short-circuits
    on the first critical violation. Set \c evaluate_all to override.

    Rules are filtered by direction: \c evaluateInput() only runs rules with
    direction "input" or "both", and \c evaluateOutput() only runs rules with
    direction "output" or "both".

    @par Example: Build and run a guardrail chain
    @code{.py}
GuardrailEngine engine((
    <GuardrailRuleConfig>{"type": "pii", "severity": "critical",
        "params": {"action": "block"}},
    <GuardrailRuleConfig>{"type": "prompt_injection", "severity": "critical"},
    <GuardrailRuleConfig>{"type": "output_format", "severity": "high",
        "params": {"max_length": 5000}},
));

# prompt_injection runs, output_format is skipped (output-only rule)
hash<GuardrailResult> input_result = engine.evaluateInput("user message...");

# output_format runs, prompt_injection is skipped (input-only rule)
hash<GuardrailResult> output_result = engine.evaluateOutput("ai response...");
    @endcode
*/
public class GuardrailEngine {
    private {
        #! Ordered list of rules to evaluate
        list<AbstractGuardrailRule> rules;
        #! Whether to evaluate all rules even after a critical failure
        bool evaluate_all;
    }

    #! Creates a GuardrailEngine with the given rule configurations
    /** @param rule_configs list of rule configurations
        @param global_options optional global settings:
            - \c evaluate_all (bool): if True, evaluate all rules even after
              critical violations (default False)
            - \c fail_closed (bool): global default for fail-closed mode
              (can be overridden per-rule)
            - \c llm_config (hash): LlmBackendConfig for LLM-based rules

        @throw GUARDRAIL-CONFIG-ERROR if a rule type is unknown
    */
    constructor(list<hash<GuardrailRuleConfig>> rule_configs,
            *hash<auto> global_options) {
        evaluate_all = global_options.evaluate_all ?? False;
        rules = ();

        foreach hash<GuardrailRuleConfig> config in (rule_configs) {
            if (!config.enabled) {
                continue;
            }
            AbstractGuardrailRule rule = createRule(config, global_options);

            # Apply direction override from config
            if (config.direction.val()) {
                rule.setDirection(config.direction);
            }

            # Apply fail-closed setting
            bool fc = config.fail_closed ?? global_options.fail_closed ?? False;
            rule.setFailClosed(fc);

            push rules, rule;
        }
    }

    #! Evaluates input text against rules with direction "input" or "both"
    /** @param text the input text to check
        @param ctx optional context for rule evaluation
        @return a @ref GuardrailResult hash
    */
    hash<GuardrailResult> evaluateInput(string text, *hash<auto> ctx) {
        return evaluateRules(text, ctx, GD_INPUT);
    }

    #! Evaluates output text against rules with direction "output" or "both"
    /** @param text the output text to check
        @param ctx optional context (e.g., RAG sources for hallucination check)
        @return a @ref GuardrailResult hash
    */
    hash<GuardrailResult> evaluateOutput(string text, *hash<auto> ctx) {
        return evaluateRules(text, ctx, GD_OUTPUT);
    }

    #! Returns the number of configured rules
    int getRuleCount() {
        return rules.size();
    }

    #! Returns the registry of built-in rule types with their config schemas
    /** Aggregates @ref RuleTypeInfo entries for every built-in rule class.
        UI tools use this to render a typed form when an operator is
        editing a guardrail rule chain: for each rule type the returned
        hash carries a human-readable \c display_name, a markdown
        \c desc, the rule's natural \c default_direction, and a
        \c config_schema shaped exactly like a DataProvider option hash
        so the existing option-form renderer handles it with no
        rule-type-specific code.

        @return a hash keyed by rule-type string
            (\c "pii", \c "prompt_injection", \c "topic_restriction",
            \c "output_format", \c "toxicity", \c "hallucination",
            \c "action_intent") where each value is a
            @ref RuleTypeInfo describing that rule type

        @par Example
        @code{.py}
hash<string, hash<RuleTypeInfo>> registry = GuardrailEngine::getRuleTypeRegistry();
foreach hash<RuleTypeInfo> info in (registry.iterator()) {
    printf("%s — %s\n", info.display_name, info.short_desc);
    foreach hash<auto> p in (info.config_schema.pairIterator()) {
        printf("  %s (%s): %s\n", p.key,
            p.value.type[0].getName(), p.value.short_desc);
    }
}
        @endcode

        @since QoreGuardrails 1.1
    */
    public static hash<string, hash<RuleTypeInfo>> getRuleTypeRegistry() {
        return {
            "pii": <RuleTypeInfo>{
                "type": "pii",
                "display_name": "PII Detection",
                "short_desc": "Detects personal data (email, phone, SSN, "
                    "credit card, IBAN, and custom patterns) in user "
                    "messages",
                "desc": "Scans the text for personally identifiable "
                    "information using regex patterns with optional "
                    "mathematical validation (Luhn checksum for credit "
                    "cards, mod-97 for IBANs).  The built-in patterns "
                    "cover the most common types; you can add your own "
                    "via the `custom_patterns` parameter.  Action mode "
                    "controls whether a match redacts (replace with "
                    "placeholder), blocks (reject the whole request), or "
                    "just logs the violation.",
                "default_direction": GD_BOTH,
                "config_schema": PiiDetector::getConfigSchema(),
            },
            "prompt_injection": <RuleTypeInfo>{
                "type": "prompt_injection",
                "display_name": "Prompt Injection Detection",
                "short_desc": "Detects attempts to override the system "
                    "prompt, jailbreak the assistant, or smuggle "
                    "instructions via user input",
                "desc": "Scans user messages for prompt-injection patterns "
                    "(instruction-override language, role-play escape "
                    "attempts, jailbreak templates) and optionally runs "
                    "an LLM classifier in parallel.  Recommended for "
                    "every public-facing endpoint where untrusted users "
                    "can control the input.",
                "default_direction": GD_INPUT,
                "config_schema": PromptInjectionDetector::getConfigSchema(),
            },
            "topic_restriction": <RuleTypeInfo>{
                "type": "topic_restriction",
                "display_name": "Topic Restriction",
                "short_desc": "Blocks user messages that don't match one "
                    "of the endpoint's allowed topics",
                "desc": "Uses an LLM classifier to decide whether each "
                    "user message is on-topic for the endpoint's "
                    "configured purpose.  Off-topic messages are blocked "
                    "before they reach the main LLM.  Use this for "
                    "endpoints with a narrow remit (customer support for "
                    "a specific product, order tracking, tech help) "
                    "where you want to politely refuse out-of-scope "
                    "requests.",
                "default_direction": GD_INPUT,
                "config_schema": TopicRestrictionRule::getConfigSchema(),
            },
            "output_format": <RuleTypeInfo>{
                "type": "output_format",
                "display_name": "Output Format Enforcement",
                "short_desc": "Validates LLM responses against length "
                    "limits and regex patterns",
                "desc": "Checks the LLM response against pattern-based "
                    "constraints: maximum/minimum length, forbidden "
                    "patterns that must NOT appear (e.g. internal URLs, "
                    "system prompt text), and required patterns that "
                    "MUST appear (e.g. disclaimer footers, citation "
                    "markers).  Fails the response if any constraint "
                    "is violated.",
                "default_direction": GD_OUTPUT,
                "config_schema": OutputFormatEnforcer::getConfigSchema(),
            },
            "toxicity": <RuleTypeInfo>{
                "type": "toxicity",
                "display_name": "Toxicity Detection",
                "short_desc": "Detects toxic, harmful, or abusive language "
                    "in user messages or LLM responses",
                "desc": "Classifies text toxicity using a local ONNX model, "
                    "an LLM classifier, or both.  Useful for moderation "
                    "on both the input side (reject abusive user "
                    "messages before they reach the LLM) and the output "
                    "side (catch the rare case where the LLM itself "
                    "produces inappropriate content).",
                "default_direction": GD_BOTH,
                "config_schema": ToxicityDetector::getConfigSchema(),
            },
            "hallucination": <RuleTypeInfo>{
                "type": "hallucination",
                "display_name": "Hallucination Detection",
                "short_desc": "Validates LLM responses against the source "
                    "texts retrieved by the RAG pipeline",
                "desc": "For RAG endpoints, asks a grounding LLM to rate "
                    "how well the response is supported by the source "
                    "passages retrieved for the user's query.  "
                    "Responses below the configured grounding-score "
                    "threshold are flagged as hallucinations.  Requires "
                    "the RAG pipeline to stash the source texts in the "
                    "evaluation context under the configured key.",
                "default_direction": GD_OUTPUT,
                "config_schema": HallucinationDetector::getConfigSchema(),
            },
            "action_intent": <RuleTypeInfo>{
                "type": "action_intent",
                "display_name": "Action-Intent Classification",
                "short_desc": "Classifies the user turn as read-only, "
                    "action-requested, or confirmation-required; fails "
                    "the rule when the classified intent is in the "
                    "configured block list",
                "desc": "Asks an LLM to classify the user's intent "
                    "against a canonical three-label taxonomy.  "
                    "Typical use is to refuse destructive tool "
                    "dispatches when the user's phrasing is ambiguous "
                    "(`confirmation-required`) or clearly "
                    "informational (`read-only`).  Adds one LLM round-"
                    "trip per evaluation; for callers that already run "
                    "a classifier LLM call (planning prompts etc.), "
                    "prefer embedding "
                    "`QoreLlmUtils::ActionIntentClassifier::getPromptFragment()` "
                    "directly and reading the `INTENT:` label off the "
                    "existing response instead of adding this rule.",
                "default_direction": GD_INPUT,
                "config_schema": ActionIntentRule::getConfigSchema(),
            },
        };
    }

    #! Evaluates rules matching the given phase direction
    private hash<GuardrailResult> evaluateRules(string text, *hash<auto> ctx,
            string phase) {
        date start = now_us();
        list<hash<GuardrailViolation>> all_violations = ();
        *string current_text = NOTHING;
        int evaluated = 0;
        bool passed = True;

        foreach AbstractGuardrailRule rule in (rules) {
            # Skip rules that don't apply to this phase
            string dir = rule.getDirection();
            if (dir != GD_BOTH && dir != phase) {
                continue;
            }

            # Use modified text from previous rule if available
            string eval_text = current_text ?? text;
            hash<GuardrailResult> result = rule.evaluate(eval_text, ctx);
            ++evaluated;

            if (result.violations) {
                all_violations += result.violations;
            }
            if (result.modified_text.val()) {
                current_text = result.modified_text;
            }
            if (!result.passed) {
                passed = False;
                # Short-circuit on critical violations unless evaluate_all is set
                if (!evaluate_all && hasCriticalViolation(result.violations)) {
                    break;
                }
            }
        }

        int elapsed_ms = (now_us() - start).durationMicroseconds() / 1000;

        return <GuardrailResult>{
            "passed": passed,
            "violations": all_violations,
            "modified_text": current_text,
            "rules_evaluated": evaluated,
            "duration_ms": elapsed_ms,
        };
    }

    #! Checks if any violation has critical severity
    private bool hasCriticalViolation(list<hash<GuardrailViolation>> violations) {
        foreach hash<GuardrailViolation> v in (violations) {
            if (v.severity == GS_CRITICAL) {
                return True;
            }
        }
        return False;
    }

    #! Creates a rule instance from configuration
    private AbstractGuardrailRule createRule(hash<GuardrailRuleConfig> config,
            *hash<auto> global_options) {
        switch (config.type) {
            case "pii":
                return new PiiDetector(config.severity, config.params);
            case "prompt_injection":
                return new PromptInjectionDetector(config.severity, config.params,
                    global_options.llm_config);
            case "topic_restriction":
                return new TopicRestrictionRule(config.severity, config.params,
                    global_options.llm_config);
            case "output_format":
                return new OutputFormatEnforcer(config.severity, config.params);
            case "toxicity":
                return new ToxicityDetector(config.severity, config.params,
                    global_options.llm_config);
            case "hallucination":
                return new HallucinationDetector(config.severity, config.params,
                    global_options.llm_config);
            case "action_intent":
                return new ActionIntentRule(config.severity, config.params,
                    global_options.llm_config);
        }
        throw "GUARDRAIL-CONFIG-ERROR",
            sprintf("unknown rule type %y; expected: pii, prompt_injection, "
                "topic_restriction, output_format, toxicity, hallucination, "
                "action_intent",
                config.type);
    }
}

} # namespace QoreGuardrails
