# -*- mode: qore; indent-tabs-mode: nil -*-

/*  AbstractGuardrailRule.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see AbstractGuardrailRule
public namespace QoreGuardrails {

#! Severity levels for guardrail violations
public const GS_WARNING = "warning";
public const GS_HIGH = "high";
public const GS_CRITICAL = "critical";

#! Rule direction — when the rule should be evaluated
public const GD_INPUT = "input";
public const GD_OUTPUT = "output";
public const GD_BOTH = "both";

#! A single guardrail violation
public hashdecl GuardrailViolation {
    #! Rule name that triggered this violation
    string rule;
    #! Severity: "warning", "high", or "critical"
    string severity;
    #! Human-readable description of the violation
    string detail;
    #! If text was redacted, the redacted version
    *string redacted_text;
    #! Additional metadata (rule-specific)
    *hash<auto> metadata;
}

#! Result from evaluating a guardrail rule or chain
public hashdecl GuardrailResult {
    #! Whether the text passed all rules
    bool passed;
    #! List of violations found
    list<hash<GuardrailViolation>> violations;
    #! Modified text (e.g., after PII redaction); NOTHING if unchanged
    *string modified_text;
    #! Number of rules evaluated
    int rules_evaluated;
    #! Total evaluation duration in milliseconds
    int duration_ms;
}

#! Configuration for a single guardrail rule
public hashdecl GuardrailRuleConfig {
    #! Rule type: "pii", "prompt_injection", "topic_restriction", "output_format",
    #! "toxicity", "hallucination"
    string type;
    #! Whether this rule is enabled
    bool enabled = True;
    #! Severity for violations from this rule
    string severity = GS_HIGH;
    #! Rule-specific parameters
    *hash<auto> params;
    #! When to evaluate: "input", "output", or "both"; if not set, uses the
    #! rule's default (e.g., prompt_injection defaults to "input")
    *string direction;
    #! If True, LLM/ONNX errors cause the rule to FAIL instead of pass;
    #! use this for high-security deployments where silent pass-through is
    #! not acceptable
    *bool fail_closed;
}

#! Abstract base class for guardrail rules
/** Subclasses implement specific detection logic (pattern-based, LLM-based,
    ONNX-based, etc.).

    Each rule declares a default direction (input, output, or both) which
    controls whether it runs during \c evaluateInput() or \c evaluateOutput().
    The direction can be overridden per-rule via \c GuardrailRuleConfig.direction.

    Rules that depend on external services (LLM, ONNX) can be configured to
    fail-closed — blocking content when the service is unavailable instead of
    silently passing.

    @since QoreGuardrails 1.0
*/
public class AbstractGuardrailRule {
    private {
        #! The severity level for violations from this rule
        string severity;
        #! Direction override from config (NOTHING = use class default)
        *string direction_override;
        #! Whether to fail when external service is unavailable
        bool fail_closed = False;
    }

    #! Creates the rule with the given severity
    constructor(string severity = GS_HIGH) {
        self.severity = severity;
    }

    #! Returns the rule name (used in violations)
    abstract string getRuleName();

    #! Evaluates the rule against the given text
    /** @param text the text to evaluate
        @param ctx optional context (e.g., allowed topics, RAG sources)
        @return a @ref GuardrailResult hash
    */
    abstract hash<GuardrailResult> evaluate(string text, *hash<auto> ctx);

    #! Returns the rule's effective direction
    /** Returns the config override if set, otherwise the class default.
        Subclasses override this to set their natural default.

        @return "input", "output", or "both"
    */
    string getDirection() {
        return direction_override ?? GD_BOTH;
    }

    #! Sets the direction override from config
    /** @param direction "input", "output", "both", or NOTHING for class default
    */
    setDirection(*string direction) {
        direction_override = direction;
    }

    #! Returns the configured severity
    string getSeverity() {
        return severity;
    }

    #! Sets whether this rule fails when external services are unavailable
    setFailClosed(bool val) {
        fail_closed = val;
    }

    #! Returns whether this rule fails when external services are unavailable
    bool getFailClosed() {
        return fail_closed;
    }

    #! Creates a passing result
    hash<GuardrailResult> makePassResult() {
        return <GuardrailResult>{
            "passed": True,
            "violations": (),
            "rules_evaluated": 1,
            "duration_ms": 0,
        };
    }

    #! Creates a failing result with a single violation
    hash<GuardrailResult> makeFailResult(string detail, *string redacted_text,
            *hash<auto> metadata) {
        return <GuardrailResult>{
            "passed": False,
            "violations": (
                <GuardrailViolation>{
                    "rule": getRuleName(),
                    "severity": severity,
                    "detail": detail,
                    "redacted_text": redacted_text,
                    "metadata": metadata,
                },
            ),
            "modified_text": redacted_text,
            "rules_evaluated": 1,
            "duration_ms": 0,
        };
    }

    #! Calculates elapsed milliseconds since start
    private int getElapsedMs(date start) {
        return (now_us() - start).durationMicroseconds() / 1000;
    }
}

} # namespace QoreGuardrails
