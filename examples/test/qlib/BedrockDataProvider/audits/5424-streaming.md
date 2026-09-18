# Bedrock native event streaming: pre-commit audit

Copyright 2026 Qore Technologies, s.r.o.

Audit date: 2026-09-18. Scope: native streaming additions following `c55391f34` on `develop`.
Checklist: `audit-changes/SKILL.md`, including every check below. No C++ or JNI code changes.
The original completed-response audit in `5424.md` remains historical evidence, not a description of
the new streaming implementation.

## Contract and evidence

`BedrockRestClientIo` implements `ConverseStream`, `InvokeModelWithResponseStream`, and
`InvokeModelWithBidirectionalStream`. The first two deliver response events incrementally; the third
supports simultaneous input/output over HTTP/2. The model-specific `chunk.bytes` value is binary after
base64 decoding. The caller owns model event ordering and audio encoding. Streams retain their client,
allow upload half-close, and must be closed when abandoned. HTTP failures, service exception events,
bad CRCs, malformed headers/JSON, timeouts, and truncated EOF never become successful completion.

`BedrockLlmProvider::chatStream()` now emits native Converse events and text fragments as they arrive.
OpenAI-compatible tool calls retain dense indices across nonconsecutive native content-block indices;
argument fragments are accumulated without replaying calls. Stop reason and usage survive the final
metadata event. Exactly one completion follows clean EOF and a message stop. Callback exceptions retain
their original identity, and failures cancel the stream without retrying a partially delivered generation.

The 118 completed-value actions are unchanged. Open event streams are exposed through the native client,
not mislabeled as completed-value actions. The app description, schema rationale, module docs, release
notes, generated root catalog, and all twelve translations now document this distinction.

Protocol references reviewed:

- https://docs.aws.amazon.com/bedrock/latest/APIReference/API_runtime_ConverseStream.html
- https://docs.aws.amazon.com/bedrock/latest/APIReference/API_runtime_InvokeModelWithResponseStream.html
- https://docs.aws.amazon.com/bedrock/latest/APIReference/API_runtime_InvokeModelWithBidirectionalStream.html
- https://docs.aws.amazon.com/nova/latest/userguide/input-events.html
- AWS SDK C++ `AWSAuthEventStreamV4Signer.cpp` and `EventStreamEncoder.cpp` in `aws/aws-sdk-cpp`.

The independent HTTP/2 fixture recomputes the chained HMAC using manually encoded timestamp headers,
checks the signed empty terminal envelope, and echoes an inner event before upload EOF. Live Nova Sonic
also accepts the complete signed input sequence and returns usage followed by clean EOF. Credential
values are obtained using `aws configure export-credentials --format env` and are never recorded.

Design references read: `qore-module-structure.md`, `module-sandboxing-audit-guide.md`,
`cooperative-cancellation.md`, `data-provider-checklist.md`, `data-provider-development-guide.md`, and
`data-provider-rest-schema-apps.md`.

## Full checklist

