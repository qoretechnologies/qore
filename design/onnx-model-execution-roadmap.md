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

Status: raw ML APIs implemented; registry metadata integration remains pending.

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
- Hardware-specific optimized model metadata and mismatched-provider rejection
  remain registry-level work.

## Phase 5: Profiling and Observability

Status: pending.

Expose ONNX Runtime profiling and Qore-level inference metrics.

APIs:

- enable profiling in session config
- `ML::OnnxModel::startProfiling()`
- `ML::OnnxModel::endProfiling()`
- structured profile summary parser

Metrics:

- model name/version
- provider list and fallback status
- input and output shapes
- batch size
- run latency
- operator-level timing where available

Acceptance checks:

- Profiling output is available as Qore data, not only raw JSON.
- DataProviderML can emit metrics or attach them to a diagnostics side channel.

## Phase 6: Quantization and Calibration

Status: pending.

Use existing ONNX Runtime and Optimum tooling when available; Qore owns
orchestration, validation, registry metadata, and deployment comparison.

Workflows:

- dynamic quantization
- static calibration from DataProvider/DataFrame input
- hardware presets such as `avx2`, `avx512`, `avx512_vnni`, `arm64`, and
  `tensorrt`
- original vs quantized accuracy comparison
- original vs quantized latency comparison

Acceptance checks:

- Missing external quantization dependencies produce actionable errors.
- Quantized artifacts record lineage, calibration fingerprint, and validation
  results.

## Phase 7: Import and Conversion Tooling

Status: pending.

Add Qore CLI and registry workflows for model ingestion.  External Python tools
may be invoked when installed, but Qore must record the exact command,
dependency versions, inputs, and outputs.

Tools:

- `qore-onnx inspect`
- `qore-onnx validate`
- `qore-onnx package`
- `qore-onnx benchmark`
- `qore-onnx optimize`
- `qore-onnx quantize`

Acceptance checks:

- PyTorch, HuggingFace, scikit-learn, and raw ONNX packages are classified.
- Unsafe pickle artifacts remain metadata-only.
- ONNX artifacts with tokenizer/config/external-data companions are packaged
  reproducibly.

## Phase 8: Task-Specific ONNX Execution Plans

Status: pending.

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
- Sequence classification and regression plans return clear typed outputs.
- Initial causal-LM support handles tokenizer input, attention masks, greedy
  decoding, stop tokens, and basic KV cache metadata.

## Phase 9: Session Pools, Async Inference, and Dynamic Batching

Status: pending.

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
- Backpressure and timeout behavior is tested.
- Dynamic batching preserves result ordering.
- Representative pool tests pass under valgrind.

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
