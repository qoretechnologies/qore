# DPQL Integration Guide

This guide is for host applications (such as Qorus) that embed DPQL — the Data Provider
Query Language. It covers callback registration, session lifecycle, field metadata
construction, WebSocket exposure, and expression evaluation at runtime.

For the end-user syntax reference (fields, operators, values, template references), see
[DPQL Syntax Reference](dpql-syntax.md). For the high-level session architecture and action
patterns, see [Action-Based Session Services](action-session-services.md).

## Overview

DPQL integration is organized into three tiers of increasing complexity:

| Tier | Classes | Use Case |
|------|---------|----------|
| Static API | `DataProvider::` static methods | Stateless parse / tokenize / validate / complete / serialize |
| Session API | `DpqlActionSession` | Stateful sessions with provider context and field metadata |
| WebSocket API | `DpqlWebSocketHandler` + `DpqlWebSocketConnection` | Browser and remote clients |

**Static API** — parse a DPQL string into an expression tree, tokenize it for syntax
highlighting, validate it against a field schema, get completions for an editor, or
round-trip an expression back to text. No provider context required; field metadata is
passed explicitly.

**Session API** — wraps the static API with persistent state (provider, field schema,
expressions). A single `handleAction()` entry point dispatches named actions
(`dpql-parse`, `dpql-get-completions`, etc.) and returns structured response hashes.
Ideal for server-side integration where the transport is already handled.

**WebSocket API** — a ready-made `WebSocketHandler` subclass that creates
`DpqlActionSession` instances per connection, translates JSON messages into actions, and
sends JSON responses back over the socket. Suitable for browser-based DPQL editors.

## Callback Registration

### Template Callbacks

Template callbacks resolve `$context:value` references at runtime. Register them once at
application startup via `AbstractDataProvider::setTemplateCallbacks()`:

```qore
static bool setTemplateCallbacks(
    code<auto(string, string, *string, *hash<auto>)> expand,
    *code<*AbstractDataProviderType(string, string, *hash<auto>)> resolve_type,
    *code<auto(*hash<auto>)> list_contexts,
    *code<auto(string, string, *hash<auto>)> list_values,
);
```

| Parameter | Signature | Purpose |
|-----------|-----------|---------|
| `expand` | `auto(string tmpl_context, string tmpl_value, *string type_assertion, *hash<auto> template_context)` | Resolve a template reference to a concrete value |
| `resolve_type` | `*AbstractDataProviderType(string tmpl_context, string tmpl_value, *hash<auto> template_context)` | Return the type of a template reference (for validation) |
| `list_contexts` | `auto(*hash<auto> template_context)` | List available template contexts (for completions) |
| `list_values` | `auto(string context, string prefix, *hash<auto> template_context)` | List values within a context matching a prefix |

