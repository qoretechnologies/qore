# ML Enhancement Plan

## Overview

This document describes a 6-phase plan to significantly improve Qore's ML capabilities in
the `ml` binary module (C++/Eigen3) and `DataProviderML` user module (Qore). Each phase
builds incrementally on existing infrastructure and includes independent verification.

## Current State

- **ml module**: 10 native algorithms (IsolationForest, LOF, DBSCAN, KMeans, GMM,
  LinearRegression, HoltWinters, SeasonalDecomposition, PCA) + optional OnnxModel
- **DataProviderML module**: 9 pipeline processors with window-based accumulation
- **Architecture**: C++ with Eigen3 (no Python), thread-safe, typed hashdecl results,
  dual API (hash-based + matrix-based)

## Architecture After Enhancement

```
┌──────────────────────────────────────────────────────────┐
│                Qorus / Applications                      │
├──────────────────────────────────────────────────────────┤
│               DataProviderML (Qore)                      │
│  Existing processors + preprocessing + metric processors │
├──────────────────────────────────────────────────────────┤
│             QoreModelRegistry (Qore)                     │
│  Model versioning, comparison, deployment management     │
├──────────────────────────────────────────────────────────┤
│                 ml (C++ binary)                           │
│  Existing algorithms + LogisticRegression + KNN          │
│  + StandardScaler + MinMaxScaler + Imputer + Pipeline    │
│  + Metrics (accuracy, F1, silhouette, MSE, ...)          │
│  + ModelSerializer (native, ONNX export, JSON)           │
│  + Online learning (LinearRegression, GMM)               │
├──────────────────────────────────────────────────────────┤
│    Eigen3          ONNX Runtime (opt)    libprotobuf*    │
└──────────────────────────────────────────────────────────┘
* libprotobuf only needed for ONNX export; optional like ONNX Runtime
```

---

## Phase 1: Data Preprocessing Pipeline

### Motivation

Users must normalize, scale, and clean data manually before calling `fit()`. This is
error-prone and tedious, especially when the same preprocessing must be applied consistently
at training and prediction time.

### Components

#### 1.1 StandardScaler (C++)

Transforms features to zero mean and unit variance.

```cpp
class QoreStandardScaler : public AbstractPrivateData {
public:
    void fit(const MatrixXd& data, ExceptionSink* xsink);
    MatrixXd transform(const MatrixXd& data, ExceptionSink* xsink) const;
    MatrixXd inverseTransform(const MatrixXd& data, ExceptionSink* xsink) const;
    MatrixXd fitTransform(const MatrixXd& data, ExceptionSink* xsink);
    bool isFitted() const;

private:
    VectorXd mean_vec;
    VectorXd std_vec;
    int n_features = 0;
    bool fitted = false;
    mutable std::mutex mtx;
    std::vector<std::string> field_names;
};
```

**QPP bindings**: `fit(list<hash>)`, `fitMatrix(list<auto>)`, `transform(hash)` → `hash`,
`transformMatrix(list<auto>)` → `list<list<float>>`, `inverseTransform()`, `fitTransform()`.

**Hashdecl**: `StandardScalerInfo` — `mean` (list<float>), `std` (list<float>),
`n_features` (int).

#### 1.2 MinMaxScaler (C++)

Scales features to a configurable range (default [0, 1]).

```cpp
class QoreMinMaxScaler : public AbstractPrivateData {
    // Options: feature_min (default 0.0), feature_max (default 1.0)
    VectorXd data_min;
    VectorXd data_max;
    double feature_min, feature_max;
    // Same API pattern as StandardScaler
};
```

**Hashdecl**: `MinMaxScalerInfo` — `data_min`, `data_max`, `feature_min`, `feature_max`.

#### 1.3 Imputer (C++)

Fills missing values using a configurable strategy.

```cpp
class QoreImputer : public AbstractPrivateData {
    // Strategy: "mean", "median", "most_frequent", "constant"
    // fit() computes fill values; transform() applies them
    std::string strategy;
    VectorXd fill_values;       // computed during fit
    double constant_value;      // for "constant" strategy
};
```

**Detection**: A field value of `NOTHING`/`NULL` or `NaN` is treated as missing.

#### 1.4 MLPipeline (C++)

Chains preprocessors and an estimator into a single fit/predict unit.

```cpp
class QoreMLPipeline : public AbstractPrivateData {
public:
    // steps: list of (name, transformer/estimator) pairs
    // The last step may be an estimator (has predict); all others must be transformers
    void fit(const MatrixXd& X, const VectorXd* y, ExceptionSink* xsink);
    QoreValue predict(const RowVectorXd& point, ExceptionSink* xsink) const;

private:
    struct Step {
        std::string name;
        enum Type { SCALER, IMPUTER, ESTIMATOR } type;
        AbstractPrivateData* obj;  // borrowed ref, caller owns
    };
    std::vector<Step> steps;
};
```

**Qore API**:
```qore
MLPipeline p(("scaler", new StandardScaler(), "model", new LinearRegression()));
p.fit(training_data, target_data);
auto result = p.predict(new_record);
```

The pipeline ensures that `transform()` is called on each preprocessor before the data
reaches the estimator, and the same transformation chain is applied at prediction time.

#### 1.5 DataProvider Processors

- `standard-scaler` processor: scales fields in streaming records
- `min-max-scaler` processor: min-max normalization
- `imputer` processor: fill missing values

Each follows the existing window-accumulation pattern: fit on the first window, then
transform all subsequent records.

### Files to Create/Modify

| File | Action |
|------|--------|
| `modules/ml/src/QC_StandardScaler.h` | New — class declaration |
| `modules/ml/src/StandardScaler.cpp` | New — implementation |
| `modules/ml/src/QC_StandardScaler.qpp` | New — QPP bindings |
| `modules/ml/src/QC_MinMaxScaler.h` | New — class declaration |
| `modules/ml/src/MinMaxScaler.cpp` | New — implementation |
| `modules/ml/src/QC_MinMaxScaler.qpp` | New — QPP bindings |
| `modules/ml/src/QC_Imputer.h` | New — class declaration |
| `modules/ml/src/Imputer.cpp` | New — implementation |
| `modules/ml/src/QC_Imputer.qpp` | New — QPP bindings |
| `modules/ml/src/QC_MLPipeline.h` | New — class declaration |
| `modules/ml/src/MLPipeline.cpp` | New — implementation |
| `modules/ml/src/QC_MLPipeline.qpp` | New — QPP bindings |
| `modules/ml/src/ql_ml.qpp` | Modify — add hashdecls |
| `modules/ml/src/ml-module.cpp` | Modify — register new classes |
| `modules/ml/CMakeLists.txt` | Modify — add source files |
| `qlib/DataProviderML/QoreStandardScalerProcessor.qc` | New |
| `qlib/DataProviderML/QoreMinMaxScalerProcessor.qc` | New |
| `qlib/DataProviderML/QoreImputerProcessor.qc` | New |
| `qlib/DataProviderML/DataProviderML.qm` | Modify — register processors |
| `qlib/DataProviderML/MLProcessorsDataProvider.qc` | Modify — add to nav |

