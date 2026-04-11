# -*- mode: qore; indent-tabs-mode: nil -*-

/*  PiiDetector.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see PiiDetector
public namespace QoreGuardrails {

#! Pattern-based PII detection and redaction rule
/** Detects common PII patterns in text:
    - Email addresses
    - Phone numbers (US, international)
    - Social Security Numbers (US SSN)
    - Credit card numbers (Visa, MasterCard, AmEx, Discover)

    Action modes:
    - \b block: fail the guardrail check (default)
    - \b redact: replace PII with placeholder text and pass
    - \b warn: report violations but pass

    @since QoreGuardrails 1.0
*/
public class PiiDetector inherits AbstractGuardrailRule {
    public {
        #! PII type constants
        const PII_EMAIL = "email";
        const PII_PHONE = "phone";
        const PII_SSN = "ssn";
        const PII_CREDIT_CARD = "credit_card";

        #! Action modes
        const ACTION_BLOCK = "block";
        const ACTION_REDACT = "redact";
        const ACTION_WARN = "warn";

        #! Patterns for PII detection
        const EmailPattern = "[a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.[a-zA-Z]{2,}";
        const PhonePattern = "(?:\\+?1[\\s.-]?)?\\(?\\d{3}\\)?[\\s.-]?\\d{3}[\\s.-]?\\d{4}";
        const SsnPattern = "\\b\\d{3}[\\s.-]\\d{2}[\\s.-]\\d{4}\\b";
        const CreditCardPattern =
            "\\b(?:4\\d{3}|5[1-5]\\d{2}|3[47]\\d{2}|6(?:011|5\\d{2}))"
            "[\\s.-]?\\d{4}[\\s.-]?\\d{4}[\\s.-]?\\d{1,4}\\b";
    }

    private {
        #! Action to take: "block", "redact", or "warn"
        string action;
        #! Which PII types to detect
        hash<string, bool> enabled_types;
    }

    #! Creates a PII detector
    /** @param severity violation severity level
        @param params optional parameters:
            - \c action (string): "block", "redact", or "warn" (default "block")
            - \c types (list<string>): PII types to detect (default all)
    */
    constructor(string severity = GS_CRITICAL, *hash<auto> params)
            : AbstractGuardrailRule(severity) {
        action = params.action ?? ACTION_BLOCK;
        if (action != ACTION_BLOCK && action != ACTION_REDACT && action != ACTION_WARN) {
            throw "GUARDRAIL-CONFIG-ERROR",
                sprintf("invalid PII action %y; expected block, redact, or warn", action);
        }

        # Default: detect all types
        enabled_types = {
            PII_EMAIL: True,
            PII_PHONE: True,
            PII_SSN: True,
            PII_CREDIT_CARD: True,
        };

        if (params.types) {
            enabled_types = {};
            foreach string t in (params.types) {
                enabled_types{t} = True;
            }
        }
    }

    string getRuleName() {
        return "pii";
    }

    hash<GuardrailResult> evaluate(string text, *hash<auto> ctx) {
        date start = now_us();
        list<hash<auto>> findings = ();
        string redacted = text;

        if (enabled_types{PII_EMAIL}) {
            findings += detectPattern(text, EmailPattern, PII_EMAIL);
        }
        if (enabled_types{PII_PHONE}) {
            findings += detectPattern(text, PhonePattern, PII_PHONE);
        }
        if (enabled_types{PII_SSN}) {
            findings += detectPattern(text, SsnPattern, PII_SSN);
        }
        if (enabled_types{PII_CREDIT_CARD}) {
            findings += detectPattern(text, CreditCardPattern, PII_CREDIT_CARD);
        }

        if (!findings) {
            hash<GuardrailResult> result = makePassResult();
            result.duration_ms = getElapsedMs(start);
            return result;
        }

        # Build violations
        list<hash<GuardrailViolation>> violations = ();
        foreach hash<auto> finding in (findings) {
            push violations, <GuardrailViolation>{
                "rule": getRuleName(),
                "severity": severity,
                "detail": sprintf("%s detected: %s", finding.pii_type, finding.matched),
                "metadata": {"pii_type": finding.pii_type},
            };
        }

        # Redact if requested
        *string modified;
        if (action == ACTION_REDACT) {
            redacted = text;
            foreach hash<auto> finding in (findings) {
                string placeholder = sprintf("[%s_REDACTED]", finding.pii_type.upr());
                redacted = regex_subst(redacted, regex_escape(finding.matched),
                    placeholder, RE_Global);
            }
            modified = redacted;
        }

        bool passed = (action == ACTION_WARN || action == ACTION_REDACT);

        return <GuardrailResult>{
            "passed": passed,
            "violations": violations,
            "modified_text": modified,
            "rules_evaluated": 1,
            "duration_ms": getElapsedMs(start),
        };
    }

    #! Detects all matches of a pattern in text
    private list<hash<auto>> detectPattern(string text, string pattern, string pii_type) {
        list<hash<auto>> findings = ();
        # Use regex_extract with a capture group to find the first match,
        # then advance past it to find subsequent matches
        string remaining = text;
        while (remaining.val()) {
            *list<string> match = regex_extract(remaining, "(" + pattern + ")");
            if (!match) {
                break;
            }
            push findings, {
                "pii_type": pii_type,
                "matched": match[0],
            };
            # Move past this match
            int pos = index(remaining, match[0]);
            if (pos >= 0) {
                remaining = remaining.substr(pos + match[0].length());
            } else {
                break;
            }
        }
        return findings;
    }

    #! Escapes special regex characters in a string for literal matching
    private string regex_escape(string s) {
        return regex_subst(s, "([\\\\\\[\\](){}.*+?^$|])", "\\\\$1", RE_Global);
    }

    #! Calculates elapsed milliseconds since start
    private int getElapsedMs(date start) {
        return (now_us() - start).durationMicroseconds() / 1000;
    }
}

} # namespace QoreGuardrails
