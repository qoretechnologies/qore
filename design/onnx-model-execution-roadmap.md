# ONNX Model Execution Roadmap

This document tracks the implementation plan for making Qore a strong ONNX
model deployment and execution platform.  The focus is production inference:
safe model management, predictable execution, strong diagnostics, columnar and
zero-copy data paths, and performance that can be measured against Python-based
ONNX Runtime workflows.

## Goals

- Make ONNX Runtime behavior explicit: providers, options, fallback, profiling,
  optimization level, and model format must be visible through Qore APIs.
- Keep model artifacts safe and portable through QoreModelRegistry metadata,
  validation, checksums, and execution plans.
- Prefer dense and columnar execution paths over row hash materialization.
- Support common enterprise inference tasks directly: raw tensor inference,
  classification/regression, embeddings, reranking, and transformer pipelines.
- Use Python ecosystem tools where they are the right source of truth for export
  and quantization, while keeping Qore responsible for orchestration,
  validation, deployment, and execution.

## Non-Goals

- Reimplement the PyTorch, HuggingFace, or scikit-learn ONNX exporters in Qore.
- Make training a primary focus.
- Silently fall back to CPU when a requested provider is unavailable.
- Hide performance-relevant copies between DataFrame, Arrow, tensor, and ONNX
  Runtime layers.

## Phase 0: Design and Baseline

Status: completed.

Deliverables:

- Inventory current `ML::OnnxModel`, `ML::Tensor`, `DataProviderML`,
  `QoreModelRegistry`, tokenizer, dataframe, Arrow, and device-buffer support.
- Record the minimum supported ONNX Runtime version for advanced features.
- Define provider fallback semantics and manifest schema additions before
  changing runtime behavior.
- Add benchmarks and validation criteria before optimizing data paths.

Acceptance checks:

- This roadmap exists and is kept current.
- Each subsequent phase has tests, docs, benchmarks where performance is affected,
  and valgrind coverage for representative native paths.

## Phase 1: Provider Management and Runtime Diagnostics

Status: completed.

Add explicit provider discovery, provider option reporting, requested-provider
validation, and fallback diagnostics.

Runtime APIs:

- `ml_get_onnx_providers()`
- `ml_get_onnx_provider_options()`
- `ML::OnnxModel::getProviders()`
- `ML::OnnxModel::getProviderOptions()`
- `ML::OnnxModel::getRequestedProviders()`
- `ML::OnnxModel::getEffectiveProviderReport()`

Config additions:

- `providers`
- `provider_options`
- `required_providers`
- `allow_cpu_fallback`
- `fail_on_provider_fallback`

Registry metadata:

- `execution.providers`
- `execution.provider_options`
- `execution.required_providers`
- `execution.allow_cpu_fallback`
- `execution.provider_diagnostics`

Acceptance checks:

- Explicit invalid providers fail with a descriptive exception.
- Explicit GPU-only config fails when the provider cannot load.
- CPU-only config remains easy: `providers: ()` or omitted CPU provider.
- Provider diagnostics are available from both raw `ML::OnnxModel` and
  registry-loaded model processors.

Implementation notes:

- Raw `ML::OnnxModel` now exposes available providers, requested providers,
  provider option metadata, provider diagnostics, and an effective provider
  report.
- Invalid explicit providers and unavailable required providers raise
  `ML-ONNX-PROVIDER-ERROR` with the requested provider and available-provider
  list.
- Registry and `DataProviderML` integration uses the raw model API until the
  high-level registry contract is expanded in Phase 10.

## Phase 2: Tensor and Type Coverage

Status: completed.

Extend tensor conversion coverage so Qore can run more exported models without
ad hoc conversion code.

Types:

- `float16`, `bfloat16` when supported by the ONNX Runtime build
- `float32`, `float64`
- `int8`, `uint8`, `int16`, `uint16`, `int32`, `uint32`, `int64`, `uint64`
- `bool`
- string tensors

Shape support:

- dynamic batch dimensions
- symbolic dimensions in metadata
- external data files
- optional, sequence, and map outputs where practical

Acceptance checks:

- Synthetic ONNX fixtures cover dtype families.
- Shape mismatch errors include input name, expected shape/type, and supplied
  shape/type.
