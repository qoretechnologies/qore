# Rich Content Schema for Creator WebSocket

This document defines a normalized rich content schema for Creator WS payloads,
including text, code, JSON, YAML, images, and files.

## Goals

- Consistent content shape across OpenAI responses, REPL output, and other services.
- Support for documents, code blocks with syntax highlighting, and media.
- Preserve raw source content alongside structured data when available.

## Content Part Schema

Each response item may include a list of content parts:

```
ContentPart:
- type (string): text | code | json | yaml | image | file | markdown
- text (*string): raw text representation
- data (*hash<auto>): structured data for json/yaml or metadata
- language (*string): code language (for code parts)
- mime_type (*string): for image/file parts
- url (*string): external URL (if available)
- bytes (*binary): raw bytes (optional, use only when necessary)
- filename (*string): original filename
- annotations (*list<hash<auto>>): optional inline annotations
```

### JSON and YAML

- `type: "json"` should include `data` (parsed object) and optional `text`.
- `type: "yaml"` should include `data` (parsed object when available) and optional `text`.
- If parsing fails, provide only `text`.

### Code

- `type: "code"` should include `language` and `text`.
- The frontend should apply syntax highlighting using token metadata when available.

### Images

- `type: "image"` should include `mime_type` and either `url` or `bytes`.
- Prefer `url` when possible to avoid large WS payloads.

### Files

- `type: "file"` should include `filename`, `mime_type`, and either `url` or `bytes`.

## Normalized Output Item

```
OutputItem:
- role (string): user | assistant | system | tool
- content (list<ContentPart>)
- metadata (*hash<auto>)
```

## Mapping from OpenAI Responses

- OpenAI `output_text` -> `type: text` part.
- OpenAI content parts:
  - `output_text` -> `text`
  - `output_image` -> `image`
  - `output_audio` -> `file` (audio)
- Tool output can map to `json` or `yaml` based on content type.

## Creator WS Integration

- All OpenAI response actions should return normalized `OutputItem` structures.
- Streaming events should carry `ContentPart` deltas when available.

## AsyncAPI Schema Requirements

- Schema definitions must document each content part type and its fields in the
  `/creator` WebSocket `@publish` messages.
