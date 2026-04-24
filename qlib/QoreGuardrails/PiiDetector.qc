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
/** Detects personally identifiable information (PII) in text using regex
    patterns with optional mathematical validation (Luhn for credit cards,
    mod-97 for IBANs).

    Built-in PII types:
    - \b email: Email addresses
    - \b phone: US phone numbers (with word-boundary enforcement)
    - \b ssn: US Social Security Numbers
    - \b credit_card: Visa, MasterCard, AmEx, Discover (with Luhn validation)
    - \b iban: International Bank Account Numbers (with mod-97 checksum)
    - \b phone_intl: International phone numbers (+CC prefix)
    - \b uk_nin: UK National Insurance Numbers

    Custom PII types can be added via the \c custom_patterns parameter.

    Action modes:
    - \b block: fail the guardrail check (default)
    - \b redact: replace PII with placeholder text and pass
    - \b warn: report violations but pass

    @par Example: Detect and redact PII in text
    @code{.py}
PiiDetector pii("critical", {"action": "redact"});
hash<GuardrailResult> result = pii.evaluate("Email me at john@example.com");
if (result.modified_text) {
    printf("Redacted: %s\n", result.modified_text);
    # Output: "Email me at [EMAIL_REDACTED]"
}
    @endcode

    @par Example: Add a custom PII pattern
    @code{.py}
PiiDetector pii("high", {
    "action": "block",
    "custom_patterns": (
        {"name": "employee_id", "pattern": "EMP-\\d{6}"},
    ),
});
hash<GuardrailResult> r = pii.evaluate("Contact EMP-123456 for details");
# r.passed == False, violation has pii_type: "employee_id"
    @endcode
*/
public class PiiDetector inherits AbstractGuardrailRule {
    public {
        #! PII type constants — built-in types
        const PII_EMAIL = "email";
        const PII_PHONE = "phone";
        const PII_SSN = "ssn";
        const PII_CREDIT_CARD = "credit_card";
        const PII_IBAN = "iban";
        const PII_PHONE_INTL = "phone_intl";
        const PII_UK_NIN = "uk_nin";

        #! Action modes
        const ACTION_BLOCK = "block";
        const ACTION_REDACT = "redact";
        const ACTION_WARN = "warn";

        #! Patterns for PII detection
        const EmailPattern = "[a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.[a-zA-Z]{2,}";

        #! US phone with word boundary enforcement
        const PhonePattern =
            "(?:(?<=\\s)|(?<=^)|(?<=[(,]))"
            "(?:\\+?1[\\s.-]?)?"
            "(?:\\(\\d{3}\\)|\\d{3})"
            "[\\s.-]?\\d{3}[\\s.-]?\\d{4}"
            "(?!\\d)";

        const SsnPattern = "\\b\\d{3}[\\s.-]\\d{2}[\\s.-]\\d{4}\\b";

        #! Credit card with Luhn validation (post-regex)
        const CreditCardPattern =
            "\\b(?:4\\d{3}|5[1-5]\\d{2}|3[47]\\d{2}|6(?:011|5\\d{2}))"
            "[\\s.-]?\\d{4}[\\s.-]?\\d{4}[\\s.-]?\\d{1,4}\\b";

        #! IBAN: 2-letter country + 2 check digits + up to 30 alphanumeric
        const IbanPattern =
            "\\b[A-Z]{2}\\d{2}[\\s-]?[A-Z0-9]{4}(?:[\\s-]?[A-Z0-9]{4}){1,7}"
            "(?:[\\s-]?[A-Z0-9]{1,4})?\\b";

        #! IBAN country-specific lengths for validation
        const IbanLengths = {
            "AD": 24, "AE": 23, "AL": 28, "AT": 20, "AZ": 28, "BA": 20,
            "BE": 16, "BG": 22, "BH": 22, "BR": 29, "BY": 28, "CH": 21,
            "CR": 22, "CY": 28, "CZ": 24, "DE": 22, "DK": 18, "DO": 28,
            "EE": 20, "EG": 29, "ES": 24, "FI": 18, "FO": 18, "FR": 27,
            "GB": 22, "GE": 22, "GI": 23, "GL": 18, "GR": 27, "GT": 28,
            "HR": 21, "HU": 28, "IE": 22, "IL": 23, "IQ": 23, "IS": 26,
            "IT": 27, "JO": 30, "KW": 30, "KZ": 20, "LB": 28, "LC": 32,
            "LI": 21, "LT": 20, "LU": 20, "LV": 21, "MC": 27, "MD": 24,
            "ME": 22, "MK": 19, "MR": 27, "MT": 31, "MU": 30, "NL": 18,
            "NO": 15, "PK": 24, "PL": 28, "PS": 29, "PT": 25, "QA": 29,
            "RO": 24, "RS": 22, "SA": 24, "SC": 31, "SE": 24, "SI": 19,
            "SK": 24, "SM": 27, "ST": 25, "SV": 28, "TL": 23, "TN": 24,
            "TR": 26, "UA": 29, "VA": 22, "VG": 24, "XK": 20,
        };

        #! International phone: +CC followed by 7-15 digits
        const PhoneIntlPattern =
            "(?:(?<=\\s)|(?<=^))"
            "\\+(?:44|33|49|34|39|31|46|47|48|43|41|32|353|351|358|852|86|81|82|91|61|64|65)"
            "[\\s.-]?(?:\\d[\\s.-]?){6,14}\\d"
            "(?!\\d)";

        #! UK National Insurance Number
        const UkNinPattern =
            "\\b(?!BG|GB|NK|KN|TN|NT|ZZ)"
            "[A-CEGHJ-PR-TW-Z][A-CEGHJ-NPR-TW-Z]"
            "[\\s-]?\\d{2}[\\s-]?\\d{2}[\\s-]?\\d{2}[\\s-]?[A-D]\\b";
    }