- DataFrame dense columns and `buffer<T>` map to tensors without row hashes.

Implementation notes:

- `ML::OnnxModel::run()` now supports `float16`, `bfloat16`, unsigned integer,
  and string tensor inputs and outputs in addition to the previously-supported
  float, double, signed integer, and bool tensor families.
- `ML::OnnxModel::runTensors()` returns half/bfloat16 tensors as `float32`
  buffers and unsigned integer tensors as `int64` buffers because the Qore
  dense buffer type system does not currently expose unsigned or half-float
  element types.
- `ML::Tensor` now supports `buffer<string>` and string list data for string
  tensor workflows.
- Tensor metadata includes `symbolic_shape` alongside numeric shape metadata.

## Phase 3: OrtValue, I/O Binding, and Zero-Copy Execution

Status: core APIs implemented; dataframe/Arrow/device binding integration remains
in later phases.

Expose ONNX Runtime binding primitives so Qore can avoid unnecessary host/device
copies and repeated tensor reconstruction.

APIs:

- `ML::OnnxIoBinding`: implemented for reusable model-specific bindings.
- `ML::OnnxRunOptions`: implemented for run tags, per-run logging,
  termination, and ONNX Runtime config entries.
- `ML::OnnxModel::createBinding()`: implemented.
- `ML::OnnxModel::runBound()`: implemented.
- `ML::OnnxValue` or `ML::OrtValue`: deferred until there is a separate need
  to pass provider-owned values outside an `OnnxIoBinding`.

Supported bindings:

- `ML::Tensor`: implemented for inputs and fixed-shape numeric preallocated
  outputs.
- `buffer<T>`: implemented for inputs when the ONNX element type maps directly
  to the Qore dense buffer type.
- dataframe dense columns: deferred to the DataProviderML/dataframe integration
  phase.
- Arrow buffers when layout is compatible: deferred to Arrow-backed tensor and
  dataframe interchange work.
- provider-owned device buffers: deferred until device buffer ownership and
  provider memory metadata are exposed in Qore.

Acceptance checks:

- Tests cover reusable input binding, ONNX Runtime-managed output allocation,
  run options, and Qore-preallocated numeric output tensors.
- Benchmarks must still compare nested lists, tensors, DataFrame dense columns,
  and I/O binding.
- DataProviderML must still use binding reuse for stable shapes.

## Phase 4: Offline Graph Optimization and ORT Format

Status: completed for raw ML APIs, registry optimized/quantized artifact
metadata, and named optimized-artifact loading with provider compatibility
checks.

Add first-class support for optimized ONNX artifacts and ORT format artifacts.

APIs:

- `ML::onnx_optimize_model(input_path, output_path, config)`: implemented as
  `ML::onnx_optimize_model(input_path:, output_path:, options:, ort_format:)`.
- `ML::OnnxModel::saveOptimized(path, config)`: implemented as
  `ML::OnnxModel::saveOptimized(output_path:, options:, ort_format:)`.
- `optimized_model_filepath` in `OnnxSessionConfig`: implemented.
- `.ort` artifact loading from file and bytes: implemented with
  `load_model_format: "ORT"` and automatic `.ort` path detection by ONNX
  Runtime.

Registry metadata:

- original artifact checksum
- optimized artifact checksum
- target provider and options
- ONNX Runtime version
- CPU feature or device profile

Acceptance checks:

- Optimized ONNX and ORT artifacts are loaded and compared against golden
  vectors in `modules/ml/test/ml.qtest`.
- Optimized and quantized artifacts are exposed in derived manifest metadata as
  `optimized_artifacts` and `quantized_artifacts`.
- Named optimized/quantized ONNX artifacts can be loaded through registry APIs;
  hardware-specific optimized artifact metadata rejects incompatible provider
  requests before execution.

## Phase 5: Profiling and Observability

Status: completed for raw profiling controls, Qore-side counters, structured
profile summarization, and DataProviderML diagnostics APIs.

Expose ONNX Runtime profiling and Qore-level inference metrics.

APIs:

- enable profiling in session config: implemented with
  `OnnxSessionConfig.enable_profiling` and `profile_file_prefix`.
- `ML::OnnxModel::endProfiling()`: implemented; returns the generated profile
  file path.
