# Voyage AI Module Design: VoyageRestClient + VoyageDataProvider

## Overview

Two new Qore modules providing embedding and reranking support via the
[Voyage AI API](https://docs.voyageai.com/):

1. **VoyageRestClient.qm** — REST client and connection provider
2. **VoyageDataProvider/** — Data provider actions for embeddings and reranking

Modeled after the `AnthropicRestClient.qm` + `AnthropicDataProvider/` pattern.

---

## Voyage AI API Summary

| Endpoint | Method | Path | Purpose |
|----------|--------|------|---------|
| Embeddings | POST | `/v1/embeddings` | Generate text embeddings |
| Rerank | POST | `/v1/rerank` | Rerank documents by relevance |

**Base URL**: `https://api.voyageai.com`
**Auth**: `Authorization: Bearer <api_key>`

### Embeddings Request
```json
{
  "model": "voyage-4-large",
  "input": ["text1", "text2"],
  "input_type": "document",
  "truncation": true,
  "output_dimension": 1024,
  "output_dtype": "float",
  "encoding_format": null
}
```

### Embeddings Response
```json
{
  "object": "list",
  "data": [
    {"object": "embedding", "embedding": [0.1, 0.2, ...], "index": 0}
  ],
  "model": "voyage-4-large",
  "usage": {"total_tokens": 42}
}
```

### Rerank Request
```json
{
  "model": "rerank-2.5",
  "query": "search query",
  "documents": ["doc1", "doc2"],
  "top_k": 5,
  "return_documents": false,
  "truncation": true
}
```

### Rerank Response
```json
{
  "object": "list",
  "data": [
    {"index": 0, "relevance_score": 0.85, "document": "..."}
  ],
  "model": "rerank-2.5",
  "usage": {"total_tokens": 156}
}
```

---

## File Structure

```
qlib/
  VoyageRestClient.qm                         # REST client + connection
  VoyageDataProvider/
    VoyageDataProvider.qm                      # Module definition, registerApp, actions
    VoyageDataProviderCommon.qc                # Base class with REST client + retry
    VoyageDataProvider.qc                      # Root provider (container)
    VoyageDataProviderFactory.qc               # Factory for DataProvider registration

    # Embeddings
    VoyageEmbeddingsDataProvider.qc            # POST /v1/embeddings
    VoyageEmbeddingsRequestDataType.qc         # Request type definition
    VoyageEmbeddingsResponseDataType.qc        # Response type definition

    # Reranking
    VoyageRerankDataProvider.qc                # POST /v1/rerank
    VoyageRerankRequestDataType.qc             # Request type definition
    VoyageRerankResponseDataType.qc            # Response type definition

    voyage-logo.svg                            # App icon (square)

examples/test/qlib/VoyageDataProvider/
    VoyageDataProvider.qtest                   # Integration tests
```

---

## Module 1: VoyageRestClient.qm

### Classes

#### VoyageRestClientBase
Constants and option processing (following `AnthropicRestClientBase` pattern):
```qore
const DefaultUrl = "https://api.voyageai.com";
const DefaultApiVersion = "v1";
const DefaultPingMethod = "POST";
const DefaultPingPath = "embeddings";  # ping uses the embeddings endpoint with a minimal request body
const DefaultPingBody = {"model": "voyage-3", "input": ("ping",)};

static hash<auto> getOptions(hash<auto> opts) {
    # Map apikey → Authorization: Bearer header
    # Set default_path to /v1
    # Set default ping options, including a minimal ping_body for POST /v1/embeddings
}
```

#### VoyageRestClient extends RestClient
- Constructor accepts: `url`, `api` (default "v1"), `apikey` (required)
- Maps `apikey` to `Authorization: Bearer <key>` header
- `Content-Type: application/json` default

#### VoyageRestConnection extends RestConnection
- **Scheme**: `"voyage"`
- **Default URL**: `"voyage://api.voyageai.com"`
- **`auto_url: True`**
- **Connection options**:
  - `apikey` (required, sensitive, preselected)
  - `api` (default: "v1")
- **`hasDataProvider()` → True**
- **`getDataProvider()`** → dynamically loads `VoyageDataProvider`

#### Connection Scheme Registration
Add to `qlib/ConnectionProvider/ConnectionSchemeCache.qc` → `SchemeMap`:
```qore
"voyage": <ConnectionSchemeInfo>{
    "cls": Class::forName("VoyageRestConnection"),
    ...
}
```

---

## Module 2: VoyageDataProvider/

### App Registration

```qore
DataProviderActionCatalog::registerApp(<DataProviderAppInfo>{
    "name": "Voyage AI",
    "display_name": "Voyage AI",
    "short_desc": "Voyage AI embedding and reranking services",
    "desc": "Provides text embedding generation and document reranking via the "
        "[Voyage AI API](https://voyageai.com). Supports state-of-the-art embedding "
        "models for semantic search, RAG, and classification.",
    "scheme": "voyage",
    "logo": VoyageLogo,
    "logo_mime_type": MimeTypeSvg,
    "groups": (DataProvider::AppGroup::AiLlm,),
});
```

### Provider Hierarchy

```
VoyageDataProvider (root container)
├── create-embeddings (VoyageEmbeddingsDataProvider)
└── rerank (VoyageRerankDataProvider)
```

### Actions

| Action | Provider | Type | Description |
|--------|----------|------|-------------|
| `create-embeddings` | `VoyageEmbeddingsDataProvider` | DPAT_API | Generate text embeddings |
| `rerank` | `VoyageRerankDataProvider` | DPAT_API | Rerank documents by query relevance |

---

## Data Types

### VoyageEmbeddingsRequestDataType

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `model` | string | Yes | Model name (e.g., `voyage-4-large`) |
| `input` | softlist\<string\> | Yes | Text(s) to embed (max 1000 items) |
| `input_type` | \*string | No | `query` or `document` — prepends retrieval prompts |
| `truncation` | \*bool | No | Truncate inputs exceeding model limits (default: `true`) |
| `output_dimension` | \*int | No | 256, 512, 1024 (default), or 2048 |
| `output_dtype` | \*string | No | `float` (default), `int8`, `uint8`, `binary`, `ubinary` |
| `encoding_format` | \*string | No | `null` or `base64` |

**`model` allowed_values** (with display_name):
- `voyage-4-large` — "Voyage 4 Large (best quality)"
- `voyage-4` — "Voyage 4 (balanced)"
- `voyage-4-lite` — "Voyage 4 Lite (fastest)"
- `voyage-code-3` — "Voyage Code 3 (code retrieval)"
- `voyage-finance-2` — "Voyage Finance 2 (finance domain)"
- `voyage-law-2` — "Voyage Law 2 (legal domain)"
- `voyage-3-large` — "Voyage 3 Large"
- `voyage-3` — "Voyage 3"
- `voyage-multilingual-2` — "Voyage Multilingual 2"

**`input_type` allowed_values**:
- `query` — "Query (for retrieval queries)"
- `document` — "Document (for documents to retrieve)"

**`output_dtype` allowed_values**:
- `float` — "Float 32 (default, highest accuracy)"
- `int8` — "Int8 (4x storage reduction)"
- `uint8` — "Uint8 (4x storage reduction)"
- `binary` — "Binary (32x storage reduction)"
- `ubinary` — "Unsigned Binary (32x storage reduction)"

**`output_dimension` allowed_values**:
- 256 — "256 dimensions"
- 512 — "512 dimensions"
- 1024 — "1024 dimensions (default)"
- 2048 — "2048 dimensions"

**Preselected options** (non-required): `input_type`, `output_dimension`

### VoyageEmbeddingsResponseDataType

| Field | Type | Description |
|-------|------|-------------|
| `object` | string | Always `"list"` |
| `data` | list\<VoyageEmbeddingItemDataType\> | Embedding results |
| `model` | string | Model used |
| `usage` | VoyageUsageDataType | Token usage |

**VoyageEmbeddingItemDataType**:
| Field | Type | Description |
|-------|------|-------------|
| `object` | string | Always `"embedding"` |
| `embedding` | list\<auto\> | Embedding vector (floats or encoded) |
| `index` | int | Position in input list |

**VoyageUsageDataType**:
| Field | Type | Description |
|-------|------|-------------|
| `total_tokens` | int | Total tokens processed |

### VoyageRerankRequestDataType

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `model` | string | Yes | Reranking model |
| `query` | string | Yes | Query text |
| `documents` | list\<string\> | Yes | Documents to rerank (max 1000) |
| `top_k` | \*int | No | Number of top results to return |
| `return_documents` | \*bool | No | Include document text in response |
| `truncation` | \*bool | No | Truncate inputs (default: `true`) |

**`model` allowed_values**:
- `rerank-2.5` — "Rerank 2.5 (best quality)"
- `rerank-2.5-lite` — "Rerank 2.5 Lite (fastest)"
- `rerank-2` — "Rerank 2"

**Preselected options** (non-required): `top_k`, `return_documents`

### VoyageRerankResponseDataType

| Field | Type | Description |
|-------|------|-------------|
| `object` | string | Always `"list"` |
| `data` | list\<VoyageRerankItemDataType\> | Ranked results (descending by score) |
| `model` | string | Model used |
| `usage` | VoyageUsageDataType | Token usage |

**VoyageRerankItemDataType**:
| Field | Type | Description |
|-------|------|-------------|
| `index` | int | Original document index |
| `relevance_score` | float | Relevance score |
| `document` | \*string | Document text (if `return_documents` was `true`) |

---

## Base Class: VoyageDataProviderCommon

Following `AnthropicDataProviderCommon` pattern:

```qore
class VoyageDataProviderCommon inherits AbstractDataProvider {
    private {
        *VoyageRestClient rest;
    }

    # REST command with retry for transient errors
    hash<auto> doRestCommand(string method, string path, auto body, *hash<auto> hdr) {
        int retries = 0;
        while (True) {
            try {
                return rest.doRequest(method, path, body, \info, NOTHING, hdr).body;
            } catch (hash<ExceptionInfo> ex) {
                if (retries >= 5 || !isRetryableError(ex)) {
                    rethrow;
                }
                ++retries;
            }
        }
    }

    private bool isRetryableError(hash<ExceptionInfo> ex) {
        return ex.err =~ /SOCKET-SSL-ERROR|SOCKET-CLOSED/;
    }
}
```

---

## Implementation Notes

### Error Handling
- HTTP 429 (rate limit): Implement exponential backoff with `Retry-After` header support
- HTTP 400: Surface API error message from response body
- Transient socket errors: Retry up to 5 times (matching Anthropic pattern)

### Sandboxing
- Mark all network operations with `QDOM_NETWORK` functional domain
- REST client methods must respect sandbox restrictions

### Cooperative Cancellation
- Long-running batch embedding requests should check for cancellation between retries
- Use the cooperative cancellation pattern from the design guide for any polling loops

### Build Registration

**CMakeLists.txt**:
```cmake
qore_user_module("qlib/VoyageRestClient.qm"
    "ConnectionProvider;Mime;RestClient")

qore_user_module("qlib/VoyageDataProvider"
    "VoyageRestClient;ConnectionProvider;Mime;RestClient;DataProvider"
    "voyage-logo.svg")
```

**Makefile.am**: Add corresponding entries for docs and install targets.

### Documentation
- Add entry in `doxygen/lang/120_modules.dox.tmpl`
- Add release note in `doxygen/lang/900_release_notes.dox.tmpl`
- Copyright 2026

---

## Implementation Phases

### Phase 1: REST Client (VoyageRestClient.qm)
1. Create `VoyageRestClient.qm` with `VoyageRestClientBase`, `VoyageRestClient`, `VoyageRestConnection`
2. Register connection scheme in `ConnectionSchemeCache.qc`
3. Verify connection ping works

### Phase 2: Data Provider — Embeddings
1. Create module skeleton: `.qm`, factory, root provider, common base
2. Implement `VoyageEmbeddingsDataProvider` with request/response types
3. Register app and `create-embeddings` action
4. Write tests with mock and live API

### Phase 3: Data Provider — Reranking
1. Implement `VoyageRerankDataProvider` with request/response types
2. Register `rerank` action
3. Write tests

### Phase 4: Build, Docs, Polish
1. Add to `CMakeLists.txt` and `Makefile.am`
2. Add doxygen entries
3. Add voyage-logo.svg
4. Run full test suite
5. Run audit checklist

---

## Implementation Audit Checklist

Run this checklist after implementation is complete. References the
[data-provider-checklist.md](data-provider-checklist.md) and
[module-sandboxing-audit-guide.md](module-sandboxing-audit-guide.md).

### REST Client Module
- [ ] `VoyageRestClient.qm` has `%requires qore >= 2.0` and `%modern`
- [ ] `VoyageRestClientBase` constants: `DefaultUrl`, `DefaultApiVersion`, `DefaultPingMethod`, `DefaultPingPath`
- [ ] `apikey` mapped to `Authorization: Bearer` header (not `x-api-key`)
- [ ] Connection scheme `"voyage"` registered in `ConnectionSchemeCache.qc` → `SchemeMap`
- [ ] `VoyageRestConnection` has `auto_url: True`
- [ ] `apikey` option is `sensitive: True` and `preselected: True`
- [ ] `required_options` string declares `"apikey"`
- [ ] `hasDataProvider()` returns `True`
- [ ] `getDataProvider()` dynamically loads `VoyageDataProvider` module
- [ ] Ping path works (returns non-error response)
- [ ] Copyright 2026

### Data Provider Module
- [ ] `VoyageDataProvider.qm` has `%requires qore >= 2.0` and `%modern`
- [ ] `%requires(reexport) DataProvider` and `%requires(reexport) VoyageRestClient`
- [ ] `VoyageDataProviderFactory` registered in init block
- [ ] `registerApp()` called with all required fields
- [ ] `groups` includes `DataProvider::AppGroup::AiLlm`
- [ ] `display_name` is "Voyage AI"
- [ ] `short_desc` is plain text, under 80 chars
- [ ] `desc` uses markdown formatting
- [ ] Logo loaded from separate `voyage-logo.svg` file via `File::readTextFile()`
- [ ] Logo has square dimensions
- [ ] Copyright 2026

### Embeddings Action
- [ ] Action registered as `create-embeddings` in `DataProviderActionCatalog`
- [ ] Action has `display_name`, `short_desc` (plain text), `desc` (markdown)
- [ ] Action has `options` populated via `getActionOptionFromFields()`
- [ ] Action has `output_type` set to response type
- [ ] `model` field is required with `allowed_values` (all `AllowedValueInfo` with `display_name`)
- [ ] `input` field is required, type `softlist<string>`
- [ ] `input_type` has `allowed_values` with `display_name` and `preselected: True`
- [ ] `output_dimension` has `allowed_values` with `display_name` and `preselected: True`
- [ ] `output_dtype` has `allowed_values` with `display_name`
- [ ] `encoding_format` has `allowed_values` with `display_name`
- [ ] `doRequestImpl()` POSTs to `"embeddings"` path
- [ ] Response type has typed `data` list with `VoyageEmbeddingItemDataType`
- [ ] Response type has typed `usage` with `VoyageUsageDataType`

### Rerank Action
- [ ] Action registered as `rerank` in `DataProviderActionCatalog`
- [ ] Action has `display_name`, `short_desc` (plain text), `desc` (markdown)
- [ ] Action has `options` populated via `getActionOptionFromFields()`
- [ ] Action has `output_type` set to response type
- [ ] `model` field is required with `allowed_values` (all `AllowedValueInfo` with `display_name`)
- [ ] `query` field is required, type string
- [ ] `documents` field is required, type `list<string>`
- [ ] `top_k` has `preselected: True`
- [ ] `return_documents` has `preselected: True`
- [ ] `doRequestImpl()` POSTs to `"rerank"` path
- [ ] Response `data` items include `relevance_score` field (float type)
- [ ] Response `data` items include optional `document` field

### Type Safety
- [ ] All request/response types use custom `HashDataType` classes
- [ ] Required fields use non-optional types (`StringType`, `IntType`)
- [ ] Optional fields use optional types (`*string`, `*int`, `*bool`)
- [ ] Type classes declared inside `public namespace VoyageDataProvider { ... }` block
- [ ] `const Fields` declared in `public {}` blocks
- [ ] Static `RequestType` and `ResponseType` members declared in `public {}` blocks
- [ ] `example_value` on key fields (e.g., `input`: `"The quick brown fox"`)

### Error Handling
- [ ] Transient socket errors retried (up to 5 retries)
- [ ] HTTP 429 rate limit errors handled with appropriate messaging
- [ ] API error messages surfaced from response body
- [ ] All exception paths are safe (no resource leaks)

### Sandboxing & Functional Domains
- [ ] Network operations marked with `QDOM_NETWORK`
- [ ] Module does not access filesystem (no `QDOM_FILESYSTEM` needed)
- [ ] Module does not spawn processes (no `QDOM_PROCESS` needed)

### Markdown in Descriptions
- [ ] All `desc` fields use markdown where appropriate
- [ ] All `short_desc` fields are plain text, under 80 chars
- [ ] Code references use backticks (`` `field_name` ``, `` `True` ``)
- [ ] Enumerations use bullet lists, not inline prose
- [ ] All backtick pairs are matched

### Build System
- [ ] `CMakeLists.txt` has `qore_user_module()` for `VoyageRestClient.qm`
- [ ] `CMakeLists.txt` has `qore_user_module()` for `VoyageDataProvider`
- [ ] `Makefile.am` entries added
- [ ] Module builds without errors

### Documentation
- [ ] Entry in `doxygen/lang/120_modules.dox.tmpl`
- [ ] Release note in `doxygen/lang/900_release_notes.dox.tmpl`

### Tests
- [ ] Test file exists: `examples/test/qlib/VoyageDataProvider/VoyageDataProvider.qtest`
- [ ] `%modern` directive used
- [ ] Test file has executable permission
- [ ] Uses relative `%requires` paths (not module names)
- [ ] Tests use mock REST client for unit tests (no API key needed)
- [ ] Tests verify request body structure (field names, types)
- [ ] Tests verify response parsing (embedding vectors, scores)
- [ ] Tests verify connection ping
- [ ] Tests verify `allowed_values` on model fields
- [ ] Tests assert specific field values (not just `assertTrue(True)`)
- [ ] Live API tests guarded by environment variable (e.g., `VOYAGE_API_KEY`)
- [ ] Live tests clean up (no persistent state to clean for embeddings/rerank)
- [ ] Tests cover: embeddings with single input, batch input, rerank basic, rerank with return_documents
