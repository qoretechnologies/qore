# ML Enhancement Plan

## Overview

This document describes the phased plan to build Qore into an enterprise data science
platform. Phases 1–7 are **complete** (ML foundation: 17 algorithms, preprocessing,
metrics, serialization, model registry, HuggingFace tokenization). Phase 8 is
**complete** (DataFrame module with SQL-like operations, CSV/Parquet/DB I/O, ML
integration, DataProvider wrapper). Phase 9 is **complete** (DecisionTree, RandomForest, GradientBoostedTrees with
6 DataProvider processors, GBT multiclass softmax, typed predict API).
Phase 10 is **complete** (SVM, NaiveBayes).
Phase 11 is **complete** (statistical functions with Cephes incbet).
Phase 12 is **core complete** (OneHotEncoder, LabelEncoder, VarianceThreshold,
PolynomialFeatures, SelectKBest, RFE — remaining: DateTimeFeatures, TextFeatures).
Phase 13 is **core complete** (13.1 concept drift detection: ADWIN, Page-Hinkley, DDM;
13.2 streaming feature computation: RollingStats, EWMA; 13.3 data validation:
DataProfile, SchemaValidation — remaining: experiment tracking).
Phase 14 is planned.

## Current State (after Phases 1–9)

- **ml module**: 22 native algorithms (Phase 9: DecisionTree, RandomForest,
  GradientBoostedTrees with shared FlatTree CART engine; Phase 10: SVM, NaiveBayes) (IsolationForest, LOF, DBSCAN, KMeans, GMM,
  LinearRegression, LogisticRegression, KNN, HoltWinters, SeasonalDecomposition, PCA,
  StandardScaler, MinMaxScaler, Imputer, MLPipeline, CrossValidator) + OnnxModel +
  classification/regression/clustering metrics + native serialization + online learning
- **DataProviderML module**: 20 pipeline processors (algorithms + preprocessing +
  metrics + registry model)
- **QoreModelRegistry module**: Multi-tenant model versioning with filesystem, database,
  and REST backends
- **tokenizer module**: HuggingFace-compatible BPE/WordPiece/Unigram tokenization with
  word_ids, pre-tokenized input, sliding window, dynamic vocabulary, Unicode support
- **Architecture**: C++ with Eigen3 (no Python), thread-safe, typed hashdecl results,
  dual API (hash-based + matrix-based)

## Architecture (Current — Phases 1–7 Complete)

```
┌──────────────────────────────────────────────────────────┐
│                Qorus / Applications                      │
├──────────────────────────────────────────────────────────┤
│               DataProviderML (Qore)                      │
│  20 processors: algorithms + preprocessing + metrics +   │
│  registry model                                          │
├──────────────────────────────────────────────────────────┤
│             QoreModelRegistry (Qore)                     │
│  Model versioning, comparison, multi-tenant, 3 backends  │
├──────────────────────────────────────────────────────────┤
│        ml (C++ binary)          tokenizer (C++ binary)   │
│  17 algorithms + metrics +      BPE, WordPiece, Unigram  │
│  serialization + pipeline +     word_ids, sliding window  │
│  online learning + CrossVal     addTokens, pre-tokenized │
├──────────────────────────────────────────────────────────┤
│    Eigen3       ONNX Runtime (opt)       utf8proc        │
└──────────────────────────────────────────────────────────┘
```

## Architecture Target (Phases 8–14)

```
┌──────────────────────────────────────────────────────────┐
│                Qorus / Applications                      │
├──────────────────────────────────────────────────────────┤
│               DataProviderML (Qore)                      │
│  All current + DataFrame + feature eng + validation +    │
│  experiment tracking + streaming processors              │
├──────────────────────────────────────────────────────────┤
│  QoreModelRegistry    QoreDataFrame    QoreStats         │
│  (current)            columnar data    distributions,    │
│                       SQL-like ops     hypothesis tests  │
├──────────────────────────────────────────────────────────┤
│        ml (C++ binary)          tokenizer (C++ binary)   │
│  Current + DecisionTree +       Current + embeddings     │
│  RandomForest + GBT + SVM +     pipeline integration     │
│  NaiveBayes + feature eng +                              │
│  drift detection + statistics                            │
├──────────────────────────────────────────────────────────┤
│  Eigen3    ONNX Runtime    utf8proc    Apache Arrow(opt) │
└──────────────────────────────────────────────────────────┘
```

