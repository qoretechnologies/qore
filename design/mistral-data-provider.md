# Mistral data provider

MistralDataProvider 1.1 exposes 137 completed-response operations from Mistral's official
OpenAPI document, vendored in the module (see below).
The original `chat-completion`, `create-embeddings`, and `ocr` action IDs and public
provider classes are retained. Their request and response classes now use the
schema-derived fields; the legacy `Fields` constants remain for source compatibility.
Chat responses retain the original epoch-to-date conversion of `created`.

The committed schema determines paths, HTTP methods, requiredness, nested types,
validation, and wire formats. `MistralManifest` selects stable action names and adds
presentation, finite choices, examples, and 21 resource dropdowns. Nullable enums
and arrays retain the same choices and element lookups as their non-nullable forms.
Parent-scoped dropdowns require the selected library, agent, conversation, or dataset. Listings fetch one
page; custom identifiers remain accepted and normal action options expose pagination. Dataset-record selectors enumerate one page of records
for each dataset in the first dataset page, because records are addressed globally but listed per dataset.

The 137 actions cover models, chat, fill-in-the-middle completion, embeddings,
moderation/classification, files, batch inference, OCR, audio, agents, conversations,
libraries and documents, connectors and their credentials, evaluation datasets, and workflows,
from 133 of the document's 347 operations (some operations back more than one action). The
others are not exported: deprecated operations, streaming-only variants (streaming chat belongs
to the Mistral LLM provider), and APIs that no action covers yet, such as administration, observability logs, traces, spans and pipelines, prompts, skills, triggers and
webhooks. Publishing an upstream operation requires an explicit manifest change.

`MistralSchema` records upstream provenance and loads the vendored JSON resource
under a mutex. Module initialization builds and registers the action set; execution
does not fetch schemas. CMake installs the JSON, import specification, and logo.
The schema is deliberately stored as JSON, which the importer supports independently
of the YAML serialization bug fixed in module-yaml while importing this API.

The upstream document is Mistral's official API reference document,
`https://docs.mistral.ai/openapi.yaml` (the "Download OpenAPI Spec" link of `https://docs.mistral.ai/api/`). It has
no versioned or commit-pinned URL, so the exact downloaded document is kept beside the import specification as
`mistral-openapi-source.yaml` (not installed), and the import reads it from there. The import specification's
`source_url` names the official URL, which `MistralSchema::Provenance` records with the vendored document's SHA-256
digest, size and import date, so an import is reproducible byte for byte.
To adopt a newer revision, download the document over that file and re-import.

Re-import using the local Qore modules and a YAML module for the upstream document:

```sh
QORE_MODULE_DIR=qlib tools/rest-schema-import.qr --spec qlib/MistralDataProvider/mistral-import.yaml
```

Review provenance and run the importer drift
check against `MistralSchema::getActionSet()` before adopting a newer revision.
The import records three reviewed upstream defects:

- `SharingDelete` requires an absent `level` property. The official SDK requires
  only `share_with_uuid` and `share_with_type`. The repair checks the old required list.
- Connector reference paths include documentation fragments and use different names
  for the same path parameter. The repair removes the fragments, renames the parameter
  declarations positionally, and merges disjoint HTTP methods. Collisions, stale
  sources, and changed parameter counts fail the import.
- The `text/event-stream` response of `POST /v1/audio/speech` refers to its local `$defs` as `#/$defs/...`, which
  resolves against the document root. The repair drops that streaming response; `generate-speech` does not stream.

Operations that the official document no longer declares are not exported: the observability campaign,
chat-completion event and field, explorer and judge operations, whose endpoints answer 404 or 410, and the
undocumented workflow worker identity. Connector credentials are created, updated and deleted through the
`{consumer_scope}` operations; each scope-specific action fixes the scope.

Connections identify the `MistralAi` app and use the canonical Mistral URL when built
from generated connection configuration. The API key is required and sensitive.
Both sync and async pings authenticate at `/v1/models`, regardless of the direct
client's selected API version. Action providers copy the connection client and clear
its prefix because the schema uses explicit versioned paths. The caller's client
retains its original prefix. Requests are sent once; ambiguous transport failures
must not duplicate inference charges or resource creation.

The OpenApi3 implementation handles JSON parameter content used by agent and
conversation metadata filters. Required-property presence is independent of nullability: an explicit null
filter remains valid through field caching, overlays and JSON serialization; omitting the required
property fails. Nullable scalar and composition types retain that distinction. Content objects are individual query values rather
than catch-all query objects. Parameters are validated as native values, serialized
once, and URI-escaped. Multipart file fields accept named files through nullable
binary schemas, preserving filename, content bytes, and media type.

Tests include a fixed request fixture for every action, catalog/type checks, all
resource dropdowns, missing parent scopes, negative requests, no automatic retries,
legacy classes, a local HTTP server, and optional live tests. `MistralLive.qtest` uses
`MISTRALAI_APIKEY`; OCR and transcription additionally use `MISTRAL_TEST_PDF` and
`MISTRAL_TEST_AUDIO`. It deletes test-created files, agents and libraries, and cancels
unfinished test-created batch jobs. No test polls job completion. Live account limits
are explicit skips: billing-disabled batch jobs and unavailable preview services do
not imply invalid authentication. Unit tests still exercise those request contracts.
