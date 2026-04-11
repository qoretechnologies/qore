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
    #! Rule type: "pii", "prompt_injection", "topic_restriction", "output_format"
    string type;
    #! Whether this rule is enabled
    bool enabled = True;
    #! Severity for violations from this rule
    string severity = GS_HIGH;
    #! Rule-specific parameters
    *hash<auto> params;
}

#! Abstract base class for guardrail rules
/** Subclasses implement specific detection logic (pattern-based, LLM-based, etc.)

    @since QoreGuardrails 1.0
*/
public class AbstractGuardrailRule {
    private {
        #! The severity level for violations from this rule
        string severity;
    }

    #! Creates the rule with the given severity
    constructor(string severity = GS_HIGH) {
        self.severity = severity;
    }

    #! Returns the rule name (used in violations)
    abstract string getRuleName();

    #! Evaluates the rule against the given text
    /** @param text the text to evaluate
        @param context optional context (e.g., allowed topics, RAG sources)
        @return a @ref GuardrailResult hash
    */
    abstract hash<GuardrailResult> evaluate(string text, *hash<auto> ctx);

    #! Returns the configured severity
    string getSeverity() {
        return severity;
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
}

} # namespace QoreGuardrails
