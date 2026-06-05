# -*- mode: qore; indent-tabs-mode: nil -*-

/*  PromptSecretProtector.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see PromptSecretProtector
public namespace QoreLlmUtils {

#! Options for @ref PromptSecretProtector
public hashdecl PromptSecretProtectionOptions {
    #! Prefix used in generated placeholder names; default \c "credential"
    string placeholder_prefix = "credential";
    #! Text used when redacting placeholders; default \c "<redacted>"
    string redaction_text = "<redacted>";
    #! Minimum length for detected credential values; default 8
    int min_value_length = 8;
    #! Optional description inserted into the generated LLM system context
    *string operation_description;
}

#! Generic prompt-secret placeholder utilities for LLM applications
/** This class keeps raw user-supplied credentials out of model-visible
    prompts while preserving enough placeholder information for a tool-calling
    agent to place values into the correct tool arguments.  It deliberately
    does not decide whether a request is authorized or operational; callers
    must make that policy decision before calling @ref protectText().
*/
public class PromptSecretProtector {
    public {
        const DefaultPlaceholderPrefix = "credential";
        const DefaultRedactionText = "<redacted>";
    }

    #! Replace credential-like values in \a input with request-local placeholders
    /** @param input prompt text to mutate in place
        @param options optional placeholder and redaction configuration

        @return a placeholder hash mapping placeholder strings to raw values,
            or @ref nothing when no credential-like values were found
    */
    static *hash<auto> protectText(reference<string> input,
            *hash<PromptSecretProtectionOptions> options) {
        *list<string> matches = (input =~ x/\b(api[\s_-]*key|apikey|access[\s_-]*token|bearer[\s_-]*token|refresh[\s_-]*token|token|password|passphrase|client[\s_-]*secret|secret|private[\s_-]*key)\b\s*(?:is|=|:)?\s*([A-Za-z0-9_\-\.\/+=]{4,})/ig);
        if (!matches) {
            return NOTHING;
        }

        int min_value_length = options.min_value_length ?? 8;
        if (min_value_length < 1) {
            min_value_length = 1;
        }
        string prefix = normalizePlaceholderPrefix(
            options.placeholder_prefix ?? DefaultPlaceholderPrefix);
        string safe_input = input;
        hash<auto!> secrets = {};
        hash<string, bool> seen_values = {};
        int idx = 0;
        for (int i = 0; i + 1 < matches.size(); i += 2) {
            string secret = matches[i + 1];
            if (secret.length() < min_value_length || seen_values{secret}) {
                continue;
            }
            seen_values{secret} = True;
            string placeholder;
            do {
                placeholder = sprintf("<%s-%d>", prefix, ++idx);
            } while (safe_input.find(placeholder) >= 0 || secrets{placeholder});
            secrets{placeholder} = secret;
            safe_input = replace(safe_input, secret, placeholder);
        }

        if (!secrets) {
            return NOTHING;
        }

        input = safe_input;
        return secrets;
    }

    #! Build an LLM system message explaining request-local placeholders
    /** @param placeholders placeholder map returned by @ref protectText()
        @param options optional placeholder and system-context configuration

        @return system-context text safe to send to an LLM
    */
    static string buildSystemContext(hash<auto> placeholders,
            *hash<PromptSecretProtectionOptions> options) {
        list<string> names = keys placeholders;
        names = sort(names, int sub (string a, string b) {
            return a <=> b;
        });
        string operation = options.operation_description ?? "the requested operation";
        return sprintf("CURRENT REQUEST CREDENTIAL PLACEHOLDERS: The current "
            "request included %d user-supplied credential value%s. They were "
            "replaced before LLM processing with these placeholders: %s. "
            "Treat these placeholders as opaque credential values supplied by "
            "the user for %s. If a tool or action argument needs a credential, "
            "pass the matching placeholder string exactly; application code "
            "can restore the real value only at the execution boundary. Never "
            "print, reveal, summarize, transform, save to memory, or ask the "
            "user to re-enter these credentials.",
            names.size(), names.size() == 1 ? "" : "s", names.join(", "),
            operation);
    }

    #! Restore request-local placeholders in a value before tool execution
    /** @param value scalar or nested value that may contain placeholder tokens
        @param placeholders placeholder map returned by @ref protectText()

        @return a copy of \a value with placeholders replaced by raw values
    */
    static auto restorePlaceholders(auto value, *hash<auto> placeholders) {
        return replacePlaceholders(value, placeholders, False, NOTHING);
    }

    #! Redact request-local placeholders in a value before logging or display
    /** @param value scalar or nested value that may contain placeholder tokens
        @param placeholders placeholder map returned by @ref protectText()
        @param redaction_text optional text used in place of placeholder tokens

        @return a copy of \a value with placeholders redacted
    */
    static auto redactPlaceholders(auto value, *hash<auto> placeholders,
            *string redaction_text) {
        return replacePlaceholders(value, placeholders, True, redaction_text);
    }

    #! Redact request-local placeholders and their protected raw values
    /** Use this for data that may have crossed an execution boundary where
        placeholders were restored.  It first redacts placeholder tokens and
        then redacts any raw values from the same placeholder map.

        @param value scalar or nested value that may contain placeholders or
            restored raw protected values
        @param placeholders placeholder map returned by @ref protectText()
        @param redaction_text optional text used in place of protected values

        @return a copy of \a value with placeholders and protected raw values
            redacted
    */
    static auto redactProtectedValues(auto value, *hash<auto> placeholders,
            *string redaction_text) {
        auto rv = redactPlaceholders(value, placeholders, redaction_text);
        if (!placeholders) {
            return rv;
        }
        return replaceProtectedRawValues(rv, placeholders,
            redaction_text ?? DefaultRedactionText);
    }

