# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file LlmToolSchema.qc JSON Schema *builders* for LLM tool definitions
/** Distinct concern from the @c Qore::Json::JsonSchema class shipped by
    the binary \c json module — that is a JSON Schema *validator*; this
    file provides *builders* that translate Qore-native parameter
    descriptors and Qore type names into JSON Schema documents ready for
    @ref ToolDefinition::parameters.  Callers that want validation of the
    output may layer \c Qore::Json::JsonSchema on top (it needs to be
    loaded via the \c \%try-module pattern since the \c json module is
    optional in some installs).
*/

/*  LlmToolSchema.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Provider-neutral LLM utilities namespace (shared with QoreLlmTypes)
public namespace QoreLlmUtils {

#! Builds a JSON Schema object document from a hash of Qore-typed param descriptors
/** Converts a Qore-native hash of \c name -> descriptor (the convention
    used by Qonsole command catalogs, OpenAPI op signatures, and any other
    Qore tool source that describes parameters in Qore terms) into the
    JSON Schema shape required for @ref ToolDefinition::parameters:

    @verbatim
{
    "type": "object",
    "properties": {
        "<name>": { ...json-schema-property... },
        ...
    },
    "required": [ "<name>", ... ]
}
    @endverbatim

    Use this when populating @ref ToolDefinition::parameters from any
    Qore-native source.  The documented contract of
    @ref ToolDefinition::parameters is that it MUST already be valid JSON
    Schema — providers (OpenAI, Anthropic, Gemini, …) ship it as-is to the
    upstream LLM API and have no opportunity to clean it up.  Sending raw
    Qore-typed descriptors instead causes provider-side HTTP 400s on
    every tool-calling chat (Gemini is strictest; OpenAI usually rejects too).

    @par Recognised descriptor fields
    Each descriptor (the value of \a params) may carry:

    - \c type — Qore type name (\c "string", \c "*string", \c "int",
      \c "*int", \c "softint", \c "bool", \c "*bool", \c "float",
      \c "*float", \c "number", \c "*number", \c "date", \c "*date",
      \c "binary", \c "*binary", \c "hash", \c "*hash", \c "list",
      \c "*list", \c "list<T>", \c "*list<T>", \c "softlist<T>",
      \c "auto", \c "any").  Unrecognised types fall back to \c "string".
      A leading \c "*" denotes a Qore nullable type and is reflected as
      JSON Schema \c "nullable":\c true (OpenAPI 3 / Gemini convention).
    - \c required (bool) — when @ref True, the property's name is added
      to the parent-level \c "required" array.  Per-property \c "required"
      is dropped because JSON Schema does not place \c "required" on
      individual property schemas — it lives on the parent object.
    - \c allowed_values (list) — translated to JSON Schema \c "enum".
    - \c default_value (any) — translated to JSON Schema \c "default".
    - \c description / \c desc / \c short_desc — used as the property's
      JSON Schema \c "description" (first non-empty key wins).

    Unrecognised descriptor fields (\c positional, \c position, Qore-CLI
    metadata, etc.) are silently dropped — they are not part of JSON Schema
    and would cause provider-side validation errors if they reached the wire.

    @param params hash of param-name -> descriptor; may be @ref nothing or
        empty (returns the minimal \c {type:"object", properties:{}} shape)

    @return a JSON Schema object hash ready for use as
        @ref ToolDefinition::parameters

    @par Example
    @code{.py}
hash<ToolDefinition> td = <ToolDefinition>{
    "name": "get_weather",
    "description": "Look up the current weather for a city",
    "parameters": QoreLlmUtils::qoreParamsToJsonSchema({
        "city": {
            "type": "string",
            "required": True,
            "description": "City name, e.g. \"Prague\"",
        },
        "units": {
            "type": "*string",
            "allowed_values": ("metric", "imperial"),
            "default_value": "metric",
        },
    }),
};
    @endcode

    @since QoreLlmUtils 1.1
*/
public hash<auto> sub qoreParamsToJsonSchema(*hash<auto> params) {
    hash<auto> properties = {};
    list<string> required = ();
    foreach hash<auto> i in (params.pairIterator()) {
        # Decay any typed hashdecl on the descriptor so unknown-field access
        # below (description, allowed_values, …) cannot raise INVALID-MEMBER.
        hash<auto> desc;
        if (i.value.typeCode() == NT_HASH) {
            desc += i.value;
        }
        properties{i.key} = qoreParamDescriptorToJsonSchemaProperty(desc);
        if (desc.required === True) {
            required += i.key;
        }
    }
    hash<auto> rv = {
        "type": "object",
        "properties": properties,
    };
    if (required) {
        rv.required = required;
    }
    return rv;
}

