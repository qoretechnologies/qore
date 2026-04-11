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

    @par Example: Build and run a guardrail chain
    @code{.py}
GuardrailEngine engine((
    <GuardrailRuleConfig>{"type": "pii", "severity": "critical",
        "params": {"action": "block"}},
    <GuardrailRuleConfig>{"type": "output_format", "severity": "high",
        "params": {"max_length": 5000}},
));

hash<GuardrailResult> result = engine.evaluateInput("user message...");
if (!result.passed) {
    # block the request
}
    @endcode

    @since QoreGuardrails 1.0
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
            push rules, createRule(config, global_options);
        }
    }

    #! Evaluates input text against all configured rules
    /** @param text the input text to check
        @param context optional context for rule evaluation
        @return a @ref GuardrailResult hash
    */
    hash<GuardrailResult> evaluateInput(string text, *hash<auto> ctx) {
        return evaluateRules(text, ctx);
    }

    #! Evaluates output text against all configured rules
    /** @param text the output text to check
        @param context optional context (e.g., RAG sources for hallucination check)
        @return a @ref GuardrailResult hash
    */
    hash<GuardrailResult> evaluateOutput(string text, *hash<auto> ctx) {
        return evaluateRules(text, ctx);
    }

    #! Returns the number of configured rules
    int getRuleCount() {
        return rules.size();
    }

    #! Evaluates all rules against the text
    private hash<GuardrailResult> evaluateRules(string text, *hash<auto> ctx) {
        date start = now_us();
        list<hash<GuardrailViolation>> all_violations = ();
        *string current_text = NOTHING;
        int evaluated = 0;
        bool passed = True;

        foreach AbstractGuardrailRule rule in (rules) {
            # Use modified text from previous rule if available (e.g., after redaction)
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
        }
        throw "GUARDRAIL-CONFIG-ERROR",
            sprintf("unknown rule type %y; expected: pii, prompt_injection, "
                "topic_restriction, output_format", config.type);
    }
}

} # namespace QoreGuardrails