### Independent Verification

1. **Unit tests** (`modules/ml/test/ml.qtest`):
   - StandardScaler: verify mean≈0, std≈1 after transform; inverse recovers original;
     fit on one dataset, transform another
   - MinMaxScaler: verify all values in [min, max] after transform; custom ranges;
     inverse recovers original
   - Imputer: verify NaN/NOTHING replaced; each strategy tested; new unseen NaN handled
   - MLPipeline: verify pipeline(scaler + model) matches manual scaler.fit→transform→model.fit
   - Edge cases: single feature, single sample, all-zero variance, all-missing column
   - Thread safety: concurrent transform after fit

2. **Numerical verification**: Compare StandardScaler output against hand-computed
   values for a small dataset (e.g., 5 samples × 3 features with known mean/std).

3. **DataProvider tests** (`examples/test/qlib/DataProviderML/DataProviderMLProcessors.qtest`):
   - Each processor: window accumulation, field selection, event output format
   - Pipeline consistency: same data through processor vs direct API gives identical output

4. **Valgrind**: Run all ML tests under valgrind to verify no memory leaks.

---

## Phase 2: Model Evaluation Metrics

### Motivation

Without standardized metrics, users cannot objectively evaluate or compare models.
LinearRegression reports R², but there are no general classification, regression, or
clustering metrics.

### Components

#### 2.1 Classification Metrics (C++ namespace functions)

```cpp
// All functions take ground-truth labels and predicted labels as Eigen vectors
double ml_accuracy(const VectorXd& y_true, const VectorXd& y_pred);
QoreHashNode* ml_confusion_matrix(const VectorXd& y_true, const VectorXd& y_pred, ...);
double ml_precision(const VectorXd& y_true, const VectorXd& y_pred, ...);
double ml_recall(const VectorXd& y_true, const VectorXd& y_pred, ...);
double ml_f1_score(const VectorXd& y_true, const VectorXd& y_pred, ...);
```

**Hashdecls**:
```
hashdecl ConfusionMatrixResult {
    list<list<int>> matrix;     // n_classes × n_classes
    list<string> labels;        // class labels
}

hashdecl ClassificationReport {
    float accuracy;
    list<hash<ClassMetrics>> per_class;   // precision, recall, f1 per class
    float macro_precision;
    float macro_recall;
    float macro_f1;
    float weighted_f1;
}

hashdecl ClassMetrics {
    string label;
    float precision;
    float recall;
    float f1;
    int support;    // number of true instances
}
```

#### 2.2 Regression Metrics

```cpp
double ml_mse(const VectorXd& y_true, const VectorXd& y_pred);
double ml_rmse(const VectorXd& y_true, const VectorXd& y_pred);
double ml_mae(const VectorXd& y_true, const VectorXd& y_pred);
double ml_r2_score(const VectorXd& y_true, const VectorXd& y_pred);
double ml_explained_variance(const VectorXd& y_true, const VectorXd& y_pred);
```

#### 2.3 Clustering Metrics

```cpp
double ml_silhouette_score(const MatrixXd& data, const VectorXi& labels);
double ml_davies_bouldin_score(const MatrixXd& data, const VectorXi& labels);
double ml_calinski_harabasz_score(const MatrixXd& data, const VectorXi& labels);
```

#### 2.4 Cross-Validation Utility

```cpp
// k-fold split utility: returns list of (train_indices, test_indices) pairs
class QoreCrossValidator : public AbstractPrivateData {
public:
    QoreCrossValidator(int n_folds, bool shuffle, int64_t seed);
    QoreListNode* split(int n_samples, ExceptionSink* xsink);
};
```

**Qore API**:
```qore
auto cv = new ML::CrossValidator({"n_folds": 5, "shuffle": True, "seed": 42});
list<hash<auto>> folds = cv.split(data.size());
# Each fold: {"train_indices": (0, 1, 3, ...), "test_indices": (2, 7, ...)}
```

#### 2.5 DataProvider Metric Processors

- `classification-metrics` processor: accumulates predictions + ground truth, emits
  ClassificationReport on flush
- `regression-metrics` processor: accumulates predictions + actuals, emits regression
  metrics on flush
- `clustering-metrics` processor: accumulates data + cluster assignments, emits
  clustering metrics on flush

### Files to Create/Modify

| File | Action |
|------|--------|
| `modules/ml/src/ml_metrics.h` | New — metric function declarations |
| `modules/ml/src/ml_metrics.cpp` | New — metric implementations |
| `modules/ml/src/QC_CrossValidator.h` | New — class declaration |
| `modules/ml/src/CrossValidator.cpp` | New — implementation |
| `modules/ml/src/QC_CrossValidator.qpp` | New — QPP bindings |
| `modules/ml/src/ql_ml.qpp` | Modify — add metric functions and hashdecls |
| `modules/ml/src/ml-module.cpp` | Modify — register |
| `modules/ml/CMakeLists.txt` | Modify — add source files |
| `qlib/DataProviderML/QoreClassificationMetricsProcessor.qc` | New |
| `qlib/DataProviderML/QoreRegressionMetricsProcessor.qc` | New |
| `qlib/DataProviderML/QoreClusteringMetricsProcessor.qc` | New |
| `qlib/DataProviderML/DataProviderML.qm` | Modify |
| `qlib/DataProviderML/MLProcessorsDataProvider.qc` | Modify |

### Independent Verification

1. **Reference value tests**: Compute expected values by hand for small datasets:
   - accuracy, precision, recall, F1 for a 3×3 confusion matrix
   - MSE, RMSE, MAE, R² for 5 known (y_true, y_pred) pairs
   - Silhouette score for 2 well-separated clusters of 3 points each

2. **Cross-validation consistency**: Verify that all samples appear in exactly one
   test fold, and each fold has approximately equal size.

3. **Metric identity properties**:
   - `accuracy(y, y) == 1.0` (perfect predictions)
   - `mse(y, y) == 0.0`
   - `r2_score(y, y) == 1.0`
   - `silhouette_score` for perfectly separated clusters ≈ 1.0

4. **Integration test**: Train LinearRegression → predict → `ml_r2_score()` matches
   the model's internal R². Train KMeans → predict → `ml_silhouette_score()` returns
   a reasonable value.