    #! Return @ref True if the key name conventionally contains a secret
    /** @param key hash key or option name to inspect

        @return @ref True when \a key conventionally names a secret value
    */
    static bool isSensitiveKey(string key) {
        return key.lwr() =~ /\b(api[\s_-]*key|apikey|access[\s_-]*token|bearer[\s_-]*token|refresh[\s_-]*token|token|pass|password|passphrase|authorization|credential|client[\s_-]*secret|secret|private[\s_-]*key|sensitive(?:_data)?|svalue)\b/;
    }

    #! Recursively redact sensitive keyed values and credential phrases
    /** @param value scalar or nested value to redact
        @param redaction_text optional text used in place of sensitive values

        @return a copy of \a value with sensitive values redacted
    */
    static auto redactSensitiveValue(auto value, *string redaction_text) {
        switch (value.typeCode()) {
            case NT_STRING:
                return redactSensitiveString(value, redaction_text);
            case NT_HASH: {
                hash<auto!> rv = {};
                foreach hash<auto> pair in (
                        cast<hash>(value).pairIterator()) {
                    rv{pair.key} = isSensitiveKey(pair.key)
                        ? (redaction_text ?? DefaultRedactionText)
                        : redactSensitiveValue(pair.value, redaction_text);
                }
                return rv;
            }
            case NT_LIST: {
                list<auto> rv = ();
                foreach auto item in (cast<list>(value)) {
                    rv += redactSensitiveValue(item, redaction_text);
                }
                return rv;
            }
        }
        return value;
    }

    #! Redact credential phrases in free-form text
    /** @param value text that may contain credential phrases
        @param redaction_text optional text used in place of sensitive values

        @return redacted text
    */
    static string redactSensitiveString(string value, *string redaction_text) {
        string marker = redaction_text ?? DefaultRedactionText;
        string rv = value;
        rv =~ s/((?:"?(?:api[\s_-]*key|apikey|access[\s_-]*token|bearer[\s_-]*token|refresh[\s_-]*token|token|pass|password|passphrase|authorization|credential|client[\s_-]*secret|secret|private[\s_-]*key|sensitive(?:_data)?|svalue)"?\s*[=:>]\s*"?))[^",}\s]+("?)/$1<redacted>$2/ig;
        rv =~ s/\b((?:api[\s_-]*key|apikey|access[\s_-]*token|bearer[\s_-]*token|refresh[\s_-]*token|token|password|passphrase|authorization|credential|client[\s_-]*secret|secret|private[\s_-]*key|sensitive(?:_data)?|svalue)\b\s*(?:is|=|:)?\s*)([A-Za-z0-9_\-.\/+=]{8,})/$1<redacted>/ig;
        rv =~ s/^([a-z][a-z0-9+.-]*:\/\/[^:\/?#\s]+:)[^@\/?#\s]+(@)/$1<redacted>$2/i;
        if (marker != DefaultRedactionText) {
            rv = replace(rv, DefaultRedactionText, marker);
            rv = replace(rv, "<redacted>", marker);
        }
        return rv;
    }

    private static auto replacePlaceholders(auto value,
            *hash<auto> placeholders, bool redact, *string redaction_text) {
        if (!placeholders) {
            return value;
        }

        switch (value.typeCode()) {
            case NT_STRING: {
                string rv = value;
                string marker = redaction_text ?? DefaultRedactionText;
                foreach hash<auto> pair in (placeholders.pairIterator()) {
                    rv = replace(rv, pair.key,
                        redact ? marker : string(pair.value));
                }
                return rv;
            }
            case NT_HASH: {
                hash<auto!> rv = {};
                foreach hash<auto> pair in (
                        cast<hash>(value).pairIterator()) {
                    rv{pair.key} = replacePlaceholders(pair.value,
                        placeholders, redact, redaction_text);
                }
                return rv;
            }
            case NT_LIST: {
                list<auto> rv = ();
                foreach auto item in (cast<list>(value)) {
                    rv += replacePlaceholders(item, placeholders, redact,
                        redaction_text);
                }
                return rv;
            }
        }
        return value;
    }

    private static auto replaceProtectedRawValues(auto value,
            hash<auto> placeholders, string marker) {
        switch (value.typeCode()) {
            case NT_STRING: {
                string rv = value;
                foreach hash<auto> pair in (placeholders.pairIterator()) {
                    string secret = string(pair.value);
                    if (secret.val()) {
                        rv = replace(rv, secret, marker);
                    }
                }
                return redactSensitiveString(rv, marker);
            }
            case NT_HASH: {
                hash<auto!> rv = {};
                foreach hash<auto> pair in (
                        cast<hash>(value).pairIterator()) {
                    rv{pair.key} = isSensitiveKey(pair.key)
                        ? marker
                        : replaceProtectedRawValues(pair.value,
                            placeholders, marker);
                }
                return rv;
            }
            case NT_LIST: {
                list<auto> rv = ();
                foreach auto item in (cast<list>(value)) {
                    rv += replaceProtectedRawValues(item, placeholders,
                        marker);
                }
                return rv;
            }
        }
        return value;
    }

    private static string normalizePlaceholderPrefix(string prefix) {
        string rv = prefix.lwr();
        rv =~ s/[^a-z0-9_-]+/-/g;
        rv =~ s/^-+//;
        rv =~ s/-+$//;
        return rv.val() ? rv : DefaultPlaceholderPrefix;
    }
}

} # namespace QoreLlmUtils
