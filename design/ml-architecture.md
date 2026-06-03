# ML Module Architecture

## Overview

The Qore ML ecosystem provides machine learning capabilities through a multi-layer
architecture:

1. **`ml` binary module** (C++/Eigen3) — Thread-safe native ML algorithms, preprocessing,
   metrics, serialization, and cross-validation
2. **`DataProviderML` user module** (Qore) — Data pipeline processor wrappers for all
   algorithms plus metric and registry processors
3. **`QoreModelRegistry` user module** (Qore) — Model versioning, package
   artifact metadata, comparison, and deployment management with pluggable backends
4. **`QoreOnnxTools` user module** (Qore) — ONNX inspection, validation,
   benchmarking, optimization, quantization, packaging, execution-plan, and CLI helpers
5. **`tokenizer` binary module** (C++/utf8proc) — HuggingFace-compatible text tokenization

## Design Principles

- **No Python dependency** — Native algorithms run entirely in C++ using Eigen3
- **Thread-safe** — All classes use `mutable std::mutex` for concurrent access
- **Fit-once, predict-concurrently** — After fitting, prediction methods are safe for
  concurrent calls
- **Window-oriented** — Pipeline processors accumulate records in windows before processing
- **Typed results** — All algorithms return typed hashdecl results for type safety
- **Optional ONNX** — ONNX Runtime support is `#ifdef HAVE_ONNXRUNTIME` guarded
- **Online learning** — Key algorithms support incremental `update()` for streaming data
- **Serialization** — All fitted models can be saved/loaded via native binary format

## Module Layering

```
┌──────────────────────────────────────────────────────────┐
│                Qorus / Applications                      │
├──────────────────────────────────────────────────────────┤
│       DataProviderML (Qore)        QoreOnnxTools (Qore)  │
│  Algorithm processors + preprocessing + metrics +        │
│  registry model processor        qore-onnx CLI helpers   │
├──────────────────────────────────────────────────────────┤
│             QoreModelRegistry (Qore)                     │
│  Model versions + named artifacts + execution metadata   │
│  Backends: filesystem, database, REST                    │
├──────────────────────────────────────────────────────────┤
│        ml (C++ binary)          tokenizer (C++ binary)   │
│  17 algorithms + metrics +      BPE, WordPiece, Unigram  │
│  serialization + pipeline       HuggingFace compatible   │
├──────────────────────────────────────────────────────────┤
│    Eigen3       ONNX Runtime (opt)       utf8proc        │
└──────────────────────────────────────────────────────────┘
```

## Algorithm Taxonomy

| Category | Algorithms | Pattern |
|----------|-----------|---------|
| Anomaly Detection | IsolationForest, LOF | fit → score |
| Clustering | DBSCAN, KMeans, GMM | fit → predict |
| Classification | LogisticRegression, KNN (classification) | fit(X, y) → predict |
| Regression | LinearRegression, KNN (regression) | fit(X, y) → predict |
| Time Series | HoltWinters, SeasonalDecomposition | fit → forecast/decompose |
| Dimensionality Reduction | PCA | fit → transform |
| Preprocessing | StandardScaler, MinMaxScaler, Imputer | fit → transform |
| Pipeline | MLPipeline | fit → predict (chains preprocessors + estimator) |
| Evaluation | CrossValidator, metric functions | split / compute metrics |
| Inference | OnnxModel | load → run |

## C++ Class Pattern

Every algorithm follows this pattern:

```cpp
class QoreAlgorithm : public AbstractPrivateData {
public:
    void fit(const MatrixXd& data, ExceptionSink* xsink);
    QoreHashNode* predict(const RowVectorXd& point, ExceptionSink* xsink) const;
    QoreListNode* predictMatrix(const MatrixXd& data, ExceptionSink* xsink) const;
    bool isFitted() const;
    void setFieldNames(const std::vector<std::string>& names);

    // Online learning (LinearRegression, LogisticRegression, GMM, KMeans)
    void update(const MatrixXd& data, ExceptionSink* xsink);

    // Serialization (all algorithms)
    BinaryNode* serialize(ExceptionSink* xsink) const;
    static QoreAlgorithm* deserialize(const BinaryNode* data, ExceptionSink* xsink);

private:
    mutable std::mutex mtx;
    bool fitted = false;
    std::vector<std::string> field_names;
};
```