All callbacks receive an optional `template_context` hash that is threaded through from
the query context (see [Expression Evaluation](#expression-evaluation)).

**Locking semantics**: The first call locks the callbacks. Subsequent calls succeed only
if the passed closures are identity-equal to the already-registered ones (returns `True`),
otherwise they return `False`. This prevents accidental re-registration from a different
component. Use `clearTemplateCallbacks()` to unlock and remove all callbacks (intended for
testing).

**Thread safety**: The lock is a simple boolean flag with no mutex. Designed for a single
call at process startup before any concurrent access.

**Convenience delegate**: `DataProvider::setTemplateCallbacks()` forwards to the
`AbstractDataProvider` static method and has the same signature.

```qore
# Example: register template callbacks at startup
DataProvider::setTemplateCallbacks(
    # expand: resolve a template reference to a value
    auto sub (string ctx, string val, *string type_assertion, *hash<auto> tctx) {
        switch (ctx) {
            case "static":  return static_ctx{val};
            case "config":  return config.get(val);
            case "env":     return ENV{val};
        }
        throw "TEMPLATE-RESOLUTION-ERROR", sprintf("unknown context %y", ctx);
    },
    # resolve_type: return the type for validation (optional)
    *AbstractDataProviderType sub (string ctx, string val, *hash<auto> tctx) {
        if (ctx == "config") {
            return config.getType(val);
        }
    },
    # list_contexts: for completions (optional)
    auto sub (*hash<auto> tctx) {
        return (
            {"name": "static",  "description": "Static values",  "sort_priority": 1},
            {"name": "config",  "description": "Configuration",  "sort_priority": 2},
            {"name": "env",     "description": "Environment",    "sort_priority": 3},
        );
    },
    # list_values: for completions within a context (optional)
    auto sub (string ctx, string prefix, *hash<auto> tctx) {
        if (ctx == "env") {
            return map {"name": $1, "description": "env var"}, keys ENV,
                $1.lwr().find(prefix.lwr()) == 0;
        }
    },
);
```

### Dynamic Value Callbacks

Dynamic value callbacks provide a legacy mechanism for resolving URI-style dynamic values
(separate from DPQL template references). Register via
`AbstractDataProvider::setDynamicValueCallbacks()`:

```qore
# Two-arg form: register callbacks
static bool setDynamicValueCallbacks(code value_needs_resolution, code resolve_value);

# Zero-arg form: lock without registering (prevents others from registering)
static bool setDynamicValueCallbacks();
```

The same locking semantics apply: the first call locks, subsequent calls succeed only on
identity match. These callbacks are independent of template callbacks and serve different
resolution paths in the DataProvider framework.

## Template Resolution

When `evalGenericExpressionValue()` encounters a `hash<DataProviderTemplateReference>`,
it calls the registered `expand` callback:

```
expand(tmpl_context, tmpl_value, type_assertion, template_context)
```

| Parameter | Source |
|-----------|--------|
| `tmpl_context` | The context identifier (e.g., `"static"`, `"config"`) |
| `tmpl_value` | The value path (e.g., `"account.id"`, `"threshold"`) |
| `type_assertion` | Optional type from `::type` suffix (e.g., `"int"`) |
| `template_context` | Opaque per-query hash threaded from `DefaultRecordIterator` |

The `template_context` parameter allows per-request state to flow from the iterator
constructor through to the callback. For example, a Qorus workflow step might pass the
current order ID so that `$static:order_id` resolves to the correct value.

If no `expand` callback is registered and a template reference is evaluated, a
`TEMPLATE-RESOLUTION-ERROR` exception is thrown with the raw template text.

```qore
# Example: expand callback with type assertion handling
auto sub (string ctx, string val, *string type_assertion, *hash<auto> tctx) {
    auto result;
    switch (ctx) {
        case "static":
            result = tctx.static_values{val};
            break;
        case "config":
            result = config_store.get(val);
            break;
        default:
            throw "TEMPLATE-RESOLUTION-ERROR", sprintf("unknown context %y", ctx);
    }
    if (type_assertion) {
        switch (type_assertion) {
            case "int":    return int(result);
            case "float":  return float(result);
            case "string": return string(result);
            case "bool":   return boolean(result);
        }
    }
    return result;
}
```

## Completion Callbacks

### Listing Template Contexts

The `list_contexts` callback returns available contexts for the `$` completion trigger.
Each entry should be a hash with:

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `name` | `string` | yes | Context identifier (e.g., `"static"`) |
| `description` | `string` | yes | Human-readable label |
| `sort_priority` | `int` | no | Lower values sort first |

If no `list_contexts` callback is registered, `DpqlCompletionProvider` falls back to
9 well-known `DefaultTemplateContexts`: `static`, `dynamic`, `config`, `var`,
`transient`, `env`, `timestamp`, `qore-expr`, `rest`.

```qore
auto sub () {
    return (
        {"name": "static",    "description": "Static context values",    "sort_priority": 1},
        {"name": "dynamic",   "description": "Dynamic runtime values",   "sort_priority": 2},
        {"name": "config",    "description": "Configuration values",     "sort_priority": 3},
        {"name": "env",       "description": "Environment variables",    "sort_priority": 6},
    );
}
```

### Listing Template Values

The `list_values` callback is called with the context name and a prefix string for
filtering. Return a list of hashes with the same shape as context entries:

```qore
auto sub (string ctx, string prefix) {
    if (ctx == "config") {
        return map {"name": $1, "description": "config value"},
            config_store.keys(), $1.find(prefix) == 0;
    }
    if (ctx == "env") {
        return map {"name": $1, "description": ENV{$1}},
            keys ENV, $1.lwr().find(prefix.lwr()) == 0;
    }
}
```

If no `list_values` callback is registered, template value completions return an empty
list.

## Type Resolution

The `resolve_type` callback returns an `*AbstractDataProviderType` for a template
reference, enabling type compatibility checking during `validateDpqlExpression()`:

```qore
*AbstractDataProviderType sub (string ctx, string val, *hash<auto> tctx) {
    if (ctx == "config") {
        *string type_name = config_store.getTypeName(val);
        switch (type_name) {
            case "int":    return AbstractDataProviderType::get(IntType);
            case "string": return AbstractDataProviderType::get(StringType);
            case "float":  return AbstractDataProviderType::get(FloatType);
        }
    }
    # Return NOTHING to skip type validation for unknown references
}
```

When `resolve_type` returns `NOTHING` (or is not registered), type validation is skipped
for that reference — the expression is accepted without type-checking the template value.

## Session Lifecycle

### Constructor Options

Create a `DpqlActionSession` with optional configuration:

```qore
DpqlActionSession session({"token_format": "token-span", "field_format": "map"});
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `token_format` | `string` | `"token-span"` | `"token-span"`: 0-based row/col spans; `"dpql"`: 1-based, includes expression tree |
| `field_format` | `string` | `"map"` | `"map"`: hash keyed by field name; `"list"`: list of field hashes |
| `logger` | `*LoggerInterface` | `NOTHING` | Optional logger for debug output |

### Action Dispatch

All interaction goes through `handleAction()`:

```qore
hash<auto> handleAction(string action, *hash<auto> args);
```

### Action Reference

| Action | Required Args | Optional Args | Response Keys |
|--------|--------------|---------------|---------------|
| `dpql-set-context` | `provider` (string) | `subtype`, `options`, `recordOptions`, `recordType` | `provider`, `recordType`, `recordRequiresSearchOptions`, `fields`, `expressions` |
| `dpql-parse` | `text` (string) | `fields` | `success`, `diagnostics`, `tokens` (+ `expression` if format=`"dpql"`) |
| `dpql-get-completions` | `text` (string), `position` (int) | `fields` | `completions` |
| `dpql-get-tokens` | `text` (string) | | `tokens` |
| `dpql-format` | `text` (string) | | `formatted`, `success` |
| `dpql-validate` | `text` (string) | `fields` | `diagnostics` |
| `dpql-serialize` | `expression` (hash) | | `dpql` |

The `dpql-set-context` action must be called first to establish the provider and field
schema. Subsequent parse/complete/validate actions use the cached context. The `fields`
arg on `dpql-parse`, `dpql-get-completions`, and `dpql-validate` allows overriding the
cached fields for a single call.

```qore
# Example: session lifecycle
DpqlActionSession session({"token_format": "token-span", "field_format": "map"});

# 1. Set context — establishes provider and field schema
hash<auto> ctx = session.handleAction("dpql-set-context", {
    "provider": "datasource/omq/table/orders",
    "recordType": "record",
});

# 2. Parse — returns tokens and diagnostics
hash<auto> parsed = session.handleAction("dpql-parse", {
    "text": "@status == \"active\" && @amount > 100",
});
if (parsed.success) {
    # expression parsed cleanly
}

# 3. Get completions at cursor position
hash<auto> comps = session.handleAction("dpql-get-completions", {
    "text": "@st",
    "position": 3,
});

# 4. Validate against field schema
hash<auto> diags = session.handleAction("dpql-validate", {
    "text": "@status == \"active\"",
});
```

## Field Metadata

### Automatic Field Discovery

When `dpql-set-context` is called with a `provider`, the session resolves the provider and
calls `provider.getRecordType()` to obtain field metadata automatically. The result is a
`hash<string, AbstractDataField>` mapping field names to their type descriptors.

### Manual Field Construction

For scenarios where the provider is not available or fields need augmentation, construct
fields manually using `QoreDataField`:

```qore
hash<string, AbstractDataField> fields;

# Simple typed fields
fields.name = new QoreDataField("name", "User name", StringType);
fields.age = new QoreDataField("age", "User age", IntType);
fields.active = new QoreDataField("active", "Active flag", BoolType);

# Nested hash field — enables @record{key} completions
HashDataType address_type();
address_type.addField(new QoreDataField("street", "Street", StringType));
address_type.addField(new QoreDataField("city", "City", StringType));
address_type.addField(new QoreDataField("zip", "ZIP code", StringType));
fields.address = new QoreDataField("address", "Mailing address", address_type);
```

### Schema-Aware Key Completions

When a field has a hash type with known sub-fields, the completion provider can suggest
keys inside `{...}` accessors. For example, with the `address` field above, typing
`@address{` will suggest `street`, `city`, and `zip`.

This requires provider-backed context from `dpql-set-context` or manually constructed
`HashDataType` fields with sub-fields.

### Field Override Pattern

The `dpql-parse`, `dpql-get-completions`, and `dpql-validate` actions accept an optional
`fields` argument that overrides the cached field schema for that single call:

```qore
hash<auto> result = session.handleAction("dpql-get-completions", {
    "text": "@",
    "position": 1,
    "fields": custom_field_map,
});
```

## WebSocket Exposure

### Handler Setup

`DpqlWebSocketHandler` extends `WebSocketHandler` and creates per-connection
`DpqlActionSession` instances:

```qore
constructor(*HttpServer::AbstractAuthenticator auth, *hash<auto> opts);
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `auth_callback` | `code(hash cx, hash hdr) returns bool` | `NOTHING` | Custom authentication callback |
| `max_connections` | `int` | `0` | Maximum concurrent connections (0 = unlimited) |

All additional options in `opts` are passed through to the base `WebSocketHandler`
constructor (e.g., `heartbeat`, `heartbeat_msg`, `logger`, `send_timeout`).

```qore
# Example: handler with authentication and connection limit
DpqlWebSocketHandler handler(authenticator, {
    "auth_callback": bool sub (hash<auto> cx, hash<auto> hdr) {
        return validate_token(hdr."Authorization");
    },
    "max_connections": 50,
    "heartbeat": 30,
});
```

### Connection Lifecycle

When a client connects, `DpqlWebSocketConnection` is created with:
- A new `DpqlActionSession` (token_format=`"dpql"`, field_format=`"list"`)
- A random 16-character session ID

The server immediately sends a `session` message:

```json
{"type": "session", "sessionId": "...", "version": "1.0", "qoreVersion": "..."}
```

Client messages are JSON objects with a `type` field that maps to session actions:

| Client `type` | Session Action | Response `type` |
|----------------|---------------|-----------------|
| `context` | `dpql-set-context` | `context` |
| `tokenize` | `dpql-get-tokens` | `tokens` |
| `parse` | `dpql-parse` | `parse` |
| `validate` | `dpql-validate` | `validate` |
| `complete` | `dpql-get-completions` | `completions` |
| `serialize` | `dpql-serialize` | `serialize` |
| `ping` | (handled directly) | `pong` |

For the full wire protocol schema, see
[DpqlApi.asyncapi.yaml](../qlib/DpqlWebSocket/DpqlApi.asyncapi.yaml).

## Expression Evaluation

### Parse-Then-Evaluate Pipeline

DPQL expressions are first parsed into a `hash<DataProviderExpression>` tree, then
evaluated against record hashes at query time.

**`evalGenericExpression()`** evaluates a parsed expression tree:

```qore
static auto evalGenericExpression(
    hash<auto> rec,
    hash<DataProviderExpression> exp,
    *hash<auto> template_context,
);
```

It looks up the operator implementation from `GenericExpressionImplementations`, calls it,
and validates the return value against the operator's declared return type.

**`evalGenericExpressionValue()`** resolves leaf values in the expression tree:

```qore
static auto evalGenericExpressionValue(
    hash<auto> rec,
    auto val,
    *hash<auto> template_context,
);
```

It handles three value types:
- `hash<DataProviderFieldReference>` — resolves dotted field paths against the record
- `hash<DataProviderTemplateReference>` — calls the registered `expand` callback
- `hash<DataProviderExpression>` — recursively evaluates nested expressions
- All other values are returned as-is (literals)

### Old-Style Hash Matching

`AbstractDataProviderRecordIterator::matchGeneric()` provides backward-compatible matching
using operator hashes (the pre-DPQL query format). It also resolves template references
via `resolveTemplateValue()`, which delegates to `evalGenericExpressionValue()` for
`hash<DataProviderTemplateReference>` values.

### DefaultRecordIterator Threading

`DefaultRecordIterator` accepts a `template_context` hash in its constructor and threads
it through to `matchGeneric()` on each iteration:

```qore
constructor(
    AbstractIterator i,
    *hash<auto> where_cond,
    *hash<auto> search_options,
    *hash<string, AbstractDataField> record_type,
    *string subrecord,
    *hash<auto> template_context,
);
```

The `template_context` flows: constructor → stored as member → passed to
`matchGeneric()` → passed to `evalGenericExpression()` → passed to `expand` callback.

```qore
# Example: parse-then-evaluate with template context
string dpql_text = "@status == $static:required_status && @amount > $config:min_amount::int";

# Parse
hash<DataProviderExpression> expr = DataProvider::parseDpqlExpression(dpql_text);

# Evaluate against a record with per-request context
hash<auto> record = {"status": "active", "amount": 500};
hash<auto> tctx = {"static_values": {"required_status": "active"}};

bool match = AbstractDataProvider::evalGenericExpression(record, expr, tctx);
```

## Static API Reference

### Method Summary

All methods are static on the `DataProvider` class:

| Method | Signature | Description |
|--------|-----------|-------------|
| `parseDpqlExpression` | `hash<DataProviderExpression>(string text, *hash<string, hash<DataProviderExpressionInfo>> expressions)` | Parse DPQL text into an expression tree; throws `DPQL-PARSE-ERROR` on syntax error |
| `parseDpqlExpressionWithInfo` | `hash<DpqlParseResult>(string text, *hash<string, hash<DataProviderExpressionInfo>> expressions)` | Parse with full result (expression, diagnostics, tokens, success flag) |
| `serializeDpqlExpression` | `string(hash<DataProviderExpression> exp, *hash<string, hash<DataProviderExpressionInfo>> expressions)` | Serialize an expression tree back to DPQL text |
| `getDpqlTokens` | `list<hash<DpqlTokenInfo>>(string text)` | Tokenize DPQL text for syntax highlighting |
| `getDpqlCompletions` | `list<hash<DpqlCompletionItem>>(string text, int position, hash<string, AbstractDataField> fields, *hash<string, hash<DataProviderExpressionInfo>> expressions, *hash<auto> template_context)` | Get completions at a cursor position |
| `validateDpqlExpression` | `list<hash<DpqlDiagnostic>>(string text, hash<string, AbstractDataField> fields, *hash<string, hash<DataProviderExpressionInfo>> expressions)` | Validate expression against field schema; returns diagnostics |
| `setTemplateCallbacks` | `bool(code expand, *code resolve_type, *code list_contexts, *code list_values)` | Register template resolution callbacks; all callbacks receive a trailing `*hash<auto> template_context` argument |
| `clearTemplateCallbacks` | `(none)` | Clear and unlock template callbacks |

The optional `expressions` parameter defaults to `DataProviderGenericExpressions` (the
built-in operator set) when not provided. The `template_context` hash is passed through
unchanged to each registered template callback.

### Hashdecl Summary

| Hashdecl | Key Fields | Description |
|----------|------------|-------------|
| `DataProviderExpression` | `exp` (string), `args` (list) | Parsed expression node |
| `DataProviderFieldReference` | `field` (string) | Field reference (`@name`) |
| `DataProviderTemplateReference` | `tmpl_context`, `tmpl_value`, `*type_assertion`, `raw` | Template reference (`$ctx:val`) |
| `DpqlParseResult` | `*expression`, `diagnostics`, `success`, `tokens` | Full parse result |
| `DpqlDiagnostic` | `severity`, `message`, `code`, `line`, `column`, `end_line`, `end_column` | Parse/validation diagnostic (1-based positions) |
| `DpqlTokenInfo` | `type` (int), `value`, `line`, `column`, `end_line`, `end_column` | Token info (1-based positions) |
| `DpqlCompletionItem` | `label`, `kind`, `insert_text`, `*documentation`, `sort_priority` | Completion suggestion |
| `DataProviderExpressionInfo` | `type`, `name`, `display_name`, `symbol`, `args`, `return_type` | Operator/function metadata |

### Complete Pipeline Example

```qore
%modern

hash<string, AbstractDataField> fields;
fields.status = new QoreDataField("status", "Order status", StringType);
fields.amount = new QoreDataField("amount", "Order amount", FloatType);
fields.created = new QoreDataField("created", "Created date", DateType);

string dpql = "@status == \"active\" && @amount > 100";

# 1. Validate
list<hash<DpqlDiagnostic>> diags = DataProvider::validateDpqlExpression(dpql, fields);
if (diags) {
    map printf("  %s:%d:%d: %s\n", $1.severity, $1.line, $1.column, $1.message), diags;
}

# 2. Parse
hash<DataProviderExpression> expr = DataProvider::parseDpqlExpression(dpql);

# 3. Evaluate
hash<auto> record = {"status": "active", "amount": 250.0, "created": now()};
bool match = AbstractDataProvider::evalGenericExpression(record, expr);
# match == True

# 4. Round-trip
string serialized = DataProvider::serializeDpqlExpression(expr);
# serialized == "@status == \"active\" && @amount > 100"
```

## Error Reference

| Error | Thrown By | Condition |
|-------|----------|-----------|
| `DPQL-PARSE-ERROR` | `DpqlParser::parse()` | Syntax error in DPQL expression |
| `TEMPLATE-RESOLUTION-ERROR` | `evalGenericExpressionValue()` | Template reference encountered but no `expand` callback registered |
| `INVALID-OPERATION` | `evalGenericExpressionValue()`, `evalGenericExpression()` | Unknown operator or invalid field path in record |
| `INVALID-ACTION` | `DpqlActionSession::handleAction()` | Unsupported action name |
| `MISSING-PARAMETER` | `DpqlActionSession` action handlers | Required arg missing from action args |
| `INVALID-PARAMETER` | `DpqlActionSession` action handlers | Arg has wrong type |
| `NO-CONTEXT` | `DpqlActionSession::handleCompletions()` | Completions requested but no provider context set |
| `CONTEXT-ERROR` | `DpqlActionSession::handleSetContext()` | Provider could not be resolved |
| `WEBSOCKETHANDLER-OPTION-ERROR` | `DpqlWebSocketHandler::constructor()` | Invalid handler option |

## Cross-References

- [DPQL Syntax Reference](dpql-syntax.md) — end-user syntax for fields, operators, values,
  and template references
- [Action-Based Session Services](action-session-services.md) — high-level architecture for
  DPQL, REPL, and OpenAI action-based sessions
- [DpqlApi.asyncapi.yaml](../qlib/DpqlWebSocket/DpqlApi.asyncapi.yaml) — WebSocket wire
  protocol schema