- `ML::OnnxModel::getInferenceStats()`: implemented for run counts, batch item
  counts, total/last/max/average latency, and profile file state.
- `ML::OnnxModel::resetInferenceStats()`: implemented.
- `QoreOnnxTools::OnnxProfileTools::summarizeProfile()`: implemented for ONNX Runtime
  Chrome-trace JSON profiles with operator, provider, node, and session
  aggregates.

Metrics:

- model name/version
- provider list and fallback status
- input and output shapes
- batch size
- run latency
- operator-level timing where available

Acceptance checks:

- ONNX Runtime profile files are generated and exposed to Qore callers.
- Qore-side inference counters and latency stats are available as Qore data.
- Structured operator-level profile parsing is available through
  QoreOnnxTools and DataProviderML ONNX processors expose provider diagnostics,
  inference counters, profile paths, and profile summaries through
  `getDiagnostics()`.

## Phase 6: Quantization and Calibration

Status: Qore orchestration and original-vs-candidate comparison implemented;
full static calibration workflow coverage remains dependent on external
calibration data and Python quantization availability.

Use existing ONNX Runtime and Optimum tooling when available; Qore owns
orchestration, validation, registry metadata, and deployment comparison.

Workflows:

- dynamic quantization through `QoreOnnxTools::OnnxQuantizer`
- static quantization with JSON calibration input through
  `QoreOnnxTools::OnnxQuantizer`
- hardware presets such as `avx2`, `avx512`, `avx512_vnni`, `arm64`, and
  `tensorrt`: option metadata is accepted and passed through for tooling that
  supports the requested target
- original vs quantized accuracy/output comparison: implemented through
  `QoreOnnxTools::OnnxModelTools::compare()` and `qore-onnx compare`
- original vs quantized latency comparison: implemented through optional
  benchmark fields in the same comparison API

APIs:

- `QoreOnnxTools::OnnxQuantizer::checkPythonDependencies()`: implemented for
  actionable Python/ONNX Runtime quantization dependency diagnostics.
- `QoreOnnxTools::OnnxQuantizer::plan()`: implemented as a dry-run command and
  artifact plan generator.
- `QoreOnnxTools::OnnxQuantizer::quantize()`: implemented for dynamic and
  static quantization orchestration with source/output model inspection.
- `OnnxQuantizationOptions`: implemented for method, weight/activation types,
  calibration input, op/node filters, extra ONNX Runtime quantization options,
  overwrite policy, and validation.
- `OnnxQuantizationResult`: implemented for command, dependency, source model,
  output model, and metadata reporting.

Acceptance checks:

- Missing external quantization dependencies produce actionable errors.
- Dry-run mode produces the exact generated command without requiring external
  quantization dependencies.
- Quantized artifacts report source/output metadata and dependency information.
- QoreModelRegistry can register optimized and quantized artifacts as named
  artifacts with source version/artifact lineage metadata.
- Successful validated quantization registration stores original-vs-quantized
  output and latency comparison metadata on the quantized artifact.
- Calibration fingerprint storage remains pending.

## Phase 7: Import and Conversion Tooling

Status: initial CLI implemented.

Add Qore CLI and registry workflows for model ingestion.  External Python tools
may be invoked when installed, but Qore must record the exact command,
dependency versions, inputs, and outputs.

Tools:

- `qore-onnx check-python`: implemented for dependency diagnostics.
- `qore-onnx quantize`: implemented for dynamic/static quantization
  orchestration and dry-run planning.
- `qore-onnx inspect`: implemented for model metadata and provider
  diagnostics.
- `qore-onnx validate`: implemented for model loadability, tensor metadata, and
  provider-policy checks.
- `qore-onnx package`: implemented for deterministic flat package assembly,
  companion artifact classification, SHA-256 checksums, and manifest metadata.
- `qore-onnx benchmark`: implemented for generated or JSON-provided inputs and
  latency summaries.
- `qore-onnx optimize`: implemented for optimized ONNX/ORT artifact creation
  through the ml module.

Acceptance checks:

- PyTorch, HuggingFace, scikit-learn, and raw ONNX packages are classified.
- Unsafe pickle artifacts remain metadata-only.
- ONNX artifacts with tokenizer/config/external-data companions are packaged
  reproducibly.