---

## Phase 1: Data Preprocessing Pipeline ✅ COMPLETE

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

## Phase 2: Model Evaluation Metrics ✅ COMPLETE

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

## Phase 3: New Algorithms — Logistic Regression and k-NN ✅ COMPLETE

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

## Phase 4: Online Learning Extensions ✅ COMPLETE

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

## Phase 5: Model Persistence and Export ✅ COMPLETE

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

## Phase 6: Model Registry and Deployment Management ✅ COMPLETE

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

## Implementation Schedule — Phases 1–7 (Complete)

```
Phase 1 ──────> Phase 2 ──────> Phase 3 ──────> Phase 4 ──────> Phase 5 ──────> Phase 6
(Preproc) ✅    (Metrics) ✅    (Classif) ✅    (Online) ✅     (Persist) ✅    (Registry) ✅

Phase 7 (Tokenizer enhancements) ✅ — independent of Phases 1–6
```

---

## Phase 8: DataFrame / Columnar Data Abstraction ✅ COMPLETE

### Motivation

This is the single biggest gap between Qore and Python for data science. Python's Pandas
is the gravity well that keeps data scientists in the ecosystem. Every ML workflow starts
with data manipulation — without a native columnar data structure, users must work with
lists of hashes, which is verbose and slow for numerical workloads.

### Components

#### 8.1 QoreDataFrame (C++ binary module)

Column-oriented data structure backed by Eigen for numeric columns and `std::vector<std::string>`
for string columns. Designed for tabular data with heterogeneous column types.

```cpp
class QoreDataFrame : public AbstractPrivateData {
public:
    // Construction
    QoreDataFrame(ExceptionSink* xsink);  // empty
    static QoreDataFrame* fromHashList(const QoreListNode* records, ExceptionSink* xsink);
    static QoreDataFrame* fromColumns(const QoreHashNode* columns, ExceptionSink* xsink);

    // Column access
    int numRows() const;
    int numCols() const;
    QoreListNode* columnNames(ExceptionSink* xsink) const;
    QoreValue getColumn(const std::string& name, ExceptionSink* xsink) const;
    QoreHashNode* getRow(int index, ExceptionSink* xsink) const;

    // SQL-like operations
    QoreDataFrame* select(const QoreListNode* columns, ExceptionSink* xsink) const;
    QoreDataFrame* filter(const std::string& column, const std::string& op,
        QoreValue value, ExceptionSink* xsink) const;
    QoreDataFrame* groupBy(const QoreListNode* columns, const QoreHashNode* aggs,
        ExceptionSink* xsink) const;
    QoreDataFrame* join(const QoreDataFrame* other, const std::string& on,
        const std::string& how, ExceptionSink* xsink) const;
    QoreDataFrame* sortBy(const QoreListNode* columns, bool ascending,
        ExceptionSink* xsink) const;
    QoreDataFrame* head(int n, ExceptionSink* xsink) const;
    QoreDataFrame* tail(int n, ExceptionSink* xsink) const;

    // Aggregation
    QoreHashNode* describe(ExceptionSink* xsink) const;  // count, mean, std, min, max, quartiles
    QoreHashNode* corrMatrix(ExceptionSink* xsink) const;

    // Mutation
    void addColumn(const std::string& name, QoreValue data, ExceptionSink* xsink);
    void dropColumn(const std::string& name, ExceptionSink* xsink);
    void renameColumn(const std::string& old_name, const std::string& new_name,
        ExceptionSink* xsink);
    QoreDataFrame* fillna(QoreValue value, ExceptionSink* xsink) const;
    QoreDataFrame* dropna(ExceptionSink* xsink) const;

    // Conversion
    QoreListNode* toHashList(ExceptionSink* xsink) const;  // list<hash>
    QoreHashNode* toColumnHash(ExceptionSink* xsink) const;
    MatrixXd toMatrix(const QoreListNode* columns, ExceptionSink* xsink) const;

    // I/O
    static QoreDataFrame* readCSV(const std::string& path, const QoreHashNode* options,
        ExceptionSink* xsink);
    void writeCSV(const std::string& path, const QoreHashNode* options,
        ExceptionSink* xsink) const;

private:
    struct Column {
        std::string name;
        enum Type { FLOAT, INT, STRING, BOOL, DATE } type;
        VectorXd float_data;           // for FLOAT columns
        std::vector<int64_t> int_data; // for INT columns
        std::vector<std::string> str_data;
        std::vector<bool> null_mask;   // true = missing value
    };
    std::vector<Column> columns;
    int n_rows = 0;
    mutable std::mutex mtx;
};
```

