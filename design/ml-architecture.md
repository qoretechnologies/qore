# ML Module Architecture

## Overview

The Qore ML ecosystem provides machine learning capabilities through a multi-layer
architecture:

1. **`ml` binary module** (C++/Eigen3) — Thread-safe native ML algorithms, preprocessing,
   metrics, serialization, and cross-validation
2. **`DataProviderML` user module** (Qore) — Data pipeline processor wrappers for all
   algorithms plus metric and registry processors
3. **`QoreModelRegistry` user module** (Qore) — Model versioning, comparison, and
   deployment management with pluggable backends
4. **`tokenizer` binary module** (C++/utf8proc) — HuggingFace-compatible text tokenization

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
│               DataProviderML (Qore)                      │
│  Algorithm processors + preprocessing + metrics +        │
│  registry model processor                                │
├──────────────────────────────────────────────────────────┤
│             QoreModelRegistry (Qore)                     │
│  Model versioning, comparison, deployment management     │
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
├── ModelRegistryDataProvider.qc          # DataProvider integration
└── ModelRegistryDataProviderFactory.qc   # Factory
```

### Key Features

- **Multi-tenant**: `tenant_id` option scopes all operations; tenant isolation enforced
- **Pluggable backends**: Filesystem (default), database (PostgreSQL/MySQL), REST
- **Version management**: Register, list, load, tag, prune model versions
- **Model comparison**: Side-by-side metric comparison of model versions
- **Artifact access**: `loadArtifact()` for raw binary model data

## ONNX Integration

- CMake detects ONNX Runtime via pkg-config or find_library
- `HAVE_ONNXRUNTIME` define gates all ONNX code
- Hashdecls `OnnxTensorInfo` and `OnnxModelInfo` in `ql_ml.qpp` with `#ifdef`
- Class registration in `ml-module.cpp` with `#ifdef`
- `ml_get_capabilities().has_onnx` for runtime detection
- DataProviderML conditionally registers the onnx-model processor

## Tokenizer Module

Separate binary module (`modules/tokenizer/`) providing HuggingFace-compatible text
tokenization. See `design/ml-enhancement-plan.md` Phase 7 for details.

### Key Features

- **Models**: BPE (GPT-2, Llama), WordPiece (BERT), Unigram/Viterbi (T5)
- **Full pipeline**: Normalizer → Pre-tokenizer → Model → Post-processor → Decoder
- **Features**: truncation, padding, batch encoding, offset mapping, special tokens mask,
  word_ids mapping, pre-tokenized input, sliding window overflow, dynamic vocabulary
  extension via `addTokens()`
- **Thread-safe**: `std::shared_mutex` for concurrent encode with dynamic vocab extension
- **Dependency**: utf8proc (external, dynamically linked)

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