Key conventions:
- **`mutable std::mutex mtx`** — protects all mutable state
- **`std::lock_guard<std::mutex>`** — used in all public methods
- **Dual API**: hash-based (`fit(list<hash>)`) and matrix-based (`fitMatrix(list<list<float>>)`)
- **Hashdecl results** — `new QoreHashNode(hashdeclFoo, xsink)` for typed returns
- **Online learning** — `update()` / `updateMatrix()` for incremental training

## QPP File Structure

Each algorithm has three files:
- `QC_Algorithm.h` — C++ class declaration
- `Algorithm.cpp` — C++ implementation
- `QC_Algorithm.qpp` — QPP Qore bindings (generates C++ code via qpp tool)

QPP files:
- Use `qclass Name [arg=QoreName* var; ns=Qore::ML; flags=final]`
- Declare extern hashdecl pointers for typed hash creation
- Follow doxygen documentation conventions with `@par Example:` blocks

## Module Registration (ml-module.cpp)

Registration follows a strict order:
1. `preinitAlgorithmClass()` — create class objects (no methods yet)
2. `init_hashdecl_ResultType(MLNS)` — initialize hashdecls, store in globals
3. `MLNS.addSystemClass(initAlgorithmClass(MLNS))` — add methods to classes
4. `init_ml_functions(MLNS)` — add namespace-level functions

## Metric Functions

Namespace-level functions in `ql_ml.qpp` for model evaluation:

| Category | Functions |
|----------|----------|
| Classification | `ml_accuracy`, `ml_confusion_matrix`, `ml_classification_report` |
| Regression | `ml_mse`, `ml_rmse`, `ml_mae`, `ml_r2_score`, `ml_explained_variance` |
| Clustering | `ml_silhouette_score`, `ml_davies_bouldin_score`, `ml_calinski_harabasz_score` |

Implemented in `ml_metrics.h` / `ml_metrics.cpp`.

## Serialization

All algorithms implement `serialize()` / `deserialize()` via the native `.qml` format.

- **Format**: Binary with `QML\x01` magic, CRC-32 checksum, algorithm-specific payload
- **Helpers**: `ml_serialization.h` / `ml_serialization.cpp` provide matrix/vector/scalar
  read/write primitives
- **Qore API**: `ml_save_model(path)`, `ml_load_model(path)`, `ml_serialize()`,
  `ml_deserialize()`

## CrossValidator

k-fold cross-validation with shuffle and stratification support. Returns per-fold metrics
and combined results. Implemented in `QC_CrossValidator.h` / `CrossValidator.cpp` /
`QC_CrossValidator.qpp`.

## DataProviderML Processor Pattern

Each processor class:
- Inherits `DataProvider::AbstractDataProvider`
- Declares `ConstructorOptions` const with `DataProviderOptionInfo` entries
- Implements `processRecordImpl()` for window accumulation
- Implements `flushRecordsImpl()` for end-of-stream processing
- Returns typed event hashdecl results

### Processor Registry

All processors are registered in `MLProcessorsDataProvider.qc` and `DataProviderML.qm`:

| Category | Processors |
|----------|-----------|
| Anomaly Detection | isolation-forest, lof |
| Clustering | dbscan, kmeans, gmm |
| Classification | logistic-regression, knn-classification |
| Regression | linear-regression, knn-regression |
| Time Series | holt-winters, seasonal-decomposition |
| Dimensionality Reduction | pca |
| Preprocessing | standard-scaler, min-max-scaler, imputer |
| Metrics | classification-metrics, regression-metrics, clustering-metrics |
| Inference | onnx-model |
| Registry | registry-model |

## QoreModelRegistry

A Qore user module (`qlib/QoreModelRegistry/`) for model lifecycle management.

### Architecture

```
qlib/QoreModelRegistry/
├── QoreModelRegistry.qm                  # Main module
├── ModelRegistry.qc                      # Core registry class (multi-tenant aware)
├── AbstractModelRegistryBackend.qc       # Abstract backend interface
├── FilesystemBackend.qc                  # Filesystem storage (default)
├── DatabaseBackend.qc                    # SQL database storage with SqlUtil
├── RestBackend.qc                        # REST API storage with LRU caching
├── ModelArtifactInspector.qc             # Safe package/artifact inspection
├── ModelRegistryDataProvider.qc          # DataProvider integration
└── ModelRegistryDataProviderFactory.qc   # Factory
```

### Key Features

- **Multi-tenant**: `tenant_id` option scopes all operations; tenant isolation enforced
- **Pluggable backends**: Filesystem (default), database (PostgreSQL/MySQL), REST
- **Version management**: Register, list, load, tag, prune model versions
- **Model comparison**: Side-by-side metric comparison of model versions
- **Package artifacts**: named artifacts for ONNX models, HuggingFace
  `tokenizer.json`, `config.json`, SafeTensors weights, generation config, and
  related metadata
