# Syntax Highlighting Design

This document defines a unified syntax highlighting token model for Creator WS
and provides a plan for module-treesitter integration.

## Goals

- Single token schema across DPQL, Tree-sitter, and other sources.
- Stable format for frontend rendering.
- Support both full-text and incremental highlighting updates.

## Token Schema

```
TokenSpan:
- type (string): category (keyword, operator, identifier, string, number, comment,
  function, class, field, property, type, constant, label, punctuation, directive)
- start (hash): row (int, 0-based), column (int, 0-based)
- end (hash): row (int, 0-based), column (int, 0-based)
- text (*string): optional raw text
- modifiers (*list<string>): optional (declaration, readonly, deprecated, async, static)
- language (*string): optional language tag
```

### Rationale

- The token model is compatible with both DPQL token types and tree-sitter capture
  names.
- `modifiers` enables richer UI without expanding the base type list.

## DPQL Token Mapping

DPQL token types map to `type` categories:
- DPQL_TOK_FIELD -> field
- DPQL_TOK_OPERATOR -> operator
- DPQL_TOK_STRING -> string
- DPQL_TOK_NUMBER -> number
- DPQL_TOK_IDENTIFIER -> identifier
- DPQL_TOK_FUNCTION -> function
- DPQL_TOK_PAREN -> punctuation

## Tree-sitter Token Mapping

- Use capture names from tree-sitter queries and map them to `type` categories.
- Preserve original capture in `modifiers` when helpful for UI.

## Creator WS Actions

### Highlighting

- `ts-highlight` and `dpql-get-tokens` return tokens using the unified model.
- For incremental updates, include optional `range` and return only tokens within
  that range.

### Completion Context

Token spans should be used to:
- Provide hover ranges.
- Drive semantic highlighting in the editor.

## module-treesitter Integration Plan

Phase 1:
- Add a common token conversion utility in module-treesitter.
- Map existing language queries to the unified token schema.
- Provide "one-shot" highlight API returning TokenSpan list.

Phase 2:
- Provide incremental parsing and highlight updates.
- Add optional symbol index for semantic tokens (functions, classes, etc.).

Phase 3:
- Add Qore language queries and validate with Creator WS.

## AsyncAPI Schema Requirements

- Ensure `@publish` messages document `TokenSpan` fields in `/creator` WebSocket
  AsyncAPI definitions.
