# ML Module Architecture

## Overview

The Qore ML ecosystem provides machine learning capabilities through a two-layer architecture:

1. **`ml` binary module** (C++/Eigen3) — Thread-safe native ML algorithms
2. **`DataProviderML` user module** (Qore) — Data pipeline processor wrappers

## Design Principles

- **No Python dependency** — Native algorithms run entirely in C++ using Eigen3
- **Thread-safe** — All classes use `mutable std::mutex` for concurrent access
- **Fit-once, predict-concurrently** — After fitting, prediction methods are safe for concurrent calls
- **Window-oriented** — Pipeline processors accumulate records in windows before processing
- **Typed results** — All algorithms return typed hashdecl results for type safety
- **Optional ONNX** — ONNX Runtime support is `#ifdef HAVE_ONNXRUNTIME` guarded

## Module Layering

```
┌─────────────────────────────────────────┐
│         Qorus / Applications            │
├─────────────────────────────────────────┤
│     DataProviderML (Qore module)        │
│  Pipeline processors, action catalog    │
├─────────────────────────────────────────┤
│          ml (C++ binary module)         │
│  IsolationForest, LOF, DBSCAN, KMeans  │
│  GMM, LinearRegression, HoltWinters    │
│  PCA, SeasonalDecomposition, OnnxModel  │
├─────────────────────────────────────────┤
│    Eigen3          ONNX Runtime (opt)   │
└─────────────────────────────────────────┘
```

## Algorithm Taxonomy

| Category | Algorithms | Pattern |
|----------|-----------|---------|
| Anomaly Detection | IsolationForest, LOF | fit → score |
| Clustering | DBSCAN, KMeans, GMM | fit → predict |
| Regression | LinearRegression | fit(X, y) → predict |
| Time Series | HoltWinters, SeasonalDecomposition | fit → forecast/decompose |
| Dimensionality Reduction | PCA | fit → transform |
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

## QPP File Structure

Each algorithm has three files:
- `QC_Algorithm.h` — C++ class declaration
- `Algorithm.cpp` — C++ implementation
- `QC_Algorithm.qpp` — QPP Qore bindings (generates C++ code via qpp tool)

QPP files:
- Use `qclass Name [arg=QoreName* var; ns=ML; flags=final]`
- Declare extern hashdecl pointers for typed hash creation
- Follow doxygen documentation conventions with `@par Example:` blocks

## Module Registration (ml-module.cpp)

Registration follows a strict order:
1. `preinitAlgorithmClass()` — create class objects (no methods yet)
2. `init_hashdecl_ResultType(MLNS)` — initialize hashdecls, store in globals
3. `MLNS.addSystemClass(initAlgorithmClass(MLNS))` — add methods to classes
4. `init_ml_functions(MLNS)` — add namespace-level functions

## DataProviderML Processor Pattern

Each processor class:
- Inherits `DataProvider::AbstractDataProvider`
- Declares `ConstructorOptions` const with `DataProviderOptionInfo` entries
- Implements `processRecordImpl()` for window accumulation
- Implements `flushRecordsImpl()` for end-of-stream processing
- Returns typed event hashdecl results

## ONNX Integration

- CMake detects ONNX Runtime via pkg-config or find_library
- `HAVE_ONNXRUNTIME` define gates all ONNX code
- Hashdecls `OnnxTensorInfo` and `OnnxModelInfo` in `ql_ml.qpp` with `#ifdef`
- Class registration in `ml-module.cpp` with `#ifdef`
- `ml_get_capabilities().has_onnx` for runtime detection
- DataProviderML conditionally registers the onnx-model processor

## Extension Guide: Adding a New Algorithm

1. Create `QC_NewAlgo.h` with the C++ class (extend `AbstractPrivateData`, add mutex)
2. Create `NewAlgo.cpp` with the implementation (use Eigen3 for matrix ops)
3. Create `QC_NewAlgo.qpp` with QPP bindings
4. Add hashdecl to `ql_ml.qpp` (extern in QPP, global in `ml-module.cpp`)
5. Register in `ml-module.cpp`: preinit → init hashdecl → addSystemClass
6. Add to `CMakeLists.txt` source lists
7. Add tests to `modules/ml/test/ml.qtest`
8. Create `QoreNewAlgoProcessor.qc` in DataProviderML
9. Register in `MLProcessorsDataProvider.qc` and `DataProviderML.qm`
10. Add tests to `examples/test/qlib/DataProviderML/DataProviderMLProcessors.qtest`