- **Safe inspection**: JSON and SafeTensors headers are parsed; PyTorch pickle
  artifacts are classified as unsafe/source metadata and are never deserialized
- **Execution metadata**: `loadExecutable()` runs native `.qml` and ONNX packages
  and reports descriptive errors for metadata-only packages
- **Artifact access**: `loadArtifact()`, `loadNamedArtifact()`,
  `loadJsonArtifact()`, and `loadTokenizerConfig()` for raw and parsed artifacts
- **ONNX package contracts**: manifests carry runtime, entry artifact, tensor
  schemas, dynamic axes, tokenizer metadata, preprocessing and postprocessing
  contracts, output-shape maps, provider diagnostics, and optional
  `device_binding` policy

### Package Safety Contract

Qore can understand PyTorch/HuggingFace package structure without executing
arbitrary Python or pickle content. The registry records source framework,
artifact kind, runtime, safety classification, tokenizer metadata, and the
artifact list. Safe local execution requires either a native Qore `.qml`
artifact or an ONNX artifact; SafeTensors-only and PyTorch pickle packages stay
metadata-only until converted to ONNX or connected to an explicit external
runtime.

## QoreOnnxTools

`QoreOnnxTools` (`qlib/QoreOnnxTools/`) is the ONNX deployment tooling layer.
It provides library helpers and the `qore-onnx` command for inspecting,
validating, benchmarking, comparing, profiling, optimizing, quantizing, and
packaging ONNX artifacts.

### Key Components

- `OnnxModelTools` wraps model inspection, validation, benchmarking, compare,
  optimize, and Python-environment checks.
- `OnnxPackageTools` builds deterministic ONNX package directories and package
  manifests.
- `OnnxProcessingSpecs` builds and normalizes standard preprocessing,
  postprocessing, tokenizer, tensor-contract, and execution-plan metadata.
- `OnnxDiagnostics` creates compact actionable diagnostics used by CLI,
  registry, and DataProviderML surfaces.

## Tensor And Device Memory

Dense tensor data is represented with core `buffer<T>` values and
`ML::Tensor`. The core `QoreBufferNode` substrate supports Qore-owned host
storage, immutable external host storage, and immutable external device storage.
Device-backed buffers expose `deviceStorage()`, `hostStorage()`,
`deviceInfo()`, and `materialize()` pseudo-methods; `ML::Tensor` exposes the
same storage metadata at tensor level.

`ML::Tensor` rejects nullable buffers because ONNX/model tensors do not carry a
Qore validity bitmap. Device-backed tensors keep their native owner alive
through the wrapped buffer owner and materialize to host storage through a
module-provided copy callback when host reads are required.

Device upload APIs live in the `ml` module, not in core buffer methods:

- `Tensor::toDevice("cuda", id)` uploads host tensor data to CUDA memory when
  the module is built with CUDA runtime support.
- `Tensor::toDevice("metal", id)` allocates a Metal `MTLBuffer` with shared
  storage on Apple Silicon builds with Metal support.
- `Tensor::pinnedHost()` creates CUDA page-locked host storage when available;
  on Apple Silicon UMA it is an honest no-op that returns a tensor sharing the
  same host buffer.
- `Tensor::mockDevice()` is a test helper that tags copied host bytes as a
  device buffer so device paths can be tested without accelerator hardware.

Device-upload and device-binding entry points are in the
`UNCONTROLLED_API` functional domain, so programs parsed with
`PO_NO_UNCONTROLLED_APIS` cannot call them.

## ONNX Integration

- CMake detects ONNX Runtime via pkg-config or find_library.
- `HAVE_ONNXRUNTIME` gates ONNX implementation code while stub classes keep the
  Qore API loadable on CPU-only or ONNX-free builds.
- Hashdecls such as `OnnxTensorInfo`, `OnnxModelInfo`,
  `OnnxProviderConfig`, `OnnxSessionConfig`, and `OnnxDeviceBindingConfig`
  live in `ql_ml.qpp`.
- `ml_get_capabilities().has_onnx` reports ONNX Runtime availability, and
  `ml_get_capabilities().has_metal` reports Apple Metal/UMA upload support.
- `OnnxModel` supports path and in-memory model loading, direct runs,
  tensor-returning runs, batch runs, provider diagnostics, and inference
  transfer statistics.
- `OnnxIoBinding` provides reusable input/output bindings, including
  `bindOutputDevice()` and `bindOutputsDevice()` for provider-managed device
  output allocation.
