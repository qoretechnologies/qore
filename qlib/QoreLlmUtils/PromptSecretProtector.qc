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

    private {
        #! Versioned machine contract returned by buildSystemContext()
        const SystemContextContractId =
            "qore.llm.protected_values.v1";
        #! Maximum unique values protected in one request
        const MaxProtectedCandidates = 64;

        #! Exact canonical machine keys whose values are sensitive
        /** These are protocol identifiers, not natural-language aliases.
            Matching is deliberately case- and separator-sensitive so
            translated words, cognates, and user-defined labels remain opaque.
        */
        const SensitiveMachineKeys = {
            "api-key": True,
            "api_key": True,
            "apikey": True,
            "access-token": True,
            "access_token": True,
            "bearer-token": True,
            "bearer_token": True,
            "refresh-token": True,
            "refresh_token": True,
            "token": True,
            "pass": True,
            "password": True,
            "passphrase": True,
            "authorization": True,
            "credential": True,
            "client-secret": True,
            "client_secret": True,
            "secret": True,
            "private-key": True,
            "private_key": True,
            "sensitive": True,
            "sensitive-data": True,
            "sensitive_data": True,
            "svalue": True,
        };
    }

    #! Replace credential-like values in \a input with request-local placeholders
    /** @param input prompt text to mutate in place
        @param options optional placeholder and redaction configuration

        @return a placeholder hash mapping placeholder strings to raw values,
            or @ref nothing when no credential-like values were found

        @throw LLM-SECRET-PROTECTION-ERROR when one request contains more
            unique protected values than the bounded placeholder contract

        @par Language-independent input forms
        Any Unicode label can mark a protected value with the structural
        \c := delimiter, for example \c contraseña:=valor or
        \c 認証情報:="値".  Values assigned to exact canonical sensitive
        machine keys and URI userinfo passwords are also protected.  Natural
        language words are never classified as sensitive.
    */
    static *hash<auto> protectText(reference<string> input,
            *hash<PromptSecretProtectionOptions> options) {
        int min_value_length = options.min_value_length ?? 8;
        if (min_value_length < 1) {
            min_value_length = 1;
        }
        list<string> candidates =
            collectProtectedCandidates(input, min_value_length);
        if (!candidates) {
            return NOTHING;
        }

        string prefix = normalizePlaceholderPrefix(
            options.placeholder_prefix ?? DefaultPlaceholderPrefix);
        string safe_input = input;
        hash<auto!> secrets = {};
        hash<string, bool> seen_values = {};
        int idx = 0;
        foreach string secret in (candidates) {
            if (seen_values{secret}) {
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
        return make_json({
            "contract_id": SystemContextContractId,
            "placeholders": names,
            "operation_description":
                options.operation_description ?? "current_operation",
            "value_semantics": "opaque_sensitive_value",
            "allowed_use": "exact_execution_argument",
            "restore_boundary": "application_execution",
            "forbidden_uses": (
                "print",
                "reveal",
                "summarize",
                "transform",
                "persist",
                "request_reentry",
            ),
        }, JGF_ADD_FORMATTING);
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

    #! Return @ref True for an exact canonical sensitive machine key
    /** @param key hash key or option name to inspect without normalization

        @return @ref True only when \a key is an exact closed protocol key

        @note Case, separator, translated, and cognate variants are not
            aliases.  Callers with schema metadata should use its explicit
            sensitive flag instead of inferring sensitivity from labels.
    */
    static bool isSensitiveKey(string key) {
        return SensitiveMachineKeys{key} ?? False;
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
        *hash<auto> placeholders;
        try {
            placeholders = protectText(\rv);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "LLM-SECRET-PROTECTION-ERROR") {
                return marker;
            }
            rethrow;
        }
        if (placeholders) {
            rv = redactPlaceholders(rv, placeholders, marker);
        }
        return rv;
    }

    #! Collect exact values protected by structural, language-neutral rules
    private static list<string> collectProtectedCandidates(string input,
            int min_value_length) {
        list<string> rv = ();
        hash<string, bool> seen = {};

        # Any-language explicit protected binding.  The delimiter, rather
        # than the label, carries sensitivity.
        addCandidateMatches(\rv, \seen,
            input =~ x/:=\s*"([^"\r\n]+)"/g, min_value_length);
        addCandidateMatches(\rv, \seen,
            input =~ x/:=\s*'([^'\r\n]+)'/g, min_value_length);
        addCandidateMatches(\rv, \seen,
            input =~ x/:=\s*(?!["'])([^\s,;}\]]+)/g,
            min_value_length);

        # Exact canonical machine-key assignments.  Capturing is structural;
        # key classification uses the closed, case-sensitive set above.
        *list<string> assignments = input =~
            x/(?:^|[\s,{;])["']?([A-Za-z][A-Za-z0-9_-]*)["']?\s*(?:=|:)\s*([^,\r\n;}]+)/g;
        if (assignments) {
            for (int i = 0; i + 1 < assignments.size(); i += 2) {
                if (isSensitiveKey(assignments[i])) {
                    addCandidate(\rv, \seen, assignments[i + 1],
                        min_value_length);
                }
            }
        }

        # URI userinfo has a standard structural password position.
        addCandidateMatches(\rv, \seen,
            input =~ x/[A-Za-z][A-Za-z0-9+.-]*:\/\/[^:\/?#\s]+:([^@\/?#\s]+)@/g,
            min_value_length);
        return rv;
    }

    private static addCandidateMatches(reference<list<string>> rv,
            reference<hash<string, bool>> seen, *list<string> matches,
            int min_value_length) {
        foreach string match in (matches ?? ()) {
            addCandidate(\rv, \seen, match, min_value_length);
        }
    }

    private static addCandidate(reference<list<string>> rv,
            reference<hash<string, bool>> seen, string raw,
            int min_value_length) {
        string candidate = trim(raw);
        if (candidate.length() >= 2) {
            string first = candidate.substr(0, 1);
            string last = candidate.substr(candidate.length() - 1, 1);
            if ((first == "\"" && last == "\"")
                    || (first == "'" && last == "'")) {
                candidate = candidate.substr(1,
                    candidate.length() - 2);
            }
        }
        candidate = trim(candidate);
        if (candidate.length() < min_value_length
                || candidate == DefaultRedactionText
                || candidate =~ /^<credential-[0-9]+>$/
                || seen{candidate}) {
            return;
        }
        if (rv.size() >= MaxProtectedCandidates) {
            throw "LLM-SECRET-PROTECTION-ERROR", make_json({
                "contract_id": SystemContextContractId,
                "constraint": "max_protected_values",
                "maximum": MaxProtectedCandidates,
            });
        }
        seen{candidate} = True;
        rv += candidate;
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