#! Converts a single Qore parameter descriptor hash into a JSON Schema property
/** Shared by @ref qoreParamsToJsonSchema(); exposed publicly so callers that
    already iterate their descriptors (for example to merge in source-specific
    metadata) can reuse the per-property translation without rebuilding the
    surrounding \c {type:"object", properties:…} envelope.

    @param desc the Qore parameter descriptor hash (may be empty)

    @return a JSON Schema property hash
*/
public hash<auto> sub qoreParamDescriptorToJsonSchemaProperty(hash<auto> desc) {
    hash<auto> prop = qoreTypeNameToJsonSchema(desc.type);
    *string description = desc.description ?? desc.desc ?? desc.short_desc;
    if (description.val()) {
        prop.description = description;
    }
    if (desc.allowed_values.typeCode() == NT_LIST) {
        # `enum` and `default` are reserved words in Qore — quote them as
        # string keys so the parser does not try to interpret them.
        prop{"enum"} = desc.allowed_values;
    }
    if (exists desc.default_value) {
        prop{"default"} = desc.default_value;
    }
    return prop;
}

#! Maps a Qore type name string to a JSON Schema property hash (no description / enum / default)
/** Handles nullable (\c "*T"), soft (\c "softT" / \c "*softT"), parameterised
    list (\c "list<T>" / \c "*list<T>" / \c "softlist<T>" / \c "*softlist<T>"),
    and bare scalar types.  Unrecognised types fall back to
    \c {"type":"string"} so the caller still gets a valid JSON Schema
    property (the LLM may not understand the value, but the request will
    not be rejected at the schema validation step).

    @param qore_type the Qore type name, or @ref nothing for "any"

    @return a JSON Schema property hash; nullable types carry
        \c "nullable":\c true (OpenAPI 3 / Gemini convention)
*/
public hash<auto> sub qoreTypeNameToJsonSchema(*string qore_type) {
    if (!qore_type.val()) {
        return {"type": "string"};
    }
    string t = qore_type;
    bool nullable = False;
    if (t.startsWith("*")) {
        nullable = True;
        t = t.substr(1);
    }
    if (t.startsWith("soft")) {
        t = t.substr(4);
    }
    # hash<auto!> avoids value-type narrowing — the rv built below mixes
    # string ("type"), string ("format"), object ("items"), and bool
    # ("nullable") values, and `hash<auto>` would narrow to string-only.
    hash<auto!> rv;
    *list<*string> list_match = (t =~ x/^list<(.+)>$/);
    if (list_match) {
        rv = {
            "type": "array",
            "items": qoreTypeNameToJsonSchema(list_match[0]),
        };
    } else {
        switch (t) {
            case "string":
                rv = {"type": "string"};
                break;
            case "int":
            case "integer":
                rv = {"type": "integer"};
                break;
            case "float":
            case "number":
            case "double":
                rv = {"type": "number"};
                break;
            case "bool":
            case "boolean":
                rv = {"type": "boolean"};
                break;
            case "date":
                rv = {"type": "string", "format": "date-time"};
                break;
            case "binary":
                rv = {"type": "string", "format": "binary"};
                break;
            case "hash":
                rv = {"type": "object"};
                break;
            case "list":
                # Bare "list" without an inner type — Qore allows this
                # (untyped homogeneous list), but JSON Schema requires
                # `items` to be a schema object (Gemini enforces this
                # strictly, OpenAI accepts the omission silently).  Default
                # to a string-items schema; callers that know the inner
                # element shape should declare it as "list<T>" so it round-
                # trips faithfully.
                rv = {"type": "array", "items": {"type": "string"}};
                break;
            case "auto":
            case "any":
            case "nothing":
                # Permissive — LLMs need a concrete primitive; string is safest
                rv = {"type": "string"};
                break;
            default:
                # Unknown type → fall back to permissive string.  The LLM
                # cannot guess what to put here without a description, but
                # at least the schema is valid and won't be rejected upstream.
                rv = {"type": "string"};
                break;
        }
    }
    if (nullable) {
        rv.nullable = True;
    }
    return rv;
}