- `OnnxRunOptions` wraps per-run ONNX Runtime options.
- `OnnxSessionPool` provides bounded reusable session pools with synchronous,
  asynchronous, and dynamic-batch execution plus pooled device-binding and
  optional device-memory statistics.
- DataProviderML conditionally registers the `onnx-model` processor.
- `QoreTokenizerUtils::EmbeddingModel` and `CrossEncoderReranker` accept ONNX
  model bytes as well as filesystem paths, so registry database/REST artifacts
  can be executed without temporary files
- DataProviderML text/embedding/reranking processors can load ONNX and tokenizer
  artifacts from QoreModelRegistry packages

### Provider And Device Binding Model

ONNX device binding is opt-in through `OnnxSessionConfig.device_binding` and
higher-level DataProviderML/registry options. The policy controls whether
outputs target CPU memory, the active provider device, or an explicit device;
whether host fallback is allowed; whether outputs are materialized before
returning; and whether zero-copy input/output behavior is required.

Provider behavior is split into three groups:

- **Raw-device-memory providers**: CUDA, TensorRT, and ROCm expose device
  memory that ONNX Runtime can bind. Compatible device inputs bind without a
  host copy, and provider-managed outputs can return as device-backed
  `ML::Tensor` values.
- **EP-acceleration providers**: CoreML, OpenVINO, DirectML, and WebGPU
  accelerate inference but manage opaque provider memory. Device-binding
  output requests resolve to host tensors without fabricated transfer counts;
  `provider_host_resolutions` makes that resolution observable.
- **CPU provider**: device output requests are configuration errors unless
  host fallback is explicitly allowed.

Apple Silicon has two paths: CoreML is an EP-acceleration provider, while
Metal/UMA tensors created with `Tensor::toDevice("metal")` use shared memory
that is readable by CPU, CoreML, and WebGPU paths without a DMA host/device
transfer. Transfer counters stay at zero for those UMA host/device copies.

DataProviderML's ONNX processor exposes `device_binding`, `output_device`,
`materialize_outputs`, `allow_host_fallback`, and `require_zero_copy` options
and reports transfer counters through diagnostics. QoreModelRegistry manifests
can carry `execution.device_binding`; `ModelExecutableAdapter` propagates it
into loaded ONNX sessions and includes the resulting diagnostics.

## Tokenizer Module

Separate binary module (`modules/tokenizer/`) providing HuggingFace-compatible text
tokenization.

### Key Features

- **Models**: BPE (GPT-2, Llama), WordPiece (BERT), Unigram/Viterbi (T5)
- **Full pipeline**: Normalizer → Pre-tokenizer → Model → Post-processor → Decoder
- **Features**: truncation, padding, batch encoding, offset mapping, special tokens mask,
  word_ids mapping, pre-tokenized input, sliding window overflow, dynamic vocabulary
  extension via `addTokens()`
- **Thread-safe**: `std::shared_mutex` for concurrent encode with dynamic vocab extension
- **Dependency**: utf8proc (external, dynamically linked)

## Protobuf / ONNX Relationship

The `protobuf` module remains the schema and serialization foundation for Qore's
ONNX tooling. Model package registration does not require protobuf parsing: the
registry stores and classifies ONNX artifacts as executable binary model files.
Detailed ONNX graph inspection/export remains in the ONNX/protobuf layers so the
registry can stay safe, fast, and backend-agnostic.

## Extension Guide: Adding a New Algorithm

1. Create `QC_NewAlgo.h` with the C++ class (extend `AbstractPrivateData`, add mutex)
2. Create `NewAlgo.cpp` with the implementation (use Eigen3 for matrix ops)
3. Create `QC_NewAlgo.qpp` with QPP bindings
4. Add hashdecl to `ql_ml.qpp` (extern in QPP, global in `ml-module.cpp`)
5. Register in `ml-module.cpp`: preinit → init hashdecl → addSystemClass
6. Add serialization support: implement `serialize()` / `deserialize()`, register in
   `ml_serialization.cpp` factory
7. Add online learning if applicable: `update()` / `updateMatrix()` methods
8. Add to `CMakeLists.txt` source lists
9. Add tests to `modules/ml/test/ml.qtest`
10. Create `QoreNewAlgoProcessor.qc` in DataProviderML
11. Register in `MLProcessorsDataProvider.qc` and `DataProviderML.qm`
12. Add tests to `examples/test/qlib/DataProviderML/DataProviderMLProcessors.qtest`