## Phase 8: Task-Specific ONNX Execution Plans

Status: completed.

Add registry execution plan types so users do not have to hand-code common
preprocess/run/postprocess logic.

Execution plans:

- `onnx/raw`
- `onnx/classification`
- `onnx/regression`
- `onnx/embedding`
- `onnx/reranker`
- `onnx/token-classification`
- `onnx/sequence-classification`
- `onnx/encoder-decoder`
- `onnx/causal-lm`

Acceptance checks:

- Existing `embed-text` and `rerank-text` processors use the plan metadata.
  Implemented for registry-backed ONNX packages with `task = "embedding"` and
  `task = "reranker"`; processors infer ONNX mode and validate incompatible
  declared tasks.
- Sequence classification and regression plans return clear typed outputs.
  Implemented for `registry-model` event output with stable `value`, `label`,
  `scores`, and `raw_outputs` fields.
- Initial causal-LM support handles tokenizer input, attention masks, greedy
  decoding, stop tokens, and basic KV cache metadata. Implemented for
  `registry-model` event output with tokenizer artifacts loaded from
  QoreModelRegistry packages and truthful metadata when stateful KV-cache reuse
  is requested by the plan but not exposed by the current Qore ONNX execution
  path.

## Phase 9: Session Pools, Async Inference, and Dynamic Batching

Status: in progress.

Add pooled inference infrastructure for high-throughput service workloads.

APIs:

- `ML::OnnxSessionPool`
- max sessions
- max concurrent runs
- queue depth
- timeout
- cancellation
- dynamic batching: max batch size and max wait time

Acceptance checks:

- Concurrent inference is deterministic for immutable sessions.
  Implemented for `ML::OnnxSessionPool` with a bounded set of immutable
  `OnnxModel` sessions, configurable `max_sessions`, `max_concurrent_runs`,
  `queue_depth`, `timeout_ms`, cancellation checks while waiting, and pool
  statistics.
- Backpressure and timeout behavior is tested.
  Implemented for saturated pools with `queue_depth = 0` backpressure and a
  waiting caller hitting `timeout_ms`.
- Dynamic batching preserves result ordering.
  Implemented for caller-supplied batches by splitting `runBatch()` input into
  stable `dynamic_batch_size` chunks and appending results in input order.
  Implemented for cross-caller `runAsync()` coalescing when both
  `dynamic_batch_size` and `dynamic_batch_wait_ms` are configured; each Future
  resolves to its own ordered result.
- Representative pool tests pass under valgrind.
  Verified with a representative async/coalesced `OnnxSessionPool` run under
  valgrind; no errors and no definite, indirect, or possible leaks were
  reported.

## Phase 10: High-Level Registry Contract

Status: pending.

Extend QoreModelRegistry manifests with a complete ONNX deployment contract.

Manifest fields:

- `framework`
- `runtime`
- `task`
- `opset`
- `producer`
- `minimum_onnxruntime_version`
- `inputs`
- `outputs`
- `dynamic_axes`
- `external_data`
- `tokenizer`
- `preprocess`
- `postprocess`
- `execution`
- `providers`
- `optimized_artifacts`
- `quantized_artifacts`
- `validation`
- `benchmarks`
- `golden_vectors`

Acceptance checks:

- Registry actions can inspect, validate, benchmark, optimize, quantize, and
  compare ONNX model versions.
- Manifest diagnostics are visible from DataProvider navigation and API
  metadata.

## Phase 11: Final Documentation and Full Verification

Status: pending.

Documentation:

- `doxygen/lang/*` ONNX user documentation
- `DataProviderML` ONNX/dataframe examples
- `QoreModelRegistry` ONNX package examples
- release notes
- migration notes for provider fallback behavior

Verification:

- `cmake --build build`
- focused qtests for `ml`, `QoreTokenizerUtils`, `QoreModelRegistry`,
  `DataProviderML`, and dataframe
- ONNX/DataProviderML benchmark pass
- valgrind for representative native ONNX paths
- docs fast targets, with full docs only when doc subsystem changes require it

## Commit Discipline

- Keep commits focused by phase or independently useful slice.
- Run the audit-changes checklist before each commit.
- Include verification commands in commit messages when relevant.
- Do not push without explicit approval.