5. **DataProvider tests**: metric processors accumulate correctly, emit proper events.

6. **Valgrind**: Full memory check.

---

## Phase 3: New Algorithms — Logistic Regression and k-NN

### Motivation

Classification is the most common supervised ML task, and Qore currently has no
classification algorithms. Logistic Regression and k-NN are the two most fundamental
classifiers, covering different use cases (linear vs. instance-based).

### Components

#### 3.1 LogisticRegression (C++)

Binary and multiclass classification via gradient descent.

```cpp
class QoreLogisticRegression : public AbstractPrivateData {
public:
    QoreLogisticRegression(double learning_rate, int max_iterations,
        double tolerance, double regularization, const std::string& penalty,
        bool fit_intercept);

    void fit(const MatrixXd& X, const VectorXd& y, ExceptionSink* xsink);
    QoreHashNode* predict(const RowVectorXd& point, ExceptionSink* xsink) const;
    QoreListNode* predictMatrix(const MatrixXd& X, ExceptionSink* xsink) const;

private:
    // Binary: single weight vector
    // Multiclass: one weight vector per class (one-vs-rest)
    MatrixXd weights;       // n_classes × n_features (or 1 × n_features for binary)
    VectorXd intercepts;
    std::vector<double> classes;  // unique class labels
    bool is_binary = false;
    // Hyperparameters
    double learning_rate;
    int max_iterations;
    double tolerance;
    double regularization;  // L2 regularization strength
    std::string penalty;    // "l2", "none"
    bool fit_intercept;
};
```

**Hashdecl**:
```
hashdecl LogisticRegressionResult {
    float predicted_class;          // most likely class label
    float max_probability;          // probability of predicted class
    list<float> probabilities;      // probability for each class
    list<float> classes;            // class labels (same order as probabilities)
}
```

**Key implementation details**:
- Sigmoid activation for binary; softmax for multiclass
- L-BFGS or mini-batch gradient descent (Eigen provides efficient matrix ops)
- One-vs-rest (OvR) decomposition for multiclass when using sigmoid
- `predict_proba()` returns probabilities alongside class predictions

#### 3.2 KNN (C++)

k-Nearest Neighbors for classification and regression.

```cpp
class QoreKNN : public AbstractPrivateData {
public:
    QoreKNN(int k, const std::string& metric, const std::string& task,
        const std::string& weight_func);

    void fit(const MatrixXd& X, const VectorXd& y, ExceptionSink* xsink);
    QoreHashNode* predict(const RowVectorXd& point, ExceptionSink* xsink) const;
    QoreListNode* predictMatrix(const MatrixXd& X, ExceptionSink* xsink) const;

private:
    int k;
    std::string metric;      // "euclidean", "manhattan", "cosine"
    std::string task;         // "classification", "regression"
    std::string weight_func;  // "uniform", "distance"
    MatrixXd ref_data;        // stored training data
    VectorXd ref_labels;      // stored training labels
};
```

**Hashdecls**:
```
hashdecl KNNClassificationResult {
    float predicted_class;
    float confidence;               // proportion of k neighbors with this class
    list<hash<KNNNeighborInfo>> neighbors;  // k nearest neighbors (optional detail)
}

hashdecl KNNRegressionResult {
    float prediction;               // weighted average of neighbor values
    list<hash<KNNNeighborInfo>> neighbors;
}

hashdecl KNNNeighborInfo {
    int index;
    float distance;
    float label;
}
```

**Key implementation details**:
- Brute-force distance computation (efficient for moderate datasets with Eigen)
- Weighted voting: "uniform" (majority) or "distance" (inverse-distance weighted)
- Dual-purpose: classification (majority vote) or regression (weighted average)

#### 3.3 DataProvider Processors

- `logistic-regression` processor: window-based, binary/multiclass classification
- `knn-classification` processor: stores training window, classifies new records
- `knn-regression` processor: stores training window, predicts continuous values

### Files to Create/Modify

| File | Action |
|------|--------|
| `modules/ml/src/QC_LogisticRegression.h` | New |
| `modules/ml/src/LogisticRegression.cpp` | New |
| `modules/ml/src/QC_LogisticRegression.qpp` | New |
| `modules/ml/src/QC_KNN.h` | New |
| `modules/ml/src/KNN.cpp` | New |
| `modules/ml/src/QC_KNN.qpp` | New |
| `modules/ml/src/ql_ml.qpp` | Modify — hashdecls |
| `modules/ml/src/ml-module.cpp` | Modify — register |
| `modules/ml/CMakeLists.txt` | Modify |
| `qlib/DataProviderML/QoreLogisticRegressionProcessor.qc` | New |
| `qlib/DataProviderML/QoreKNNClassificationProcessor.qc` | New |
| `qlib/DataProviderML/QoreKNNRegressionProcessor.qc` | New |
| `qlib/DataProviderML/DataProviderML.qm` | Modify |
| `qlib/DataProviderML/MLProcessorsDataProvider.qc` | Modify |

### Independent Verification