**Key design decisions:**
- Column-oriented (not row-oriented) for efficient numerical operations
- Null mask per column for missing value tracking (like Pandas nullable types)
- Eigen-backed numeric columns for zero-copy integration with ml module
- `toMatrix()` enables direct feeding into ml algorithms
- CSV I/O with type inference

#### 8.2 ML Module Integration

Extend all ml algorithms to accept DataFrame input:
```qore
DataFrame df = DataFrame::readCSV("training_data.csv");
auto model = new ML::LinearRegression();
model.fitDataFrame(df, "target_column");
DataFrame predictions = model.predictDataFrame(df.select(feature_columns));
```

#### 8.3 DataProvider Integration

- DataFrame as a DataProvider record source/sink
- Database query results → DataFrame (via SqlUtil)
- REST API responses → DataFrame
- DataProvider processor that accumulates records into a DataFrame

### Files to Create

| File | Action |
|------|--------|
| `modules/dataframe/` | New module directory |
| `modules/dataframe/src/QC_DataFrame.h` | Class declaration |
| `modules/dataframe/src/DataFrame.cpp` | Implementation |
| `modules/dataframe/src/QC_DataFrame.qpp` | QPP bindings |
| `modules/dataframe/src/df_csv.h/cpp` | CSV reader/writer |
| `modules/dataframe/src/ql_dataframe.qpp` | Hashdecls |
| `modules/dataframe/src/dataframe-module.cpp` | Module init |
| `modules/dataframe/CMakeLists.txt` | Build config |
| `modules/dataframe/test/dataframe.qtest` | Tests |

### Independent Verification

1. Construction from list<hash>, column hash, CSV file
2. SQL-like operations: select, filter, groupby, join, sort produce correct results
3. `describe()` matches hand-computed statistics
4. `toMatrix()` → ml fit → predictDataFrame round-trip
5. Missing value handling: fillna, dropna, null propagation in aggregations
6. Large dataset test: 100K rows, verify no memory leaks, reasonable performance
7. Thread safety: concurrent read operations after construction

---

## Phase 9: Tree-Based Algorithms ✅ CORE COMPLETE

### Status

Core algorithms implemented: DecisionTree, RandomForest, GradientBoostedTrees with
shared FlatTree CART engine, full serialization, feature importance, and tests.
20 new test cases, 75 new assertions.

**Remaining items:**
- GBT multiclass: currently uses simplified nearest-class-to-raw-prediction; proper
  softmax with K sets of trees per round needed for calibrated multiclass probabilities
- DataProvider processors: decision-tree-classification, decision-tree-regression,
  random-forest-classification, random-forest-regression, gbt-classification,
  gbt-regression (6 processor files + MLProcessorsDataProvider + DataProviderML.qm
  registration)
- `findBestSplit()` performance: creates new index vectors per candidate threshold;
  pre-sorted index optimization would reduce allocation pressure for large datasets
- DecisionTree `fit()` has no cancellation check inside the recursive tree build;
  acceptable since bounded by max_depth but could be improved for very deep trees

### Motivation

Decision trees, random forests, and gradient boosting are the workhorses of enterprise ML
on tabular data. They dominate Kaggle competitions, fraud detection, churn prediction,
and credit scoring. Without these, Qore cannot compete for the most common enterprise
use cases.

### Components

#### 9.1 DecisionTree (C++)

Classification and regression tree (CART) with Gini/entropy split criteria.