|!Check|!Status|!Evidence
|Module index entry|Pass|All four existing modules remain registered; AwsEventStream description now includes encoding.
|Release notes|Pass|Global and module notes describe all three streams, CRC validation, signing, and incremental LLM events.
|CMake module registration|Pass|Existing `qore_user_module()` entries cover all changed sources; the new AwsEventStream dependency comes from `%requires`.
|QMOD registration|Pass|Release and debug qmod targets build, including transitive dependencies.
|Lowercase first intro section|Pass|All changed module intros use the documented lowercase names.
|Modern module directives|Pass|Existing `%modern` retained; no redundant parse directives added.
|Separated QC directives|Pass|BedrockSchema.qc contains no module parse directive.
|No module include|Pass|No `%include` added.
|New-file copyright|Pass|New test and audit files identify copyright 2026.
|Module directory layout|Pass|Existing flat single-file clients/codec and directory provider layout retained.
|No duplicate module|Pass|No new module entry point or duplicate qm added.
|QPP namespace alignment|N/A|No QPP class changed.
|Modern tests|Pass|All changed/new qtests use `%modern`.
|Executable tests|Pass|Both new wire tests and existing modified tests have executable permission.
|Test module paths|Pass|In-repository module path precedes `%requires`.
|Optional external test modules|Pass|New dependencies are bundled Qore modules; existing external-dependency conventions retained.
|Native filesystem sandbox checks|N/A|No C++ filesystem change.
|Native network sandbox checks|N/A|No C++ network change.
|Native sandbox helper|N/A|No native I/O added.
|Qore I/O justification|Pass|Production uses the existing sandbox-aware HTTP manager. Raw sockets bind loopback only in wire fixtures; file reads load existing test TLS fixtures.
|Native long-loop cancellation|N/A|No C++ loop added; Qore loops use runtime cancellation points.
|Unified native cancellation API|N/A|No native cancellation call changed.
|Native cancellation frequency|N/A|No native loop changed.
|Blocking cancellation|Pass|Timeout and cross-thread close tests wake blocked readers; cleanup does not take the reader lock.
|Action presentation keys|Pass|No action registration changed; complete provider suite checks the existing 118-action surface.
|Action options|Pass|Existing schema-derived options and curated field mappings retained and tested.
|Action output types|Pass|Existing typed schema-backed outputs retained and tested.
|DPAT_API support|Pass|Existing request providers and supports_request declarations unchanged.
|DPAT_FIND search options|N/A|No record-search action changed.
|Scheme action paths, no cls|Pass|Existing scheme-based app and action paths unchanged; provider tests resolve them.
|Single-key hash slices|Pass|No new single-key slice; existing registration tests pass.
|Typed request/response types|Pass|No DP type change; native event framing uses AwsEventStreamFrame and LLM delivery uses ChatStreamEvent.
|Public request Fields|Pass|Existing curated request Fields and schema action contracts retained.
|Private response Fields|Pass|Existing response visibility retained.
|Field presentation metadata|Pass|No field metadata changed; recursive provider tests remain green.
|Useful input examples|Pass|Existing examples retained; streaming docs add regional client and event-processing examples.
|Finite AllowedValueInfo choices|Pass|No enum change; existing finite-choice coverage passes.
|Sensitive credentials|Pass|AWS secret/token options unchanged; no real credentials committed.
|AppGroup enum|Pass|Existing AppGroup::AiLlm retained.
|Separate app logo|Pass|Existing installed SVG resource retained.
|Business app description|Pass|Capability bullets and project link retained; streaming availability now stated accurately.
|Friendly app name|Pass|AWS Bedrock name unchanged.
|Plain short descriptions|Pass|No short description changed; provider metadata tests pass.
|Markdown descriptions|Pass|Updated paragraph preserves the app's paragraphs, bullets, code spans, and links.
|Business context|Pass|App continues to explain workflow inference and administration capabilities.
|Boolean/nothing prose|Pass|No bare code literals introduced in presentation prose.
|Field names in prose|Pass|No unformatted field identifier introduced in app description.
|Long descriptions|Pass|App retains bold introduction and capability bullets.
|FactoryMap|Pass|Existing bedrock mapping retained; clean-installed discovery finds the app.
|getRecordTypeImpl signature|N/A|No record-provider implementation changed.
|Committed dependency JARs|N/A|No JNI dependency.
|JAR installation|N/A|No JNI dependency.
|Lightweight producer inventory|Pass|Unchanged exact 118-action inventory; clean-installed qualification does not add native sessions as actions.
|Structured qualification failures|Pass|Provider and installed qualification report zero initialization/discovery failures.
|Authenticated publication token|Pass|Unchanged shared publication path exercised by source-excluded installed index generation/readback.
|Versioned schema boundary|Pass|No schema or normalization change; existing absence/null/choice regressions pass.
|Static recursive presentation|Pass|Only a static app paragraph changed; strict extraction succeeds without connection callbacks.
|Stable presentation identities|Pass|Root remains 5,052 IDs; only app description text changes.
|Locale parity|Pass|All 60,624 translations pass strict root/source parity and completeness checks.
|Installed artifacts|Pass|Fresh empty-prefix install, source paths excluded, passes AST/IR/JIT/tiered discovery and index publication.
|JNI canonical inventory|N/A|No Java dependency.
|No workarounds or stubs|Pass|All three native operations implemented; no TODO/FIXME placeholder added.
|Exception safety|Pass|Stream errors cancel transport; on_exit closes high-level streams; callback exceptions are preserved.
|Thread safety|Pass|Read, write, and lifecycle locks are separate. Signing state is per-session; credentials/owner are immutable, and client initialization remains locked.
|Type safety|Pass|Typed signing callback, frame hashdecl, ChatStreamEvent and ExceptionInfo; dynamic hashes only at native JSON boundaries.
|Performance|Pass|No complete-response buffering or token replay; incremental decoder drains each frame and bounds advertised frame/header sizes. Existing CRC table is immutable after locked initialization.
|Error handling|Pass|Malformed/duplicate/truncated headers, oversize frames, CRC failures, service errors, HTTP status/media type, idle timeout, callback errors, and late failure are tested.
|Public documentation|Pass|New public operations document parameters, results, errors, concurrency, ownership, and model-owned audio/session semantics; Doxygen builds cleanly.
|QPP flags|N/A|No QPP method changed.
|Security|Pass|Independent wire signatures and live authentication pass; no credentials logged or committed; existing fixture key/password is test-only.
|Correctness|Pass|Wire, bidirectional signature-chain, codec, execution-mode, debug, clean-install, and live AWS gates listed below.

## Validation gates

|!Gate|!Result
|Release Bedrock matrix|AST, IR, JIT, tiered: 43 cases / 2,015 assertions per mode, all passing.
|Debug codec and Bedrock|55 cases / 2,134 assertions, freshly rebuilt debug qmods, all passing.
|AWS client regressions|12 cases / 54 assertions; legacy public IoT Events network probe skips because its hostname no longer resolves.
|Codec bounds/round-trip|12 cases / 119 assertions, including independent encoding fixtures and all decoded header types.
|Shared LLM streaming|23 cases / 144 assertions pass after rebuilding the shared modules updated by the upstream pull.
|Strict catalogs|5,052 exact IDs per locale; 60,624 translations checked across twelve standard locales.
|Installed qualification|Empty prefix `/tmp/bedrock-stream-install.tF2dAZ`; four fresh execution-mode processes, 118 actions, 108+8 schema operations, zero failures, qualified index publication/readback.
|Documentation|All four docs-fast targets pass without warnings after fixing a fully qualified public-class reference.
|Live AWS|Committed opt-in suite: 14 cases / 57 assertions pass, including Nova Micro Converse and model-response streams and a Nova Sonic signed silent-audio session with usage and clean EOF.
|Hygiene|`git diff --check`, executable/modern checks, changed-code review, and sensitive-value scans pass. No unrelated working-tree edits.

Tests use rebuilt qmods and `-penable-debug`. Live streaming tests are opt-in with
`BEDROCK_STREAM_LIVE=1` on `BedrockStreaming.qtest`; they use the normal AWS credential chain and may
incur inference charges. No live provisioning or account configuration is required. The bidirectional
smoke test proves framing, signing, ingestion, accounting, and shutdown, not speech-recognition quality.

The audit corrected callback-exception wrapping, upload-half-close cleanup, stale streaming documentation,
and one unresolved public documentation reference. All affected checks are rerun before commit.