1. **Known-answer tests**:
   - LogisticRegression on linearly separable 2D data → 100% accuracy
   - LogisticRegression on XOR-like data → accuracy around 50% (expected failure for
     linear model — verifies it doesn't overfit)
   - KNN with k=1 on training data → 100% accuracy (memorization property)
   - KNN regression on constant-value data → all predictions equal the constant

2. **Reference comparison**: Train on the Iris-like dataset (3 classes, 4 features):
   - LogisticRegression should achieve >90% accuracy
   - KNN (k=5) should achieve >90% accuracy
   - Compare against hand-computed softmax probabilities for 2-3 sample points

3. **Metric integration**: Train → predict → `ml_accuracy()` → verify against
   manual count of correct predictions.

4. **Multiclass verification**: 3+ classes, verify probability vectors sum to 1.0
   (within floating-point tolerance).

5. **Edge cases**: single class (degenerate), k > n_samples, empty features, single
   training sample.

6. **Thread safety**: concurrent predictions after fit.

7. **Valgrind**: Full memory check.

---

## Phase 4: Online Learning Extensions

### Motivation

Currently only KMeans supports online learning (`update()`). Extending this to
LinearRegression and GMM enables continuous model refinement on streaming data without
full retraining. HoltWinters already has `update()` for sequential observations.

### Components

#### 4.1 LinearRegression Online Learning

Add Stochastic Gradient Descent (SGD) update to LinearRegression.

```cpp
// New methods on QoreLinearRegression
void update(const MatrixXd& X, const VectorXd& y, double learning_rate,
    ExceptionSink* xsink);
void updateSingle(const RowVectorXd& x, double y, double learning_rate,
    ExceptionSink* xsink);
```

**Implementation**: Standard SGD update rule:
```
w = w - lr * (predicted - actual) * x
b = b - lr * (predicted - actual)
```

The learning rate can be constant or use a schedule (1/sqrt(t)).

#### 4.2 GMM Online Learning

Add incremental EM update to GMM.

```cpp
// New method on QoreGMM
void update(const MatrixXd& data, ExceptionSink* xsink);
```

**Implementation**: Online EM with sufficient statistics:
- Maintain running sums for responsibilities, means, covariances
- Each `update()` call performs a partial E-step on the new data, then updates the
  sufficient statistics and re-derives parameters
- Learning rate η = 1/(batch_count + 1) for convergence

#### 4.3 LogisticRegression Online Learning

Include online learning from the start in the Phase 3 implementation.

```cpp
void update(const MatrixXd& X, const VectorXd& y, double learning_rate,
    ExceptionSink* xsink);
```

**Implementation**: Mini-batch SGD on the cross-entropy loss.

#### 4.4 DataProvider Integration

Update existing processors to support online mode:
- `linear-regression` processor: add `online` option (like KMeans already has)
- `gmm` processor: add `online` option
- `logistic-regression` processor: add `online` option

### Files to Create/Modify

| File | Action |
|------|--------|
| `modules/ml/src/QC_LinearRegression.h` | Modify — add update methods |
| `modules/ml/src/LinearRegression.cpp` | Modify — implement SGD update |
| `modules/ml/src/QC_LinearRegression.qpp` | Modify — add QPP bindings |
| `modules/ml/src/QC_GMM.h` | Modify — add update method |
| `modules/ml/src/GMM.cpp` | Modify — implement online EM |
| `modules/ml/src/QC_GMM.qpp` | Modify — add QPP bindings |
| `modules/ml/src/QC_LogisticRegression.h` | Modify (if Phase 3 didn't include it) |
| `modules/ml/src/LogisticRegression.cpp` | Modify |
| `modules/ml/src/QC_LogisticRegression.qpp` | Modify |
| `qlib/DataProviderML/QoreLinearRegressionProcessor.qc` | Modify — add online option |
| `qlib/DataProviderML/QoreGMMProcessor.qc` | Modify — add online option |
| `qlib/DataProviderML/QoreLogisticRegressionProcessor.qc` | Modify — add online option |

### Independent Verification

1. **Convergence test**: Generate linear data (y = 2x + 3 + noise). Fit batch
   LinearRegression, note coefficients. Then create a fresh model, call `update()` in
   many mini-batches on the same data — verify coefficients converge to approximately
   the same values as batch fit.

2. **Monotonic improvement**: For each update batch, track the loss (MSE for regression,
   cross-entropy for classification). With a small enough learning rate, the loss should
   decrease monotonically (or at least trend downward).

3. **Consistency**: `fit()` on data1 + `update()` on data2 should produce different
   coefficients than `fit()` on data1 alone — verify the update actually changes the
   model.

4. **GMM online**: Fit GMM on data1 (2 clusters). Generate data2 from a shifted
   distribution. After `update()`, means should move toward the new data.

5. **DataProvider integration**: Pipeline with `online=True` processes multiple
   windows; verify the model state changes between windows (examine output cluster
   assignments or predictions).

6. **Thread safety**: concurrent `update()` calls should not corrupt state (mutex
   serializes them).

7. **Valgrind**: Full memory check.

---

## Phase 5: Model Persistence and Export

### Motivation

This is the foundational capability for production ML: trained models must be saved,
loaded, shared, and deployed. Without persistence, every restart retrains from scratch.
A generic persistence layer with pluggable format backends enables both fast native
save/load and cross-platform interoperability via ONNX and other formats.

### Architecture

```
                      Qore API
                         │
              ┌──────────┴──────────┐
              │   ModelSerializer    │  (C++ — abstract interface)
              │   save() / load()   │
              └──────────┬──────────┘
                         │
         ┌───────────────┼───────────────┐
         │               │               │
    ┌────┴────┐    ┌─────┴─────┐   ┌─────┴─────┐
    │ Native  │    │   ONNX    │   │   JSON    │
    │ (.qml)  │    │  (.onnx)  │   │  (.json)  │
    └─────────┘    └───────────┘   └───────────┘
```

### Components

#### 5.1 Generic Persistence Interface (C++)

```cpp
//! Abstract serialization interface — each algorithm implements this
class MLSerializable {
public:
    virtual ~MLSerializable() = default;

    //! Serialize model state to a byte buffer
    virtual BinaryNode* serialize(ExceptionSink* xsink) const = 0;

    //! Deserialize model state from a byte buffer (static factory pattern)
    //! Each algorithm registers a deserializer function
    // See ml_serialization.h for the registry

    //! Return algorithm type name (e.g. "KMeans", "LinearRegression")
    virtual const char* algorithmName() const = 0;

    //! Return model metadata as a hash (hyperparams, feature count, etc.)
    virtual QoreHashNode* getMetadata(ExceptionSink* xsink) const = 0;
};
```

**Native format (.qml)**:

A compact binary format with a header and algorithm-specific payload.

```
┌─────────────────────────────────────────────┐
│  Magic: "QML\x01"            (4 bytes)      │
│  Format version: uint16      (2 bytes)      │
│  Algorithm name length: uint16              │
│  Algorithm name: UTF-8 string               │
│  Hyperparameter JSON length: uint32         │
│  Hyperparameter JSON: UTF-8                 │
│  Field names JSON length: uint32            │
│  Field names JSON: UTF-8 (or 0 if none)    │
│  Model data length: uint64                  │
│  Model data: algorithm-specific binary      │
│    (Eigen matrices stored row-major with    │
│     rows, cols, then flat double[])         │
│  CRC-32 checksum: uint32                    │
└─────────────────────────────────────────────┘
```

Each algorithm's `serialize()` writes its Eigen matrices and scalar state into the
"model data" section. The format is versioned so future changes are backward-compatible.

**Serialization helper** (`ml_serialization.h`):

```cpp
namespace MLSerialization {
    // Write an Eigen matrix to a buffer: rows(int32), cols(int32), data(double[])
    void writeMatrix(std::vector<uint8_t>& buf, const MatrixXd& mat);
    MatrixXd readMatrix(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink);

    void writeVector(std::vector<uint8_t>& buf, const VectorXd& vec);
    VectorXd readVector(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink);

    void writeScalar(std::vector<uint8_t>& buf, double val);
    double readScalar(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink);

    void writeInt32(std::vector<uint8_t>& buf, int32_t val);
    int32_t readInt32(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink);

    void writeString(std::vector<uint8_t>& buf, const std::string& s);
    std::string readString(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink);

    void writeStringVector(std::vector<uint8_t>& buf, const std::vector<std::string>& v);
    std::vector<std::string> readStringVector(const uint8_t*& ptr, size_t& remaining,
        ExceptionSink* xsink);

    uint32_t computeCRC32(const uint8_t* data, size_t len);
}
```

**Qore-level API** (namespace functions):

```qore
# Save a model to file (format detected from extension)
ML::ml_save_model(ML::KMeans model, string path, *hash<auto> options);

# Load a model from file (returns the algorithm object, ready for predict)
auto model = ML::ml_load_model(string path);

# Serialize to binary (in-memory, no file)
binary data = ML::ml_serialize(ML::KMeans model);
auto model = ML::ml_deserialize(binary data);
```

The `ml_save_model()` function detects the format from the file extension:
- `.qml` → native format
- `.onnx` → ONNX export
- `.json` → JSON format

An explicit `format` option overrides extension-based detection.

#### 5.2 Per-Algorithm Serialization

Each algorithm implements `serialize()` / static `deserialize()`. The state to serialize
for each algorithm:

| Algorithm | Serialized State |
|-----------|-----------------|
| **KMeans** | k, centroids (MatrixXd), centroid_counts, n_features, inertia, field_names, init_method, max_iterations, tolerance |
| **LinearRegression** | coefficients (VectorXd), intercept, r_squared, n_features, fit_intercept, do_normalize, feature_means, feature_stds, field_names, target_field |
| **PCA** | components (MatrixXd), mean_vec, stddev_vec, explained_variance_ratio_vec, n_features, actual_n_components, center, scale, field_names |
| **IsolationForest** | trees (vector<IsolationTree>: each tree's nodes with feature, split_value, left, right, size), n_features, actual_sample_size, threshold, n_trees, sample_size, max_depth, field_names |
| **LOF** | ref_data (MatrixXd), ref_k_distances, ref_lrds, ref_knn_indices, k, threshold, n_features, field_names |
| **GMM** | means (vector<VectorXd>), covariances (vector<MatrixXd>), weights (VectorXd), n_components, n_features, covariance_type, field_names |
| **HoltWinters** | level, trend, seasonal (vector<double>), t, period, alpha, beta, gamma, seasonal_type, damped, phi |
| **SeasonalDecomposition** | period, type (stateless algorithm — only hyperparams needed) |
| **DBSCAN** | epsilon, min_points, metric (stateless — re-clusters each time; nothing to persist beyond hyperparams) |
| **LogisticRegression** | weights (MatrixXd), intercepts (VectorXd), classes, n_features, is_binary, hyperparams, field_names |
| **KNN** | ref_data (MatrixXd), ref_labels (VectorXd), k, metric, task, weight_func, n_features, field_names |
| **StandardScaler** | mean_vec, std_vec, n_features, field_names |
| **MinMaxScaler** | data_min, data_max, feature_min, feature_max, n_features, field_names |
| **Imputer** | fill_values, strategy, constant_value, n_features, field_names |

Note: DBSCAN and SeasonalDecomposition are stateless (they operate on input data directly
without maintaining a trained model). Their persistence stores only hyperparameters
so they can be reconstructed, but there is no trained state to save.

#### 5.3 ONNX Export Backend

Export trained native models to ONNX format for deployment in any ONNX Runtime-compatible
environment (Python, C#, Java, JavaScript, edge devices).

**Dependency**: ONNX uses Protocol Buffers (protobuf). The ONNX .proto files define the
model schema. We use the ONNX protobuf definitions directly (header-only or via
libprotobuf).

**CMake integration**: `find_package(Protobuf)` — optional like ONNX Runtime. ONNX
export is `#ifdef HAVE_PROTOBUF` guarded.

**Algorithm-to-ONNX operator mapping**:

| Algorithm | ONNX Operators | ONNX-ML Operators |
|-----------|---------------|-------------------|
| **LinearRegression** | MatMul + Add | ai.onnx.ml.LinearRegressor |
| **LogisticRegression** | MatMul + Add + Sigmoid/Softmax | ai.onnx.ml.LinearClassifier |
| **PCA** | Sub (center) + MatMul (project) | — (standard ops) |
| **KMeans** | — | ai.onnx.ml.KMeansPredictor* |
| **StandardScaler** | Sub + Div | ai.onnx.ml.Scaler |
| **MinMaxScaler** | Sub + Div + Mul + Add | ai.onnx.ml.Scaler |
| **KNN** | — | ai.onnx.ml.KNearestNeighbors* |
| **IsolationForest** | — | ai.onnx.ml.TreeEnsembleRegressor (score) |
| **GMM** | Complex subgraph | — (not natively supported) |
| **HoltWinters** | — | — (stateful time-series, no ONNX mapping) |
| **LOF** | — | — (requires reference data, no standard mapping) |

*Note: KMeans and KNN do not have standard ONNX-ML operators but can be expressed as
computation graphs. Alternatively, we can use custom operator domains.*

**Algorithms that export cleanly to ONNX**: LinearRegression, LogisticRegression, PCA,
StandardScaler, MinMaxScaler, IsolationForest (via TreeEnsembleRegressor), KMeans
(via distance computation graph).

**Algorithms that do NOT export to ONNX** (and throw ML-ONNX-EXPORT-ERROR): HoltWinters,
SeasonalDecomposition, LOF, DBSCAN (stateless), GMM (complex covariance computations
not worth the graph complexity). These models can still be saved in native and JSON formats.

**Export API**:

```cpp
class OnnxExporter {
public:
    // Export a model to ONNX format
    static BinaryNode* exportModel(const MLSerializable* model,
        const std::string& producer_name, int64_t opset_version,
        ExceptionSink* xsink);

    // Export a pipeline (preprocessors + estimator) as a single ONNX graph
    static BinaryNode* exportPipeline(const QoreMLPipeline* pipeline,
        const std::string& producer_name, int64_t opset_version,
        ExceptionSink* xsink);
};
```

**Pipeline export**: The MLPipeline's chain of preprocessors + estimator is exported
as a single ONNX graph where each step's operators are concatenated. This is the most
natural ONNX representation and avoids the need for multiple model files.

#### 5.4 JSON Export Backend

Human-readable format for debugging, inspection, and web service integration.

```json
{
    "format": "qore-ml-json",
    "version": 1,
    "algorithm": "KMeans",
    "hyperparameters": {
        "k": 3,
        "max_iterations": 100,
        "tolerance": 0.0001,
        "init": "kmeans++"
    },
    "field_names": ["sepal_length", "sepal_width", "petal_length", "petal_width"],
    "model_state": {
        "n_features": 4,
        "centroids": [[5.0, 3.4, 1.5, 0.2], [5.9, 2.8, 4.3, 1.3], [6.6, 3.0, 5.5, 2.0]],
        "centroid_counts": [50, 50, 50],
        "inertia": 78.94
    },
    "metadata": {
        "created": "2026-03-20T10:30:00Z",
        "ml_module_version": "1.2",
        "eigen_version": "3.4.0"
    }
}
```

**Implementation**: Uses Qore's built-in JSON serialization. Each algorithm implements
`toJSON()` / static `fromJSON()`. Eigen matrices are serialized as nested lists of
numbers.

The JSON backend does NOT require any external dependencies — it uses the
json module already available in Qore.

#### 5.5 ONNX Import Enhancement

Currently `OnnxModel` loads any `.onnx` file for inference. Enhance it to recognize
Qore-exported ONNX models (via producer metadata) and optionally reconstruct the
native algorithm object for further training/updating:

```qore
# Load as OnnxModel (inference only — already works)
auto onnx = new ML::OnnxModel("model.onnx");

# Load as native algorithm (if exported by Qore)
auto model = ML::ml_load_model("model.onnx");
# Returns a KMeans/LinearRegression/etc. if the ONNX model was exported by Qore
# Returns an OnnxModel otherwise
```

This is best-effort: ONNX models exported by other tools remain as `OnnxModel`.

### Files to Create/Modify

| File | Action |
|------|--------|
| `modules/ml/src/ml_serialization.h` | New — serialization primitives + registry |
| `modules/ml/src/ml_serialization.cpp` | New — implementations |
| `modules/ml/src/ml_onnx_export.h` | New — ONNX export (ifdef HAVE_PROTOBUF) |
| `modules/ml/src/ml_onnx_export.cpp` | New — ONNX graph construction |
| `modules/ml/src/ml_json_export.h` | New — JSON export/import |
| `modules/ml/src/ml_json_export.cpp` | New — implementations |
| `modules/ml/src/ql_ml.qpp` | Modify — add save/load/serialize functions |
| `modules/ml/src/ml-module.cpp` | Modify — register deserializer factories |
| `modules/ml/CMakeLists.txt` | Modify — add sources, optional protobuf dep |
| Every `QC_*.h` | Modify — add MLSerializable base, serialize/deserialize |
| Every `*.cpp` | Modify — implement serialize/deserialize |
| `modules/ml/src/QC_MLPipeline.h` | Modify — pipeline serialize (delegates) |

### Independent Verification

1. **Round-trip tests (native format)**: For each algorithm:
   - Train model → `ml_save_model("test.qml")` → `ml_load_model("test.qml")` →
     predict on same input → assert identical results to original model
   - Verify all hyperparameters preserved (k, threshold, etc.)
   - Verify field_names preserved (hash-based API still works after load)

2. **Round-trip tests (JSON format)**: Same as native, but with `.json` extension.
   Additionally verify the JSON file is valid JSON and contains expected keys.

3. **Binary integrity**: Corrupt one byte in a `.qml` file → `ml_load_model()` must
   raise `ML-DESERIALIZE-ERROR` (CRC check fails).

4. **Cross-version compatibility**: Save with format version 1, bump the reader to
   expect version 1, verify it still loads. (Foundation for future version upgrades.)

5. **ONNX export verification**: For each exportable algorithm:
   - Export to `.onnx` → load with `OnnxModel` → run inference → compare results
     against the native algorithm's predictions (within floating-point tolerance
     of 1e-6)
   - Verify the ONNX model's metadata (producer_name, description, inputs, outputs)

6. **ONNX cross-platform verification** (if Python available on CI):
   - Export `.onnx` from Qore → load in Python `onnxruntime` → run same test input →
     compare predictions. This is the gold standard for interoperability.

7. **Pipeline export**: Export a pipeline (StandardScaler + LinearRegression) to ONNX →
   run inference → verify output matches the pipeline's `predict()`.

8. **Large model stress test**: Train IsolationForest with 1000 trees on 10K points →
   save → load → verify file size is reasonable and load time < 2 seconds.

9. **Thread safety**: Concurrent `ml_save_model()` and `predict()` on the same model
   (save acquires read lock, predict acquires read lock — both should succeed).

10. **Valgrind**: Full memory check on save/load cycle.

---

## Phase 6: Model Registry and Deployment Management

### Motivation

Production ML requires tracking which models are in use, comparing versions, and
managing the lifecycle from training to deployment. The QoreModelRegistry module provides
this without external dependencies (filesystem or database-backed).

**Multi-tenancy** is a first-class design requirement. The registry is designed from
the start to support multi-tenant deployments where a single shared registry instance
serves multiple independent tenants with full data isolation. This is critical for
SaaS/platform scenarios (e.g., Qorus) where multiple customers or business units
share infrastructure but must not see each other's models.

### Components

#### 6.1 QoreModelRegistry Module (Qore)

A new Qore user module (`qlib/QoreModelRegistry/`) that builds on Phase 5's persistence.

```
qlib/QoreModelRegistry/
├── QoreModelRegistry.qm                 # Main module
├── ModelRegistry.qc                     # Core registry class (multi-tenant aware)
├── ModelVersion.qc                      # Version tracking
├── ModelComparison.qc                   # Side-by-side model comparison
├── ModelRegistryDataProvider.qc         # DataProvider integration
└── ModelRegistryDataProviderFactory.qc  # Factory
```

**Core API** (single-tenant):

```qore
# Registry backed by filesystem (single-tenant — no tenant_id)
auto reg = new ModelRegistry({"path": "/var/models"});

# Register a trained model
string version_id = reg.register(model, {
    "name": "fraud-detector",
    "tags": ("production", "v2"),
    "description": "Isolation forest for transaction anomaly detection",
    "metrics": {"silhouette_score": 0.82, "f1": 0.91},
    "training_data_hash": "sha256:abc...",
});

# List versions
list<hash<ModelVersionInfo>> versions = reg.listVersions("fraud-detector");

# Load a specific version
auto model = reg.load("fraud-detector", version_id);
# Or load the latest
auto model = reg.load("fraud-detector", "latest");

# Compare two versions
hash<ModelComparisonResult> cmp = reg.compare("fraud-detector", v1_id, v2_id, test_data);

# Promote a version (tag as "production")
reg.tag("fraud-detector", version_id, "production");

# Delete old versions (keeps tagged versions)
reg.prune("fraud-detector", {"keep_tagged": True, "keep_latest": 5});
```

**Multi-tenant API**:

```qore
# Tenant-scoped registries — each sees only its own models
auto reg_a = new ModelRegistry({"path": "/var/models", "tenant_id": "acme-corp"});
auto reg_b = new ModelRegistry({"path": "/var/models", "tenant_id": "globex-inc"});

# Both tenants can register models with the same name — no conflict
reg_a.register(model_a, {"name": "fraud-detector"});
reg_b.register(model_b, {"name": "fraud-detector"});

# Each tenant only sees its own models
reg_a.listModels();  # ("fraud-detector",) — only acme-corp's
reg_b.listModels();  # ("fraud-detector",) — only globex-inc's

# Metadata includes tenant_id
hash<auto> meta = reg_a.getVersionMetadata("fraud-detector", vid);
# meta.tenant_id == "acme-corp"

# Admin registry (no tenant_id) can list all tenants
auto admin = new ModelRegistry({"path": "/var/models"});
list<string> tenants = admin.listTenants();  # ("acme-corp", "globex-inc")

# getTenantId() returns the tenant context
reg_a.getTenantId();   # "acme-corp"
admin.getTenantId();   # NOTHING
```

**Multi-tenancy design principles**:
- **Tenant isolation**: All storage and API operations are scoped by `tenant_id` when
  present. A tenant cannot see, load, modify, or delete another tenant's models.
- **Path-safe tenant IDs**: Tenant IDs are validated on construction — path separators
  (`/`, `\`) and traversal patterns (`..`) are rejected to prevent directory escape.
- **Metadata carries tenant context**: `tenant_id` is stored in version metadata,
  making it possible to audit which tenant owns each model version.
- **Admin operations**: A registry created without `tenant_id` operates at the global
  level and can enumerate tenants via `listTenants()`.
- **Backward compatible**: Single-tenant mode (no `tenant_id`) works exactly as before
  — the tenant layer is fully optional.

**Hashdecls**:

```
hashdecl ModelVersionInfo {
    string version_id;          # UUID
    string name;                # model name
    string algorithm;           # algorithm type
    date created;               # registration timestamp
    *string tenant_id;          # tenant identifier (multi-tenant deployments)
    *string description;
    *hash<auto> metrics;        # stored evaluation metrics
    *hash<auto> hyperparameters;
    list<string> tags;
    int file_size;              # bytes
    string format;              # "qml", "onnx", "json"
    *string training_data_hash;
}

hashdecl ModelComparisonResult {
    hash<ModelVersionInfo> version_a;
    hash<ModelVersionInfo> version_b;
    *hash<auto> metric_deltas;   # metric_name → (a_value, b_value, delta)
    *hash<auto> prediction_comparison;  # summary of prediction differences
}
```

**Storage layout** (filesystem backend):

Single-tenant:
```
/var/models/
├── fraud-detector/
│   ├── registry.json               # version index
│   ├── v_20260320_abc123.qml       # model file
│   ├── v_20260320_abc123.meta.json # metadata
│   ├── v_20260321_def456.qml
│   └── v_20260321_def456.meta.json
└── sales-forecast/
    └── ...
```

Multi-tenant:
```
/var/models/
├── acme-corp/                          # tenant directory
│   ├── fraud-detector/
│   │   ├── registry.json
│   │   ├── v_20260320_abc123.qml
│   │   └── v_20260320_abc123.meta.json
│   └── churn-predictor/
│       └── ...
├── globex-inc/                         # another tenant
│   ├── fraud-detector/                 # same model name, different tenant
│   │   ├── registry.json
│   │   └── ...
│   └── ...
```

#### 6.2 Database Backend (optional)

For production deployments with multiple instances, the registry can be backed by a
database using Qore's SqlUtil/DatasourcePool:

```qore
auto reg = new ModelRegistry({
    "datasource": ds,
    "table_prefix": "ml_",
    "tenant_id": "acme-corp",  # optional — scopes all queries
});
```

Tables (all include `tenant_id` from day one):
- `ml_models` (`tenant_id`, name, algorithm) — unique constraint on (`tenant_id`, `name`)
- `ml_versions` (`tenant_id`, version_id, model_id, created, metadata JSON)
- `ml_tags` (`tenant_id`, version_id, tag)
- `ml_blobs` (`tenant_id`, version_id, data BLOB)

All queries include `WHERE tenant_id = :tenant_id` to enforce isolation. The database
backend uses row-level tenant scoping rather than separate schemas, keeping deployment
simple while ensuring strict isolation.

**Resource quotas** (future): Per-tenant limits on model count, total storage, and
version count can be enforced at the registry level without schema changes — the
`ml_models` and `ml_blobs` tables already carry the `tenant_id` needed for aggregation.

This is an optional add-on and depends on existing SqlUtil infrastructure.

#### 6.3 DataProvider Integration

- `model-registry` data provider: browse registered models, list versions
- Supports DataProvider search/navigation for Qorus integration
- Actions: register, load, compare, prune — accessible via DataProviderActionCatalog
- **Tenant scoping**: DataProvider paths are tenant-scoped when `tenant_id` is set,
  e.g., `model-registry/acme-corp/fraud-detector/versions`

### Files to Create

| File | Action |
|------|--------|
| `qlib/QoreModelRegistry/QoreModelRegistry.qm` | New — main module |
| `qlib/QoreModelRegistry/ModelRegistry.qc` | New — core registry class (multi-tenant) |
| `qlib/QoreModelRegistry/ModelVersion.qc` | New — version tracking |
| `qlib/QoreModelRegistry/ModelComparison.qc` | New — comparison utilities |
| `qlib/QoreModelRegistry/ModelRegistryDataProvider.qc` | New |
| `qlib/QoreModelRegistry/ModelRegistryDataProviderFactory.qc` | New |
| `CMakeLists.txt` | Modify — add module |
| `Makefile.am` | Modify — add module |
| `examples/test/qlib/QoreModelRegistry/QoreModelRegistry.qtest` | New |

### Independent Verification

1. **Registration round-trip**: Register a model → `listVersions()` → verify it
   appears with correct metadata → `load()` → predict → same results as original.

2. **Multi-version management**: Register 5 versions of the same model → verify
   chronological ordering → load "latest" → verify it's the most recent.

3. **Tagging**: Tag version as "production" → load by tag → verify correct version.

4. **Pruning**: Register 10 versions, tag 2 → `prune(keep_tagged=True, keep_latest=3)`
   → verify 5 versions remain (2 tagged + 3 latest).

5. **Comparison**: Register two KMeans models with different k → `compare()` with
   test data → verify metric_deltas computed correctly.

6. **Concurrent access**: Multiple processes register models simultaneously (filesystem
   locking via advisory locks or atomic rename).

7. **Storage integrity**: Verify `.meta.json` files are valid JSON with expected schema.
   Verify model files match stored file_size.

8. **DataProvider navigation**: Browse registry via DataProvider API, verify model
   listing matches direct registry listing.

9. **Error handling**: Load non-existent model → `ML-REGISTRY-ERROR`. Load corrupted
   file → `ML-DESERIALIZE-ERROR`. Register unfitted model → `ML-REGISTRY-ERROR`.

10. **Multi-tenant isolation**: Two tenant registries sharing the same base path can
    register models with the same name without conflict. Each tenant's `listModels()`,
    `listVersions()`, `load()`, `tag()`, and `prune()` operations are scoped to that
    tenant only. One tenant cannot see, load, or modify another tenant's models.

11. **Tenant enumeration**: `listTenants()` on an admin registry returns all tenants
    that have registered models.

12. **Tenant metadata**: Version metadata includes `tenant_id` when registered via a
    tenant-scoped registry. Non-tenant registries do not include `tenant_id`.

13. **Invalid tenant IDs**: Constructor rejects tenant IDs containing path separators
    or traversal patterns (`/`, `\`, `..`) with `ML-REGISTRY-ERROR`.

14. **Cross-tenant tag isolation**: Tagging a model in tenant A does not affect
    the same-named model in tenant B.

---

## Implementation Schedule and Dependencies

```
Phase 1 ──────────────> Phase 2 ──────────────> Phase 3
(Preprocessing)         (Metrics)               (New Algorithms)
                                                       │
                            Phase 4 <──────────────────┘
                            (Online Learning)
                                  │
                            Phase 5 <── depends on all trained models existing
                            (Persistence & Export)
                                  │
                            Phase 6
                            (Model Registry)
```

- **Phase 1** has no dependencies on other phases
- **Phase 2** can proceed in parallel with Phase 1 (metrics don't depend on preprocessing)
- **Phase 3** benefits from Phase 2 (can use metrics in tests) but is not blocked by it
- **Phase 4** depends on Phase 3 (extends LogisticRegression)
- **Phase 5** benefits from all prior phases (more algorithms to serialize) but can start
  with existing algorithms first
- **Phase 6** depends on Phase 5 (uses persistence layer)

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| Protobuf dependency for ONNX export | Make fully optional (ifdef); JSON and native formats work without it |
| ONNX operator coverage | Not all algorithms map to ONNX; clearly document which export and which don't |
| Large model serialization performance | Use binary format with Eigen's raw memory layout; avoid JSON for large models |
| Backward-compatible format evolution | Version field in header; reader checks version and dispatches accordingly |
| Thread safety in registry | Filesystem: atomic rename + advisory locks; Database: SQL transactions |

## Phase 7: Tokenizer Module Future Enhancements

The `tokenizer` binary module (C++/utf8proc) provides HuggingFace-compatible text
tokenization with BPE, WordPiece, and Unigram models. It loads `tokenizer.json` files
and produces token IDs matching the Python `tokenizers` library exactly.

### Current Capabilities (v1.0)
- **Models**: BPE (GPT-2, Llama), WordPiece (BERT), Unigram/Viterbi (T5)
- **Normalizers**: BertNormalizer, Unicode NFC/NFD/NFKC/NFKD, Precompiled (SentencePiece
  Darts DoubleArray trie), Sequence, Replace, Prepend, Lowercase, StripAccents
- **Pre-tokenizers**: BertPreTokenizer, ByteLevel (GPT-2), Metaspace (SentencePiece),
  Whitespace, Sequence
- **Post-processors**: TemplateProcessing, BertProcessing, RobertaProcessing, ByteLevel
- **Decoders**: WordPiece, ByteLevel, Metaspace, ByteFallback, Fuse, Strip, Sequence
- **Features**: truncation (3 strategies), padding, batch encoding, offset mapping,
  special tokens mask, added token matching, sentence pair encoding, getVocab()
- **Performance**: O(n log n) BPE merge via priority queue + doubly linked list
- **Verified**: BERT, GPT-2, T5, Llama/TinyLlama — exact match vs Python `tokenizers`

### Future Enhancements

#### 7.1 `word_ids` Mapping
Maps each output token to its pre-token word index. Required for NER span alignment
and token-to-word grouping. Implementation: track the pre-token index through the
pipeline alongside byte offsets.

#### 7.2 Overflowing Tokens (Sliding Window)
When truncation discards tokens, return the overflow as a separate `EncodingResult`.
Enables sliding-window processing of long documents (e.g., 512-token chunks with
overlap). Add `stride` parameter to `encodeAdvanced()`.

#### 7.3 Pre-tokenized Input (`is_pretokenized`)
Accept already-split word lists instead of raw strings. Useful when the caller has
domain-specific tokenization (e.g., code tokenizers, chemical formulas). Skip the
normalize and pre-tokenize stages; apply model tokenization to each word directly.

#### 7.4 Dynamic Vocabulary Extension (`add_tokens()`)
Allow adding tokens to the vocabulary after construction. Required for chat templates
with custom special tokens. Implementation: mutable vocab with thread-safe locking
(reader-writer lock since reads vastly outnumber writes).

#### 7.5 Decoder Output Cleanup
Improve BERT WordPiece decoder spacing around punctuation when `skip_special_tokens`
is enabled. Current decoder includes [CLS]/[SEP] literal strings in edge cases.

#### 7.6 Full Unicode `\p{N}` Digit Support in GPT-2 Pattern
The ByteLevel pre-tokenizer's `isDigit()` now uses utf8proc `UTF8PROC_CATEGORY_ND`,
but the GPT-2 regex `\p{N}` also includes `UTF8PROC_CATEGORY_NL` (letter number)
and `UTF8PROC_CATEGORY_NO` (other number). Expand to match the full `\p{N}` class.

#### 7.7 ONNX Inference Pipeline Integration
Create a `QorusModelProcessor` that chains: load tokenizer.json → tokenize input →
run ONNX model → decode output. This provides end-to-end text-in/result-out inference
for NLP models in Qorus data pipelines.

---

## Conventions

All new code follows the existing patterns documented in `design/ml-architecture.md`:
- C++ classes extend `AbstractPrivateData` with `mutable std::mutex mtx`
- QPP bindings follow `qclass Name [arg=QoreType* var; ns=Qore::ML; flags=final]`
- Hashdecls in `ql_ml.qpp`, registered in `ml-module.cpp`
- DataProvider processors in `qlib/DataProviderML/`, registered in `DataProviderML.qm`
- Tests: binary module tests in `modules/ml/test/ml.qtest`, DataProvider tests in
  `examples/test/qlib/DataProviderML/DataProviderMLProcessors.qtest`
- Copyright 2026, MIT license
- All hashdecls and functions marked with `@since ml 1.2` (or appropriate version)