```cpp
class QoreDecisionTree : public AbstractPrivateData {
public:
    QoreDecisionTree(const std::string& task, int max_depth, int min_samples_split,
        int min_samples_leaf, const std::string& criterion);

    void fit(const MatrixXd& X, const VectorXd& y, ExceptionSink* xsink);
    QoreHashNode* predict(const RowVectorXd& point, ExceptionSink* xsink) const;
    QoreHashNode* featureImportances(ExceptionSink* xsink) const;

private:
    struct Node {
        int feature_index = -1;
        double threshold = 0.0;
        double value = 0.0;           // leaf value (class or mean)
        std::vector<double> class_probs;  // class probabilities at leaf
        std::unique_ptr<Node> left, right;
        int n_samples = 0;
    };
    std::unique_ptr<Node> root;
};
```

**Hashdecls**: `DecisionTreeResult`, `FeatureImportanceInfo`

#### 9.2 RandomForest (C++)

Bagging ensemble of decision trees. Qore's native threading is an advantage — trees can
be trained in parallel using Qore background threads or C++ `std::async`.

```cpp
class QoreRandomForest : public AbstractPrivateData {
    // n_trees, max_depth, max_features, bootstrap, n_jobs
    // fit() trains trees in parallel
    // predict() aggregates predictions (majority vote / mean)
    // featureImportances() averages across trees
    // Out-of-bag (OOB) error estimation
};
```

#### 9.3 GradientBoostedTrees (C++)

Sequential ensemble with gradient descent on a loss function. The enterprise ML algorithm
— equivalent to XGBoost/LightGBM for tabular data.

```cpp
class QoreGBT : public AbstractPrivateData {
    // n_estimators, learning_rate, max_depth, min_samples_split
    // subsample, colsample_bytree
    // loss: "mse" (regression), "log_loss" (classification)
    // fit() builds trees sequentially, each fitting residuals
    // predict() sums tree predictions with learning rate
    // Early stopping with validation set
    // featureImportances() via split gain or permutation
};
```

#### 9.4 DataProvider Processors

- `decision-tree` processor: classification/regression
- `random-forest` processor: ensemble classification/regression
- `gradient-boosted-trees` processor: ensemble with early stopping

### Independent Verification

1. DecisionTree on linearly separable data → perfect split
2. DecisionTree on XOR data → requires depth ≥ 2
3. RandomForest accuracy ≥ single DecisionTree on noisy data
4. GBT on Iris-equivalent → >95% accuracy
5. Feature importances sum to 1.0
6. Serialization round-trip for all three
7. OOB error close to cross-validation error
8. Early stopping triggers when validation loss stops improving

---

## Phase 10: Additional Classifiers — SVM and Naive Bayes ✅ COMPLETE

### Motivation

SVM is widely used in enterprise for text classification and anomaly detection with
small-to-medium datasets. Naive Bayes is the fastest classifier for text — a good
baseline that's commonly required in enterprise pipelines.

### Components

#### 10.1 SVM (C++)

Support Vector Machine with linear and RBF kernels. Uses SMO (Sequential Minimal
Optimization) algorithm — no external dependency needed.

```cpp
class QoreSVM : public AbstractPrivateData {
    // kernel: "linear", "rbf", "poly"
    // C (regularization), gamma (RBF), degree (poly)
    // fit() uses SMO algorithm
    // predict() returns class + decision function value
    // Multi-class via one-vs-one decomposition
};
```

#### 10.2 NaiveBayes (C++)

Gaussian, Multinomial, and Bernoulli variants.

```cpp
class QoreNaiveBayes : public AbstractPrivateData {
    // variant: "gaussian", "multinomial", "bernoulli"
    // fit() computes class priors and per-feature statistics
    // predict() returns class + log probabilities
    // Laplace smoothing for multinomial/bernoulli
    // Online update() support (sufficient statistics are additive)
};
```

---

## Phase 11: Statistical Functions ✅ COMPLETE

### Motivation

The ml module has ML algorithms but lacks foundational statistics. Data scientists need
descriptive statistics, hypothesis testing, and distribution functions for exploratory
data analysis and feature validation.

### Components

#### 11.1 Descriptive Statistics (C++ namespace functions)

