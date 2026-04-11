# -*- mode: qore; indent-tabs-mode: nil -*-

/*  OutputFormatEnforcer.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see OutputFormatEnforcer
public namespace QoreGuardrails {

#! Pattern-based output format validation
/** Validates LLM output against format constraints:
    - Maximum text length
    - Minimum text length
    - Forbidden patterns (e.g., code blocks, URLs, profanity patterns)
    - Required patterns (e.g., must contain a greeting)

    @since QoreGuardrails 1.0
*/
public class OutputFormatEnforcer inherits AbstractGuardrailRule {
    private {
        #! Maximum output length in characters (0 = no limit)
        int max_length;
        #! Minimum output length in characters (0 = no minimum)
        int min_length;
        #! Forbidden regex patterns
        list<string> forbidden_patterns;
        #! Required regex patterns
        list<string> required_patterns;
    }

    #! Creates an output format enforcer
    /** @param severity violation severity level
        @param params optional parameters:
            - \c max_length (int): maximum text length in characters
            - \c min_length (int): minimum text length in characters
            - \c forbidden_patterns (list<string>): regex patterns that must not appear
            - \c required_patterns (list<string>): regex patterns that must appear
    */
    constructor(string severity = GS_HIGH, *hash<auto> params)
            : AbstractGuardrailRule(severity) {
        max_length = params.max_length ?? 0;
        min_length = params.min_length ?? 0;
        forbidden_patterns = params.forbidden_patterns ?? ();
        required_patterns = params.required_patterns ?? ();
    }

    string getRuleName() {
        return "output_format";
    }

    hash<GuardrailResult> evaluate(string text, *hash<auto> ctx) {
        date start = now_us();
        list<hash<GuardrailViolation>> violations = ();

        # Check max length
        if (max_length > 0 && text.length() > max_length) {
            push violations, <GuardrailViolation>{
                "rule": getRuleName(),
                "severity": severity,
                "detail": sprintf("output exceeds maximum length: %d > %d characters",
                    text.length(), max_length),
                "metadata": {"check": "max_length", "actual": text.length(),
                    "limit": max_length},
            };
        }

        # Check min length
        if (min_length > 0 && text.length() < min_length) {
            push violations, <GuardrailViolation>{
                "rule": getRuleName(),
                "severity": severity,
                "detail": sprintf("output below minimum length: %d < %d characters",
                    text.length(), min_length),
                "metadata": {"check": "min_length", "actual": text.length(),
                    "limit": min_length},
            };
        }

        # Check forbidden patterns
        foreach string pattern in (forbidden_patterns) {
            if (regex(text, pattern, RE_Caseless)) {
                push violations, <GuardrailViolation>{
                    "rule": getRuleName(),
                    "severity": severity,
                    "detail": sprintf("output contains forbidden pattern: %s", pattern),
                    "metadata": {"check": "forbidden_pattern", "pattern": pattern},
                };
            }
        }

        # Check required patterns
        foreach string pattern in (required_patterns) {
            if (!regex(text, pattern, RE_Caseless)) {
                push violations, <GuardrailViolation>{
                    "rule": getRuleName(),
                    "severity": severity,
                    "detail": sprintf("output missing required pattern: %s", pattern),
                    "metadata": {"check": "required_pattern", "pattern": pattern},
                };
            }
        }

        int elapsed_ms = (now_us() - start).durationMicroseconds() / 1000;

        if (violations) {
            return <GuardrailResult>{
                "passed": False,
                "violations": violations,
                "rules_evaluated": 1,
                "duration_ms": elapsed_ms,
            };
        }

        return <GuardrailResult>{
            "passed": True,
            "violations": (),
            "rules_evaluated": 1,
            "duration_ms": elapsed_ms,
        };
    }
}

} # namespace QoreGuardrails