    private {
        #! Action to take
        string action;
        #! Which PII types to detect
        hash<string, bool> enabled_types;
        #! Custom PII patterns: list of {name, pattern, *validator}
        list<hash<auto>> custom_patterns;
    }

    #! Creates a PII detector
    /** @param severity violation severity level
        @param params optional parameters:
            - \c action (string): "block", "redact", or "warn" (default "block")
            - \c types (list<string>): PII types to detect (default all built-in types)
            - \c custom_patterns (list<hash>): custom patterns, each with:
              - \c name (string): unique PII type name
              - \c pattern (string): regex pattern
              - \c validator (code): optional \c code<bool(string)> post-match validator
    */
    constructor(string severity = GS_CRITICAL, *hash<auto> params)
            : AbstractGuardrailRule(severity) {
        action = params.action ?? ACTION_BLOCK;
        if (action != ACTION_BLOCK && action != ACTION_REDACT && action != ACTION_WARN) {
            throw "GUARDRAIL-CONFIG-ERROR",
                sprintf("invalid PII action %y; expected block, redact, or warn", action);
        }

        # Default: detect all built-in types
        enabled_types = {
            PII_EMAIL: True,
            PII_PHONE: True,
            PII_SSN: True,
            PII_CREDIT_CARD: True,
            PII_IBAN: True,
            PII_PHONE_INTL: True,
            PII_UK_NIN: True,
        };

        # Register custom patterns and add them to enabled_types
        custom_patterns = params.custom_patterns ?? ();
        foreach hash<auto> cp in (custom_patterns) {
            enabled_types{cp.name} = True;
        }

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

    #! Returns the DataProvider-option schema for this rule's \c params hash
    /** UI renderers can use this schema to build a typed editor form for a
        PII detection rule without hard-coding knowledge of the PII detector's
        internals.

        @since QoreGuardrails 1.1
    */
    public static hash<string, hash<DataProviderOptionInfo>> getConfigSchema() {
        return {
            "action": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(StringType),),
                "display_name": "Action on Detection",
                "short_desc": "What to do when PII is detected in the text",
                "desc": "Controls how the rule reacts when any configured PII "
                    "pattern matches.  **Redact** (default) replaces the "
                    "matching text with a placeholder and lets the request "
                    "continue; **Block** rejects the whole request; **Warn** "
                    "records the detection but lets the request through "
                    "unchanged.",
                "default_value": "block",
                "required": False,
                "preselected": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "block",
                        "display_name": "Block the request (default)",
                        "short_desc": "Reject the request with a guardrail violation.",
                        "desc": "The detection is reported as a violation and the "
                            "request is blocked.  Use when any appearance of PII "
                            "is a policy failure.",
                    },
                    <AllowedValueInfo>{
                        "value": "redact",
                        "display_name": "Redact the match",
                        "short_desc": "Replace the matching text with a placeholder "
                            "and let the request continue.",
                        "desc": "The sensitive value is replaced with a "
                            "`[PII_<type>_REDACTED]`-style placeholder and the "
                            "request continues as if the user had written the "
                            "redacted form themselves.  Safe default for most "
                            "public-facing endpoints.",
                    },
                    <AllowedValueInfo>{
                        "value": "warn",
                        "display_name": "Log only",
                        "short_desc": "Record the detection but let the request "
                            "through unchanged.",
                        "desc": "The violation is written to the guardrail audit "
                            "log but the original text (with PII intact) "
                            "reaches the LLM.  Useful during pilot rollout when "
                            "you want visibility without behaviour change.",
                    },
                ),
            },
            "types": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(AutoListOrNothingType),),
                "display_name": "Enabled PII Types",
                "short_desc": "Which PII types to detect (default: all built-in types)",
                "desc": "List of PII type names to enable for this rule.  When "
                    "empty, every built-in type is enabled: `email`, `phone`, "
                    "`ssn`, `credit_card`, `iban`, `phone_intl`, `uk_nin`.  "
                    "Supply an explicit list to narrow the rule to a specific "
                    "subset — e.g. `[\"email\", \"phone\"]` for endpoints that "
                    "only need basic contact-info redaction.  Custom pattern "
                    "names added via `custom_patterns` are always enabled "
                    "regardless of this list.",
                "required": False,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "email",
                        "display_name": "Email addresses",
                        "desc": "Standard RFC 5322-like email addresses.",
                    },
                    <AllowedValueInfo>{
                        "value": "phone",
                        "display_name": "US phone numbers",
                        "desc": "North-American-format phone numbers with word-"
                            "boundary enforcement to reduce false positives on "
                            "order / product / invoice numbers.",
                    },
                    <AllowedValueInfo>{
                        "value": "ssn",
                        "display_name": "US Social Security Numbers",
                        "desc": "Nine-digit US SSNs in `xxx-xx-xxxx` format.",
                    },
                    <AllowedValueInfo>{
                        "value": "credit_card",
                        "display_name": "Credit card numbers",
                        "desc": "Visa, MasterCard, AmEx, Discover — validated "
                            "with the Luhn checksum to reduce false positives.",
                    },
                    <AllowedValueInfo>{
                        "value": "iban",
                        "display_name": "IBAN bank account numbers",
                        "desc": "International Bank Account Numbers, validated "
                            "with the mod-97 checksum.",
                    },
                    <AllowedValueInfo>{
                        "value": "phone_intl",
                        "display_name": "International phone numbers",
                        "desc": "Phone numbers with `+<country-code>` prefix.",
                    },
                    <AllowedValueInfo>{
                        "value": "uk_nin",
                        "display_name": "UK National Insurance Numbers",
                        "desc": "UK NINO format with checksum letter.",
                    },
                ),
                "allowed_values_creatable": False,
                "element_allowed_values_creatable": True,
            },
            "custom_patterns": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(AutoListOrNothingType),),
                "display_name": "Custom Patterns",
                "short_desc": "Additional regex patterns to detect as PII",
                "desc": "List of custom PII patterns to detect in addition to "
                    "(or instead of) the built-in types.  Each entry is a "
                    "hash with `name` (unique type name), `pattern` (a regex), "
                    "and optional `validator` (a `code<bool(string)>` "
                    "post-match validator, e.g. for checksum validation).  "
                    "Use this to add business-specific identifiers like "
                    "employee numbers or customer IDs.",
                "required": False,
                "example_value": (
                    {
                        "name": "employee_id",
                        "pattern": "EMP-\\d{6}",
                    },
                ),
            },
        };
    }

    hash<GuardrailResult> evaluate(string text, *hash<auto> ctx) {
        date start = now_us();
        list<hash<auto>> findings = ();

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
            # Credit card with Luhn validation
            list<hash<auto>> cc_raw = detectPattern(text, CreditCardPattern,
                PII_CREDIT_CARD);
            foreach hash<auto> f in (cc_raw) {
                if (luhnCheck(f.matched)) {
                    push findings, f;
                }
            }
        }
        if (enabled_types{PII_IBAN}) {
            # IBAN with mod-97 checksum
            list<hash<auto>> iban_raw = detectPattern(text, IbanPattern, PII_IBAN);
            foreach hash<auto> f in (iban_raw) {
                if (ibanCheck(f.matched)) {
                    push findings, f;
                }
            }
        }
        if (enabled_types{PII_PHONE_INTL}) {
            findings += detectPattern(text, PhoneIntlPattern, PII_PHONE_INTL);
        }
        if (enabled_types{PII_UK_NIN}) {
            findings += detectPattern(text, UkNinPattern, PII_UK_NIN);
        }

        # Custom patterns
        foreach hash<auto> cp in (custom_patterns) {
            if (!enabled_types{cp.name}) {
                continue;
            }
            list<hash<auto>> raw = detectPattern(text, cp.pattern, cp.name);
            if (cp.validator) {
                foreach hash<auto> f in (raw) {
                    if (cp.validator(f.matched)) {
                        push findings, f;
                    }
                }
            } else {
                findings += raw;
            }
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
            string redacted = text;
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
            int pos = index(remaining, match[0]);
            if (pos >= 0) {
                remaining = remaining.substr(pos + match[0].length());
            } else {
                break;
            }
        }
        return findings;
    }

    #! Validates a credit card number using the Luhn algorithm
    /** @param number the credit card number (may contain separators)
        @return True if the number passes the Luhn checksum
    */
    private bool luhnCheck(string number) {
        string digits = regex_subst(number, "[^0-9]", "", RE_Global);
        if (digits.length() < 13 || digits.length() > 19) {
            return False;
        }
        int sum = 0;
        bool double_it = False;
        for (int i = digits.length() - 1; i >= 0; --i) {
            int d = ord(digits[i]) - ord("0");
            if (double_it) {
                d *= 2;
                if (d > 9) {
                    d -= 9;
                }
            }
            sum += d;
            double_it = !double_it;
        }
        return (sum % 10) == 0;
    }

    #! Validates an IBAN using the mod-97 checksum
    /** @param iban the IBAN string (may contain spaces/dashes)
        @return True if the IBAN passes the mod-97 checksum
    */
    private bool ibanCheck(string iban) {
        string clean = regex_subst(iban, "[\\s-]", "", RE_Global);
        if (clean.length() < 5) {
            return False;
        }
        # Validate country-specific length
        string country = clean.substr(0, 2);
        *int expected_len = IbanLengths{country};
        if (exists expected_len && clean.length() != expected_len) {
            return False;
        }
        # Move first 4 chars to end
        string rearranged = clean.substr(4) + clean.substr(0, 4);
        # Convert letters to numbers (A=10, B=11, ... Z=35)
        string num_str = "";
        for (int i = 0; i < rearranged.length(); ++i) {
            int c = ord(rearranged[i]);
            if (c >= ord("A") && c <= ord("Z")) {
                num_str += sprintf("%d", c - ord("A") + 10);
            } else {
                num_str += chr(c);
            }
        }
        # Compute mod 97 using iterative approach for large numbers
        int remainder = 0;
        for (int i = 0; i < num_str.length(); ++i) {
            remainder = (remainder * 10 + (ord(num_str[i]) - ord("0"))) % 97;
        }
        return remainder == 1;
    }

    #! Escapes special regex characters in a string for literal matching
    private string regex_escape(string s) {
        return regex_subst(s, "([\\\\\\[\\](){}.*+?^$|])", "\\\\$1", RE_Global);
    }
}

} # namespace QoreGuardrails