```cpp
// All operate on VectorXd or DataFrame columns
double ml_percentile(const VectorXd& data, double q);
double ml_iqr(const VectorXd& data);
double ml_skewness(const VectorXd& data);
double ml_kurtosis(const VectorXd& data);
MatrixXd ml_correlation_matrix(const MatrixXd& data);
MatrixXd ml_covariance_matrix(const MatrixXd& data);
```

#### 11.2 Hypothesis Testing

```cpp
QoreHashNode* ml_t_test(const VectorXd& a, const VectorXd& b, ...);     // t-statistic, p-value
QoreHashNode* ml_chi_squared_test(const MatrixXd& contingency, ...);
QoreHashNode* ml_anova(const std::vector<VectorXd>& groups, ...);
QoreHashNode* ml_ks_test(const VectorXd& data, const std::string& distribution, ...);
```

**Hashdecls**: `HypothesisTestResult` — `statistic`, `p_value`, `reject_null`, `df`

#### 11.3 Probability Distributions

```cpp
class QoreDistribution : public AbstractPrivateData {
    // Factory: Normal, Binomial, Poisson, Uniform, Exponential, Gamma, Beta
    double pdf(double x) const;
    double cdf(double x) const;
    double ppf(double p) const;      // inverse CDF (percent point function)
    VectorXd sample(int n) const;    // random sampling
};
```

---

## Phase 12: Feature Engineering Pipeline

### Motivation

Raw data rarely feeds directly into ML models. Feature engineering — encoding
categoricals, extracting date components, computing text features — is where enterprise
ML practitioners spend most of their time. Standardized, serializable feature
transformers that compose into pipelines eliminate manual boilerplate and ensure
consistency between training and production.

### Components

#### 12.1 Categorical Encoders (C++)

- **OneHotEncoder**: Sparse binary columns for each category
- **LabelEncoder**: Integer mapping for ordinal categories
- **OrdinalEncoder**: Ordered integer mapping with explicit ordering
- **TargetEncoder**: Replace category with mean target value (with smoothing)

#### 12.2 Feature Extractors

- **DateTimeFeatures**: Extract day-of-week, hour, month, quarter, is_weekend,
  is_holiday from date columns
- **TextFeatures**: TF-IDF and bag-of-words via tokenizer module integration
- **PolynomialFeatures**: Generate interaction and polynomial terms

#### 12.3 Feature Selection

- **SelectKBest**: Select top-k features by mutual information or chi-squared score
- **RecursiveFeatureElimination**: Iteratively remove least important features
- **VarianceThreshold**: Remove low-variance features

All transformers implement the `fit()` / `transform()` / `fitTransform()` pattern and
are serializable via Phase 5's persistence layer. They compose into MLPipeline chains.

---

## Phase 13: Streaming Analytics and Data Governance

### Motivation

Enterprise data science operates on live data streams and requires governance guardrails.
Concept drift detection tells you when a deployed model's assumptions are violated.
Data validation prevents garbage-in-garbage-out. These are where Qore + Qorus has a
structural advantage over Python.

### Components

#### 13.1 Concept Drift Detection (C++)

- **ADWIN** (Adaptive Windowing): detects distribution changes in streaming data
- **Page-Hinkley**: detects mean shifts in sequential observations
- **DDM** (Drift Detection Method): monitors error rate for classification models

```cpp
class QoreADWIN : public AbstractPrivateData {
    // add(double value) — add observation
    // bool driftDetected() — check if drift occurred
    // double getMean() — current window mean
    // int getWidth() — current window size
};
```

#### 13.2 Streaming Feature Computation

DataProvider processors for real-time feature engineering:
- Rolling window statistics (mean, std, min, max over last N records)
- Exponential moving average / exponential weighted statistics
- Rate computation (events per time window)

#### 13.3 Data Validation (Qore module)

- Schema validation: expected types, ranges, nullability per column
- Distribution drift: compare incoming data distribution against training baseline
- Automated data profiling: cardinality, missing rates, outlier detection per column
- Integration with DataProvider processors for pipeline guardrails

#### 13.4 Experiment Tracking (extend QoreModelRegistry)

