# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SGLangDataProvider module definition

/** SGLangGenerateRequestDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SGLangDataProvider module
public namespace SGLangDataProvider {
#! Data type for SGLang native `/generate` requests (constrained decoding)
public class SGLangGenerateRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "text": {
                "display_name": "Prompt Text",
                "short_desc": "Raw prompt text (or list of prompts) to complete",
                "desc": "The raw prompt(s) to send to the model.  Can be a single string or a list "
                    "of strings for batch completion.\n\n"
                    "Unlike the chat endpoint, `/generate` does not apply a chat template — the text "
                    "is tokenized and fed directly to the model.  Include any role markers or "
                    "formatting the model expects yourself.\n\n"
                    "**Example**: `\"Extract the invoice number from: Invoice #INV-2026-0041 ...\"`",
                "type": AbstractDataProviderTypeMap."softlist",
                "required": True,
                "example_value": (
                    "Extract the total amount from this invoice as a number: "
                    "Invoice total due: $1,234.56",
                ),
            },
            "input_ids": {
                "display_name": "Input Token IDs",
                "short_desc": "Pre-tokenized input as a list of integer token IDs",
                "desc": "Alternative to `text` — pass a pre-tokenized list of integer token IDs "
                    "directly.  Useful when you've already run the tokenizer yourself and want to "
                    "skip re-tokenization.  Provide exactly one of `text` or `input_ids`.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "sampling_params": {
                "display_name": "Sampling Parameters",
                "short_desc": "Sampling configuration (temperature, max_new_tokens, constraints)",
                "desc": "Hash of sampling parameters controlling how the model generates tokens.\n\n"
                    "**Decoding controls**:\n"
                    "- `temperature` — sampling temperature (`0.0` = deterministic, `1.0` = default)\n"
                    "- `top_p` — nucleus sampling cutoff (`0.0`-`1.0`)\n"
                    "- `top_k` — sample only from the K most likely tokens\n"
                    "- `max_new_tokens` — hard cap on generated tokens\n"
                    "- `stop` — string or list of stop sequences\n"
                    "- `n` — number of completions to return\n\n"
                    "**Constraint modes** (mutually exclusive — pick at most one):\n"
                    "- `json_schema` — force output to match a JSON schema (passed as a JSON string)\n"
                    "- `regex` — force output to match a regular expression\n"
                    "- `ebnf` — force output to match an EBNF grammar\n"
                    "- `structural_tag` — constrain output with XML-like structural tags\n\n"
                    "Constraints are enforced at the token-sampling step — the model cannot emit "
                    "tokens that would violate the constraint, so no retry loop is needed.\n\n"
                    "The convenience fields `temperature`, `max_new_tokens`, `json_schema`, `regex`, "
                    "and `ebnf` at the top level of the request are automatically merged into "
                    "`sampling_params` by the provider, so you usually don't need to build this hash "
                    "manually.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "temperature": {
                "display_name": "Temperature",
                "short_desc": "Sampling temperature (merged into sampling_params)",
                "desc": "Convenience field merged into `sampling_params.temperature`.\n\n"
                    "- `0.0` — deterministic (greedy decoding; recommended for extraction tasks)\n"
                    "- `0.7` — balanced\n"
                    "- `1.0`+ — more varied output",
                "type": AbstractDataProviderTypeMap."*softfloat",
                "example_value": 0.0,
            },
            "max_new_tokens": {
                "display_name": "Max New Tokens",
                "short_desc": "Maximum tokens to emit (merged into sampling_params)",
                "desc": "Convenience field merged into `sampling_params.max_new_tokens`.  Maximum "
                    "number of tokens to generate.  Increase for long-form output; decrease to cap "
                    "cost.",
                "type": AbstractDataProviderTypeMap."*softint",
                "example_value": 256,
            },
            "json_schema": {
                "display_name": "JSON Schema Constraint",
                "short_desc": "JSON schema (as a string) the output must conform to",
                "desc": "Convenience field merged into `sampling_params.json_schema`.\n\n"
                    "A JSON schema, serialized as a string, that the output is guaranteed to match. "
                    "SGLang enforces the schema at the token-sampling level, so the output is "
                    "**always** valid JSON conforming to the schema — no parse errors, no retries.\n\n"
                    "**Example** (extract an invoice total):\n"
                    "```\n"
                    "{\n"
                    "  \"type\": \"object\",\n"
                    "  \"properties\": {\n"
                    "    \"amount\": {\"type\": \"number\"},\n"
                    "    \"currency\": {\"type\": \"string\"}\n"
                    "  },\n"
                    "  \"required\": [\"amount\", \"currency\"]\n"
                    "}\n"
                    "```\n\n"
                    "Mutually exclusive with `regex`, `ebnf`, and `structural_tag`.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "regex": {
                "display_name": "Regex Constraint",
                "short_desc": "Regular expression the output must match",
                "desc": "Convenience field merged into `sampling_params.regex`.\n\n"
                    "A regular expression that the generated output is guaranteed to match.  Useful "
                    "for tight single-value extraction (dates, identifiers, classifications).\n\n"
                    "**Example** (classify as exactly one of three labels):\n"
                    "`\"^(positive|negative|neutral)$\"`\n\n"
                    "Mutually exclusive with `json_schema`, `ebnf`, and `structural_tag`.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "ebnf": {
                "display_name": "EBNF Grammar Constraint",
                "short_desc": "EBNF grammar the output must conform to",
                "desc": "Convenience field merged into `sampling_params.ebnf`.\n\n"
                    "An EBNF grammar that the generated output must conform to.  Useful for "
                    "structured outputs that don't fit naturally into JSON schema (SQL queries, "
                    "domain-specific languages, arithmetic expressions).\n\n"
                    "Mutually exclusive with `json_schema`, `regex`, and `structural_tag`.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "stream": {
                "display_name": "Stream",
                "short_desc": "Stream partial output as server-sent events",
                "desc": "When `True`, the server streams partial output as Server-Sent Events "
                    "instead of returning a single response.  Most action flows expect non-streaming; "
                    "enable only when wiring into a streaming consumer.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
            "return_logprob": {
                "display_name": "Return Log Probabilities",
                "short_desc": "Include per-token log probabilities in the response",
                "desc": "When `True`, the response's `meta_info` includes per-token log "
                    "probabilities.  Useful for calibration and confidence estimation; adds response "
                    "size.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
            "logprob_start_len": {
                "display_name": "Log Probability Start Length",
                "short_desc": "Token offset at which log-probability reporting begins",
                "desc": "Token offset at which log-probability reporting begins.  Use `-1` to report "
                    "from the first generated token onward (typical).  Has no effect unless "
                    "`return_logprob` is `True`.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "top_logprobs_num": {
                "display_name": "Top Log Probabilities Count",
                "short_desc": "Number of alternative tokens to include in each logprob entry",
                "desc": "For each generated token, include the top N alternative tokens and their "
                    "log probabilities.  Has no effect unless `return_logprob` is `True`.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "return_text_in_logprobs": {
                "display_name": "Return Text in Log Probabilities",
                "short_desc": "Include decoded token text alongside each logprob",
                "desc": "When `True`, each log-probability entry includes the decoded token text in "
                    "addition to the token ID.  Easier to inspect but larger payload.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