#! Sanitises a candidate tool name so it conforms to LLM provider rules
/** Major LLM providers restrict tool/function names to a narrow character
    set; the conservative intersection of OpenAI and Gemini is the regex
    \c "^[a-zA-Z_][a-zA-Z0-9_-]{0,63}$".  Any character outside that set
    causes upstream HTTP 400 on every tool-enabled chat (qorus#230).

    The Qore-side caller may naturally produce names containing path
    separators, dots, or other punctuation (Qonsole command paths, REST
    API actions derived from OpenAPI op IDs, MCP tool identifiers).  Use
    this helper at @ref ToolDefinition construction time so every Qore
    consumer ships compliant names without having to reinvent the
    sanitization rules.

    @par Rules applied
    - Characters outside \c [a-zA-Z0-9_-] are replaced with \c "_".
    - If the first character is not a letter or underscore (for example
      a digit, or a leading replacement underscore that came from
      sanitization of an originally-invalid first char), an underscore
      is prepended.
    - The result is truncated to 64 characters (OpenAI's documented limit;
      Gemini allows 128 but the lower bound is the safe upper).

    @par Collision avoidance
    Sanitization may collapse two distinct inputs to the same output (for
    example \c "foo/bar" and \c "foo.bar" both become \c "foo_bar").
    Callers that need round-trip uniqueness should deduplicate by appending
    a stable suffix (e.g., a short hash of the original name) themselves;
    this helper deliberately performs no allocation tracking so it stays
    stateless and idempotent.

    @param name the raw tool name from the source catalogue
    @param prefix optional prefix to prepend before sanitization (e.g.
        \c "system_" or \c "rest_") — included so the prefix participates
        in the length check and never ends up split across the truncation
        boundary

    @return a sanitized name guaranteed to match the provider rules

    @since QoreLlmUtils 1.1
*/
public string sub sanitizeLlmToolName(string name, *string prefix) {
    string raw = (prefix ?? "") + name;
    # Replace any disallowed character with underscore.  Note: '-' is allowed
    # but must be the last char of the character class to avoid being read
    # as a range.
    string clean = regex_subst(raw, "[^a-zA-Z0-9_-]", "_", RE_Global);
    # The first character must be a letter or underscore — digits and
    # hyphens are not valid leading characters.  Prepend an underscore
    # whenever the leading char doesn't satisfy that constraint (handles
    # empty strings as well — `clean.size() == 0` short-circuits the regex
    # without raising).
    if (clean.size() == 0 || !regex(clean.substr(0, 1), "^[a-zA-Z_]$")) {
        clean = "_" + clean;
    }
    # Fast path: input was already valid and fits — round-trip is a no-op
    # so callers' lookup tables keyed by the raw name keep working without
    # any indirection.
    if (clean == raw && clean.size() <= 64) {
        return clean;
    }
    # Slow path: sanitization rewrote characters or the name needs truncation.
    # Append a deterministic 8-hex-char suffix derived from the *raw* input
    # so two distinct raw names that collapse to the same sanitized base
    # (a frequent collision when several action names share a common prefix
    # past character 55) get distinct final names.  The suffix is a stable
    # MD5 prefix of the raw input — same raw name always yields the same
    # final tool name, which is essential for caching, logging, and audit
    # cross-referencing.
    string suffix = "_" + MD5(raw).substr(0, 8);
    int base_budget = 64 - suffix.size();
    if (clean.size() > base_budget) {
        clean = clean.substr(0, base_budget);
    }
    return clean + suffix;
}

} # namespace QoreLlmUtils