- Track hyperparameters, metrics, datasets, and code versions per experiment
- Compare experiments side-by-side
- Reproducibility metadata (random seeds, data hashes, environment info)

---

## Phase 14: Ecosystem Interoperability and LLM Integration

### Motivation

Enterprise environments are polyglot. Qore must exchange data with Python/Spark/R
ecosystems and participate in the enterprise AI/LLM wave. Apache Arrow is the lingua
franca for columnar data; ONNX covers model exchange. Embedding computation and vector
search enable RAG and semantic search pipelines.

### Components

#### 14.1 Apache Arrow / Parquet Support (C++ module, optional)

- Read/write Parquet files (the standard format for data lake storage)
- Zero-copy Arrow ↔ DataFrame conversion
- Arrow IPC for cross-process data sharing
- Optional dependency: `libarrow` / `libparquet`

#### 14.2 ONNX Export (extend ml module)

Complete Phase 5's ONNX export plan — export native Qore models to ONNX format for
deployment in any ONNX Runtime environment. Requires optional `libprotobuf` dependency.

See Phase 5 for the algorithm-to-ONNX operator mapping table.

#### 14.3 LLM / Embedding Pipeline

Build on the tokenizer module:
- **Embedding computation**: tokenize → run ONNX embedding model → return vectors
  (sentence-transformers style)
- **Vector similarity**: cosine, dot product, euclidean distance on embedding vectors
- **ONNX inference pipeline processor** (deferred 7.7): chains tokenizer → ONNX model
  → output in a DataProvider processor
- **Prompt templating**: structured prompt construction for LLM API calls

---

## Implementation Schedule — Phases 8–14

```
Phase 8 (DataFrame) ──────> Phase 12 (Feature Eng)
        │                          │
        └──> Phase 9 (Trees) ─────>│──> Phase 13 (Streaming/Governance)
        │                          │
        └──> Phase 10 (SVM/NB)     └──> Phase 14 (Interop/LLM)
        │
        └──> Phase 11 (Statistics)
```

- **Phase 8** (DataFrame) is the top priority — it unlocks Phases 9–14
- **Phases 9, 10, 11** can proceed in parallel after Phase 8
- **Phase 12** benefits from Phase 8 (feature transformers operate on DataFrames)
- **Phase 13** benefits from Phases 9–10 (drift detection needs diverse algorithms)
- **Phase 14** is largely independent but benefits from Phase 8 (Arrow ↔ DataFrame)

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| DataFrame performance vs Pandas | Use Eigen for numerics; column-oriented storage; benchmark against Pandas on key operations |
| Apache Arrow dependency size | Make fully optional; DataFrame works without Arrow |
| GBT complexity | Start with basic GBT; optimize later (histogram binning, etc.) |
| SVM SMO convergence | Use well-tested SMO variant (Platt 1998); fall back to linear for non-convergence |
| Parquet format complexity | Use Apache Arrow's Parquet reader rather than implementing from scratch |
| LLM API landscape changes | Focus on ONNX local inference; API integrations via existing REST DataProvider |

## Phase 7: Tokenizer Module Enhancements ✅ COMPLETE (7.1–7.6)

The `tokenizer` binary module (C++/utf8proc) provides HuggingFace-compatible text
tokenization with BPE, WordPiece, and Unigram models. It loads `tokenizer.json` files
and produces token IDs matching the Python `tokenizers` library exactly.

### Capabilities (v1.1)
- **Models**: BPE (GPT-2, Llama), WordPiece (BERT), Unigram/Viterbi (T5)
- **Normalizers**: BertNormalizer, Unicode NFC/NFD/NFKC/NFKD, Precompiled (SentencePiece
  Darts DoubleArray trie), Sequence, Replace, Prepend, Lowercase, StripAccents
- **Pre-tokenizers**: BertPreTokenizer, ByteLevel (GPT-2), Metaspace (SentencePiece),
  Whitespace, Sequence
- **Post-processors**: TemplateProcessing, BertProcessing, RobertaProcessing, ByteLevel
- **Decoders**: WordPiece (Unicode punctuation cleanup), ByteLevel, Metaspace,
  ByteFallback, Fuse, Strip, Sequence
- **Features**: truncation (3 strategies), padding, batch encoding, offset mapping,
  special tokens mask, added token matching, sentence pair encoding, getVocab(),
  **word_ids mapping**, **pre-tokenized input**, **sliding window overflow**,
  **dynamic vocabulary extension** (`addTokens()`), **full Unicode `\p{N}` digit support**
- **Thread safety**: `std::shared_mutex` — concurrent encode with dynamic vocab extension
- **Performance**: O(n log n) BPE merge via priority queue + doubly linked list
- **Verified**: BERT, GPT-2, T5, Llama/TinyLlama — exact match vs Python `tokenizers`
- **Tests**: 22 test cases, 103 assertions

### Implemented Enhancements

#### 7.1 `word_ids` Mapping ✅
Each output token maps to its pre-token word index. NOTHING for special/added/padding
tokens. Flows through InternalEncoding → EncodingResult → processWithOffsets →
TokenizerEncoding hashdecl. Subword tokens from the same word share the same word_id.

#### 7.2 Overflowing Tokens (Sliding Window) ✅
`stride` and `return_overflowing_tokens` options in `encodeAdvanced()`. When truncation
discards tokens and stride > 0, produces overlap chunks. Each chunk includes special
tokens, word_ids, and attention mask. Returned in `overflowing` hashdecl field.

#### 7.3 Pre-tokenized Input (`is_pretokenized`) ✅
`encodePreTokenized(list<string> words)` method and `is_pretokenized` option in
`encodeAdvanced()`. Skips normalization and pre-tokenization. word_ids naturally map
to input word indices. Supports truncation and padding.

#### 7.4 Dynamic Vocabulary Extension (`addTokens()`) ✅
`addTokens(list<auto>)` accepts strings or hashes with content/special/single_word.
Thread-safe via `std::shared_mutex` — `shared_lock` on all read methods, `unique_lock`
on `addTokens()`. Duplicates skipped. `tokenToId()` and `idToToken()` search dynamically
added tokens. `getVocabSize()` includes dynamic additions.

#### 7.5 Decoder Output Cleanup ✅
WordPieceDecoder cleanup uses utf8proc Unicode punctuation detection (all P* categories)
instead of ASCII-only `std::ispunct()`.

#### 7.6 Full Unicode `\p{N}` Digit Support ✅
`ByteLevelPreTokenizer::isDigit()` expanded to include `UTF8PROC_CATEGORY_NL` (letter
numbers, e.g. Roman numerals) and `UTF8PROC_CATEGORY_NO` (other numbers, e.g. fractions,
superscripts) in addition to `UTF8PROC_CATEGORY_ND`.

#### 7.7 ONNX Inference Pipeline Integration — DEFERRED to Phase 14
Create a `QorusModelProcessor` that chains: load tokenizer.json → tokenize input →
run ONNX model → decode output. Deferred because it requires DataProviderML integration
and is better scoped as part of the LLM/embedding pipeline work in Phase 14.

---

## Conventions

All new code follows the existing patterns documented in `design/ml-architecture.md`:
- C++ classes extend `AbstractPrivateData` with `mutable std::mutex mtx`
- QPP bindings follow `qclass Name [arg=QoreType* var; ns=Qore::ML; flags=final]`
- Hashdecls in `ql_ml.qpp`, registered in `ml-module.cpp`
- DataProvider processors in `qlib/DataProviderML/`, registered in `DataProviderML.qm`
- Tests: binary module tests in `modules/ml/test/ml.qtest`, DataProvider tests in
  `examples/test/qlib/DataProviderML/DataProviderMLProcessors.qtest`
- Serialization: all algorithms implement `serialize()` / `deserialize()` and register
  in `ml_serialization.cpp`
- Online learning: algorithms with streaming support implement `update()` / `updateMatrix()`
- Tokenizer: `ns=Qore::Tokenizer`, `std::shared_mutex` for dynamic vocab, `qore_check_cancel()`
  for cooperative cancellation
- Copyright 2026, MIT license
- Version tags: `@since ml 1.2` for Phases 1–6, `@since tokenizer 1.1` for Phase 7
