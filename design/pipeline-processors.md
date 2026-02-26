# Pipeline Processor Architecture — Design Document

## Overview

Pipeline processors are composable data transformation units in the Qore DataProvider framework. Each processor receives records, applies logic (transform, filter, aggregate, analyze, enrich via external API), and emits zero or more output records. Processors chain into pipelines for ETL, streaming analytics, ML inference, and data enrichment workflows.

**Since:** DataProvider 3.3

## 1. Processor Capability System

### 1.1 Capability Constants (DPC_*)

Defined in `AbstractDataProcessor.qc` (lines 33-65). Each processor declares a capability bitfield describing its data flow semantics:

| Constant | Value | Bit | Cardinality | Description |
|----------|-------|-----|-------------|-------------|
| `DPC_TRANSFORM` | 1 | 0 | 1 → 1 | Maps each input record to exactly one output record |
| `DPC_FILTER` | 2 | 1 | 1 → 0..1 | Conditionally passes or drops each record |
| `DPC_EXPAND` | 4 | 2 | 1 → N | Splits one input into multiple outputs |
| `DPC_AGGREGATE` | 8 | 3 | N → M | Accumulates records in `processRecordImpl()`; emits results in `flushRecordsImpl()` |
| `DPC_SINK` | 16 | 4 | N → 0 | Consumes records, writes to external target; may or may not pass records through |
| `DPC_ANALYTICS` | 32 | 5 | N → M (windowed) | Streaming analytics with sliding/tumbling windows; emits typed events |

**Bitfield semantics:** Capabilities are OR'd together — a processor may combine them (e.g., `DPC_TRANSFORM | DPC_FILTER`). The pipeline and UI use capabilities to enforce valid pipeline topologies.

**Name mapping:** `ProcessorCapabilityNameMap` maps each `DPC_*` constant to its human-readable name (e.g., `DPC_TRANSFORM → "TRANSFORM"`).

### 1.2 How Capabilities Affect Behavior

| Capability | `processRecordImpl()` returns | `flushRecordsImpl()` returns | Bulk API |
|-----------|------------------------------|------------------------------|----------|
| TRANSFORM | Transformed record (always) | Nothing | Optional |
| FILTER | Record or NOTHING | Nothing | Usually False |
| EXPAND | List of records | Nothing | Optional |
| AGGREGATE | NOTHING (accumulates) | List of aggregated results | Optional |
| SINK | NOTHING or pass-through | Nothing | Optional |
| ANALYTICS | Event hash(es) per record or NOTHING | Remaining window events | True |

## 2. Dual Processing API

### 2.1 The Two APIs

`AbstractDataProcessor` provides two equivalent processing APIs with automatic bidirectional bridging:

**Closure-based API** (original, supports arbitrary fan-out):
```qore
private submitImpl(code enqueue, auto _data) {
    # Call enqueue(record) for each output record
    # Call nothing to filter/drop
}

private flushImpl(code enqueue) {
    # Call enqueue(record) for each pending result
}
```

**Return-based API** (simpler for 1:1 operations, added in DataProvider 3.3):
```qore
private *auto processRecordImpl(auto record) {
    # Return transformed record, list of records, or NOTHING to filter
}

private *list<auto> flushRecordsImpl() {
    # Return list of pending results, or NOTHING
}
```

### 2.2 Bidirectional Bridge

Subclasses override **one** API; the base class bridges to the other automatically:

```
submitImpl(enqueue, data)  ←bridge→  processRecordImpl(data)
flushImpl(enqueue)         ←bridge→  flushRecordsImpl()
```

**Bridge mechanics:**
- `submitImpl` default: calls `processRecordImpl()`, enqueues result if non-NOTHING; lists are interpreted as multiple records
- `processRecordImpl` default: calls `submitImpl()` with a capturing closure, collects results into return value
- Recursion guard (`in_dispatch` flag) prevents infinite loops; if neither is overridden, throws `PROCESSOR-ERROR`

**Return value interpretation:**
- `NOTHING` → record is filtered out (no downstream output)
- Single value → one output record
- List → multiple output records (each element enqueued separately)

### 2.3 Convenience Methods

| Method | Description |
|--------|-------------|
| `submit(enqueue, data)` | Public entry; sets/clears thread-local data, delegates to `submitImpl()` |
| `processRecord(record)` | Public entry; sets/clears thread-local data, delegates to `processRecordImpl()` |
| `processRecords(list)` | Iterates list, calls `processRecordImpl()` per element, collects results |
| `processRecordsBulk(hash)` | Hash-of-lists columnar processing; defaults to passing through `processRecordImpl()` |
| `submitAndCollect(data)` | Closure-based submit with automatic list collection |
| `flushAndCollect()` | Closure-based flush with automatic list collection |
| `flush(enqueue)` | Public flush entry; delegates to `flushImpl()` |
| `flushRecords()` | Public flush; returns list of pending records |

### 2.4 Bulk Processing (Hash-of-Lists)

For high-throughput columnar processing, the pipeline can pass data in hash-of-lists format where keys are field names and values are lists of field values:

```qore
# Columnar format
{"name": ("Alice", "Bob"), "score": (95, 87)}

# Equivalent to row format
({"name": "Alice", "score": 95}, {"name": "Bob", "score": 87})
```

- `processRecordsBulk(hash<auto> records)` — processes columnar data
- `supportsBulkApi()` → True if processor handles bulk format efficiently (default: True)
- `AbstractDataProcessor::hashListToRecords()` / `recordsToHashList()` — static conversion helpers

## 3. Class Hierarchy

### 3.1 Complete Hierarchy

```
AbstractDataProcessor (qlib/DataProvider/AbstractDataProcessor.qc)
│   Serializable interface for pipeline data processing
│   submit/flush/processRecord methods
│   Thread-local data management (setThreadLocalData)
│   Capability declaration (getProcessorCapabilities)
│   Bulk processing (processRecordsBulk)
│   Static helpers: hashListToRecords(), recordsToHashList()
│
AbstractDataProvider (qlib/DataProvider/AbstractDataProvider.qc)
│   Full data provider interface — navigation, metadata, pipeline support
│   ProviderInfo: supports_pipeline_processor, processor_capabilities, pipeline_processor_options
│   Constructor options via DataProviderOptionInfo
│   checkOptions() for option validation
│   tryGetActionOptionsForPipelineProcessorWithOptionsImpl() for dynamic options
│   getPipelineProcessorOutputTypeImpl() / getPipelineProcessorInputTypeImpl()
│
├── Simple Processors
│   │   Override processRecordImpl() directly
│   │   Constructor: no-arg (navigation) + hash<auto> options (execution)
│   │
│   ├── QoreFilterRecordsProcessor (DPC_FILTER)
│   │     where_cond expression → evalGenericExpression() → pass or drop
│   │     Dynamic options with expression builder UI
│   │
│   ├── QoreSelectFieldsProcessor (DPC_TRANSFORM)
│   │     Comma-separated field specs, supports renaming: new_name=old_name
│   │
│   ├── QoreSetFieldsProcessor (DPC_TRANSFORM)
│   │     Sets field values: name=value specs
│   │
│   ├── QoreRemoveFieldsProcessor (DPC_TRANSFORM)
│   │     Removes specified fields from records
│   │
│   ├── QoreSearchReplaceProcessor (DPC_TRANSFORM)
│   │     Regex search-replace on field values
│   │
│   ├── QoreRecordLimitProcessor (DPC_FILTER)
│   │     Limits total record count passing through
│   │
│   └── QoreGroupByProcessor (DPC_AGGREGATE)
│         Groups records by field values, computes aggregates per group
│         Aggregate operations: count(), sum(), avg(), min(), max(), first(), last()
│         Emits one record per group on flush
│
├── AbstractAnalyticsProcessor (qlib/DataProvider/AbstractAnalyticsProcessor.qc)
│   │   Window-based streaming analytics base class
│   │   Two emission modes:
│   │     ANALYTICS_MODE_WINDOW (0): tumbling window, emit per window
│   │     ANALYTICS_MODE_RECORD (1): sliding window, emit per record
│   │   Per-field circular buffers with automatic window management
│   │   Type coercion: int/float/number/string(numeric)/bool → float
│   │   Subclasses implement:
│   │     getAnalyticsMode() → mode constant
│   │     analyzeWindow(field, values, total_count) → event hash (window mode)
│   │     analyzeRecord(field, value, history, total_count) → event hash (record mode)
│   │     getAnalyticsOutputType() → AbstractDataProviderType
│   │   BaseConstructorOptions: "fields" (comma-separated), "window_size" (int)
│   │
│   ├── QoreRunningStatsProcessor (window) → StatisticsEventInfo
│   ├── QoreMovingAverageProcessor (record) → MovingAverageEventInfo
│   ├── QoreTrendAnalysisProcessor (record) → TrendEventInfo
│   ├── QoreAnomalyDetectionProcessor (record) → AnomalyEventInfo
│   ├── QorePercentileProcessor (window) → PercentileEventInfo
│   ├── QoreCorrelationProcessor (window) → CorrelationEventInfo
│   ├── QoreCrossCorrelationProcessor (window) → CrossCorrelationEventInfo
│   ├── QoreHistogramProcessor (window) → HistogramEventInfo
│   ├── QoreRateOfChangeProcessor (record) → RateOfChangeEventInfo
│   ├── QoreEWMAControlChartProcessor (record) → EWMAControlChartEventInfo
│   └── QoreSeasonalityDetectionProcessor (window) → SeasonalityDetectionEventInfo
│
├── ML Processors (qlib/DataProviderML/)
│   │   Wrap C++ ml module algorithms as pipeline processors
│   │   All DPC_AGGREGATE or DPC_ANALYTICS
│   │   Window-oriented: accumulate records, fit model, emit results on flush
│   │
│   ├── QoreIsolationForestProcessor (AGGREGATE) — anomaly detection
│   ├── QoreLOFProcessor (AGGREGATE) — local outlier factor
│   ├── QoreDBSCANClusteringProcessor (AGGREGATE) — DBSCAN clustering
│   ├── QoreKMeansProcessor (AGGREGATE) — K-Means clustering
│   ├── QoreGMMProcessor (AGGREGATE) — Gaussian mixture model
│   ├── QorePCAProcessor (AGGREGATE) — principal component analysis
│   ├── QoreLinearRegressionProcessor (AGGREGATE) — linear regression
│   ├── QoreHoltWintersProcessor (ANALYTICS) — Holt-Winters forecasting
│   ├── QoreSeasonalDecompositionProcessor (ANALYTICS) — time series decomposition
│   └── QoreOnnxModelProcessor (AGGREGATE) — ONNX Runtime inference (optional)
│
└── External API Processors
    │   Inherit from module-specific base class (e.g., BigMLDataProviderBase)
    │   Base class handles connection/auth resolution from constructor options
    │   Per-record processors: call API in processRecordImpl()
    │   Batch processors: accumulate in processRecordImpl(), call batch API in flushRecordsImpl()
    │
    └── BigML Processors (qlib/BigMLDataProvider/)
        ├── BigMLDataProviderBase — REST client, doPost/doGet/doDelete, pollUntilFinished
        ├── BigMLPredictionProcessor (TRANSFORM) — per-record prediction
        ├── BigMLAnomalyScoreProcessor (TRANSFORM) — per-record anomaly scoring
        ├── BigMLCentroidProcessor (TRANSFORM) — per-record cluster assignment
        ├── BigMLTopicDistributionProcessor (TRANSFORM) — per-record topic distribution
        ├── BigMLTrainAndPredictProcessor (AGGREGATE) — train model then predict
        ├── BigMLBatchPredictionProcessor (AGGREGATE) — batch prediction
        ├── BigMLBatchAnomalyScoreProcessor (AGGREGATE) — batch anomaly scoring
        ├── BigMLBatchCentroidProcessor (AGGREGATE) — batch cluster assignment
        ├── BigMLProjectionProcessor (TRANSFORM) — per-record PCA projection
        ├── BigMLBatchProjectionProcessor (AGGREGATE) — batch PCA projection
        ├── BigMLLinearRegressionProcessor (TRANSFORM) — per-record linear regression
        ├── BigMLForecastProcessor (TRANSFORM) — per-record time series forecast
        ├── BigMLLogisticRegressionProcessor (TRANSFORM) — per-record classification
        └── BigMLAssociationSetProcessor (TRANSFORM) — per-record association rules
```

### 3.2 Root Data Providers (Processor Navigation)

Processors are discovered through the data provider navigation tree:

```
qore-processors/              → QoreProcessorsDataProvider
  ├── filter                   → QoreFilterRecordsProcessor
  ├── select-fields            → QoreSelectFieldsProcessor
  ├── set-fields               → QoreSetFieldsProcessor
  ├── remove-fields            → QoreRemoveFieldsProcessor
  ├── search-replace           → QoreSearchReplaceProcessor
  ├── record-limit             → QoreRecordLimitProcessor
  ├── group-by                 → QoreGroupByProcessor
  ├── running-stats            → QoreRunningStatsProcessor
  ├── moving-average           → QoreMovingAverageProcessor
  ├── trend-analysis           → QoreTrendAnalysisProcessor
  ├── anomaly-detection        → QoreAnomalyDetectionProcessor
  ├── percentile               → QorePercentileProcessor
  ├── correlation              → QoreCorrelationProcessor
  ├── cross-correlation        → QoreCrossCorrelationProcessor
  ├── histogram                → QoreHistogramProcessor
  ├── rate-of-change           → QoreRateOfChangeProcessor
  ├── ewma-control-chart       → QoreEWMAControlChartProcessor
  └── seasonality-detection    → QoreSeasonalityDetectionProcessor

ml-processors/                → MLProcessorsDataProvider
  ├── isolation-forest         → QoreIsolationForestProcessor
  ├── lof                      → QoreLOFProcessor
  ├── dbscan-clustering        → QoreDBSCANClusteringProcessor
  ├── kmeans-clustering        → QoreKMeansProcessor
  ├── gmm                      → QoreGMMProcessor
  ├── pca                      → QorePCAProcessor
  ├── linear-regression        → QoreLinearRegressionProcessor
  ├── holt-winters             → QoreHoltWintersProcessor
  ├── seasonal-decomposition   → QoreSeasonalDecompositionProcessor
  └── onnx-model               → QoreOnnxModelProcessor (if ONNX available)

bigml/                        → BigMLDataProvider
  ├── predict                  → BigMLPredictionProcessor
  ├── anomaly-score            → BigMLAnomalyScoreProcessor
  ├── centroid                 → BigMLCentroidProcessor
  ├── topic-distribution       → BigMLTopicDistributionProcessor
  ├── train-and-predict        → BigMLTrainAndPredictProcessor
  ├── batch-predict            → BigMLBatchPredictionProcessor
  ├── batch-anomaly-score      → BigMLBatchAnomalyScoreProcessor
  ├── batch-centroid           → BigMLBatchCentroidProcessor
  ├── projection               → BigMLProjectionProcessor
  ├── batch-projection         → BigMLBatchProjectionProcessor
  ├── linear-regression        → BigMLLinearRegressionProcessor
  ├── forecast                 → BigMLForecastProcessor
  ├── logistic-regression      → BigMLLogisticRegressionProcessor
  ├── association-set          → BigMLAssociationSetProcessor
  └── (dynamic TypeScript children if connection available)
```

Root providers declare:
```qore
const ProviderInfo = <DataProviderInfo>{
    "supports_children": True,
    "children_can_support_pipeline_processors": True,
};
```

The root provider implements:
- `getChildProviderNamesImpl()` → returns `keys Children`
- `getChildProviderImpl(name)` → switch statement returning `new ProcessorClass()`

## 4. Analytics Event Types

`AbstractAnalyticsProcessor.qc` defines typed hashdecl event types for each analytics processor:

### 4.1 Base Event

```qore
public hashdecl AnalyticsEventInfo {
    string event_type;    # e.g., "statistics", "moving-average", "trend"
    string field;         # Field name this event was computed for
    int window_size;      # Configured window size
    int window_count;     # Total windows/records processed for this field
}
```

### 4.2 Event Types

| Hashdecl | Inherits | Key Fields | Used By |
|----------|----------|------------|---------|
| `StatisticsEventInfo` | AnalyticsEventInfo | count, sum, avg, min_val, max_val, stddev, variance | running-stats |
| `MovingAverageEventInfo` | AnalyticsEventInfo | sma, ema, ema_alpha | moving-average |
| `TrendEventInfo` | AnalyticsEventInfo | slope, intercept, r_squared, direction ("rising"/"falling"/"flat") | trend-analysis |
| `AnomalyEventInfo` | AnalyticsEventInfo | z_score, threshold, is_anomaly, record_value, mean, stddev | anomaly-detection |
| `PercentileEventInfo` | AnalyticsEventInfo | percentiles (hash), iqr, min_val, max_val, skewness, kurtosis, count | percentile |
| `CorrelationEventInfo` | (standalone) | correlations (pairwise), matrix (full), count | correlation |
| `CrossCorrelationEventInfo` | (standalone) | field_x, field_y, max_lag, lag_correlations, best_lag, best_correlation | cross-correlation |
| `HistogramEventInfo` | AnalyticsEventInfo | bin_edges (list), bin_counts (list), bin_frequencies (list), bin_width, count | histogram |
| `RateOfChangeEventInfo` | AnalyticsEventInfo | record_value, rate_of_change, acceleration, percent_change | rate-of-change |
| `EWMAControlChartEventInfo` | AnalyticsEventInfo | record_value, ewma, ucl, lcl, center, out_of_control, signal | ewma-control-chart |
| `SeasonalityDetectionEventInfo` | AnalyticsEventInfo | has_seasonality, dominant_period, dominant_correlation, periods (list), autocorrelation (list) | seasonality-detection |

### 4.3 Analytics Mode Behavior

**ANALYTICS_MODE_WINDOW (tumbling window):**
1. Records accumulate in per-field buffers
2. When `buffers{field}.lsize() >= window_size` → call `analyzeWindow(field, values, total_count)`
3. Buffer is cleared after each window emission
4. On flush: remaining partial windows are analyzed and emitted

**ANALYTICS_MODE_RECORD (sliding window):**
1. Each record triggers immediate analysis against the historical window
2. `analyzeRecord(field, value, history, total_count)` receives the current value AND the sliding window **before** the value is added (critical for anomaly detection — the test value must not dampen its own score)
3. Window is maintained as a circular buffer of size `window_size`
4. Emits one event per record per field

## 5. Action Catalog Integration

### 5.1 Action Type

Pipeline processor actions use `DPAT_PIPELINE_PROCESSOR = 10` from the `DataProviderActionType` enum.

Related maps:
- `ActionCodeFlagMap{DPAT_PIPELINE_PROCESSOR}` → `"supports_pipeline_processors"` (app flag)
- `ActionAttrMap{DPAT_PIPELINE_PROCESSOR}` → `"supports_pipeline_processor"` (provider attribute)
- `ActionNameMap{DPAT_PIPELINE_PROCESSOR}` → `"PIPELINE_PROCESSOR"` (display name)

### 5.2 DataProviderActionInfo (Full Reference)

The complete hashdecl for action registration (`DataProviderActionCatalog.qc:349-474`):

```qore
public hashdecl DataProviderActionInfo {
    # Required fields
    string app;                          # Application name (e.g., "Qore", "BigML")
    string action;                       # Unique action name within the app
    string path;                         # Data provider path (e.g., "/predict")
    string display_name;                 # Human-readable name for UI
    string short_desc;                   # One-line plain text description
    string desc;                         # Full markdown description
    int action_code;                     # DPAT_PIPELINE_PROCESSOR for processors

    # Processor-specific (required for DPAT_PIPELINE_PROCESSOR)
    int processor_capabilities = 0;      # DPC_* bitfield (MUST match processor class)

    # Class-based instantiation
    *string cls;                         # Fully qualified class name (e.g., "DataProvider::QoreFilterRecordsProcessor")
    *string cls_mod;                     # Module to load_module() before Class::forName()

    # Options
    *hash<string, hash<ActionOptionInfo>> options;  # Constructor/runtime options
    bool data_dependent_options = False;  # Options change based on input data context
    bool has_dynamic_options;             # Action supports dynamic option generation
    *code get_dynamic_options;            # Closure to generate options dynamically

    # Type information
    *AbstractDataProviderType output_type;  # Static output type
    *code get_output_type;                  # Lazy output type closure (for expensive types)
    hash<string, bool> input_data_types;    # Acceptable input data types (DPAD_NORMAL, DPAD_RECORD_SET)
    string output_data_type;                # Output data type (DPAD_NORMAL)

    # Grouping and context
    *softlist<string> groups;            # Action categories (e.g., "Filtering", "Predictions")
    *string action_context;              # Context this action sets for child actions
    *bool requires_action_context;       # Requires parent action context
    *bool requires_child_eligibility_check;  # UI should check child eligibility

    # Data provider info (for record-based providers)
    *hash<DataProviderInfo> data_provider_info;  # Static provider info
    *string subtype;                     # Connection subtype
    *string action_val;                  # Event/message type (for DPAT_EVENT/DPAT_SEND_MESSAGE)

    # Record-based action additions
    *hash<auto> where_add;               # Added to "where" argument
    *hash<auto> options_add;             # Added to "options" argument
    *hash<auto> set_add;                 # Added to "set" argument

    # Metadata
    *string exec_type;                   # Free-form exec type info
    *hash<auto> metadata;                # Free-form metadata
    *hash<string, hash<DataProviderPathVarInfo>> path_vars;  # Path variable descriptions
}
```

### 5.3 ActionOptionInfo (Full Reference)

Options for processor constructor parameters (`DataProviderActionCatalog.qc:248-318`):

```qore
public hashdecl ActionOptionInfo inherits BaseOptionInfo {
    # Core
    AbstractDataProviderType type;           # Data type of the option
    bool required;                           # Is the option required?
    *auto default_value;                     # Default value

    # UI presentation
    bool preselected = False;                # Show in UI by default (auto-set if required)
    *softlist<auto> allowed_values;          # Enumerated values for dropdowns
    *string example_value;                   # Example value for placeholder

    # Grouping and validation
    *softlist<string> groups;                # Option groups
    *softlist<string> required_groups;       # Required group (at least one must be set)
    *softlist<string> exclusive_with;        # Mutually exclusive options
    *string validation_regex;                # Regex validation for string values

    # Reference data (dynamic allowed values)
    *string ref_data;                        # Reference data name for dynamic allowed values
    *string element_ref_data;                # Element-level reference data
    *string default_ref_data;                # Reference data for dynamic default value
    *string dynamic_type;                    # Runtime type retrieval tag

    # Expression support
    bool supports_templates = True;          # Allow template expressions
    bool supports_custom_values = True;      # Allow custom/literal values
    bool supports_expressions = True;        # Allow expression builder
    bool server_expression_handling = False;  # Server evaluates expressions
    *enum<UiOptionView> default_view;        # Default UI view (Template or Expression)

    # Location
    *string loc;                             # Where the option goes:
                                             #   "constructor" — processor constructor arg
                                             #   "where" — where clause
                                             #   "options" — options argument
                                             #   "set" — set argument for updates

    # Dependencies
    bool structural_determinate;             # Value determines structure of other options
    *softlist<string> depends_on;            # Options this depends on
    *list<string> on_change;                 # Actions on change (e.g., ("refetch",))
}
```

### 5.4 Registration Pattern

Actions are registered in the `.qm` module file's `init {}` block:

```qore
DataProviderActionCatalog::registerAction(<DataProviderActionInfo>{
    "app": AppName,
    "path": "/processor-name",
    "action": "processor-name",
    "display_name": "Human-Readable Name",
    "short_desc": "One-line plain text description",
    "desc": "Full **markdown** description with formatting",
    "action_code": DPAT_PIPELINE_PROCESSOR,
    "processor_capabilities": DPC_TRANSFORM,
    "cls": "ModuleName::ProcessorClassName",
    "groups": ("Category",),
    "options": DataProviderActionCatalog::getActionOptionFromFields(
        ProcessorClass::ConstructorOptions, {
            "preselected": True,
            "loc": "constructor",
        }
    ),
});
```

### 5.5 getActionOptionFromFields() Helper

Converts `DataProviderOptionInfo` or `AbstractDataField` hashdecls into `ActionOptionInfo` hashdecls for action registration. Located in `DataProviderActionCatalog.qc:1416+`.

```qore
static hash<string, hash<ActionOptionInfo>> getActionOptionFromFields(
    hash<auto> fields,          # ConstructorOptions hash or subset
    *hash<auto> additional_fields  # Extra fields merged into each option
)
```

**Behavior:**
- Converts `DataProviderOptionInfo` entries to `ActionOptionInfo`
- Resolves type objects from strings via `AbstractDataProviderTypeMap`
- Copies through: `default_value`, `required`, `sensitive`, `ref_data`, `preselected`, `on_change`, `allowed_values`, etc.
- Merges `additional_fields` into every option (e.g., `{"loc": "constructor", "preselected": True}`)
- Auto-sets `preselected: True` if `required` is True

**Selecting option subsets:**
```qore
# Register only specific options with overrides
DataProviderActionCatalog::getActionOptionFromFields(
    ProcessorClass::ConstructorOptions{"model", "input_fields",},  # hash slice
    {"preselected": True, "required": True, "loc": "constructor"}
)
```

### 5.6 Data-Dependent Options and on_change Pattern

For processors where one option's value determines available/visible options:

```qore
DataProviderActionCatalog::registerAction(<DataProviderActionInfo>{
    ...
    "data_dependent_options": True,  # Tell UI to refetch options on change
    "options":
        # First: the structural option with has_dependents and on_change
        DataProviderActionCatalog::getActionOptionFromFields(
            Processor::ConstructorOptions{"model_type",}, {
                "preselected": True,
                "required": True,
                "loc": "constructor",
                "has_dependents": True,
                "on_change": ("refetch",),  # UI refetches options when this changes
            }
        )
        # Then: options that depend on the structural option
        + DataProviderActionCatalog::getActionOptionFromFields(
            Processor::ConstructorOptions{"input_fields", "window_size",}, {
                "preselected": True,
                "required": True,
                "loc": "constructor",
            }
        )
        # Then: optional/conditional options
        + DataProviderActionCatalog::getActionOptionFromFields(
            Processor::ConstructorOptions{"objective_field",}, {
                "loc": "constructor",
            }
        ),
});
```

### 5.7 Dynamic Options via tryGetActionOptionsForPipelineProcessorWithOptionsImpl()

Processors can override this method to provide context-dependent options at runtime. The filter processor uses this for expression builder integration:

```qore
private *hash<string, hash<ActionOptionInfo>>
        tryGetActionOptionsForPipelineProcessorWithOptionsImpl(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
    return action.options + {
        "where_cond": <ActionOptionInfo>{
            "display_name": "Filter Expression",
            "desc": "A boolean expression to filter records",
            "type": AbstractDataProviderTypeMap."*bool",
            "supports_templates": False,
            "supports_custom_values": False,
            "supports_expressions": True,
            "server_expression_handling": True,
            "default_view": UiOptionView::Expression,
            "preselected": True,
            "loc": "constructor",
        },
    };
}
```

### 5.8 Output Type Declaration

Processors can declare their output type for UI schema display and downstream type checking:

```qore
private *AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
    return new HashDeclDataType(
        TypedHash::forName("BigMLDataProvider::BigMLPredictionEventInfo"));
}
```

Similarly, `getPipelineProcessorInputTypeImpl()` can declare expected input type.

## 6. Pipeline Execution Engine

### 6.1 DataProviderPipeline Architecture

`DataProviderPipeline` (`qlib/DataProvider/DataProviderPipeline.qc`) orchestrates multi-processor pipelines using a queue-based threading model.

**Key concepts:**
- Pipeline is composed of one or more `PipelineQueue` objects
- Each queue runs in its own background thread
- Queues contain a list of elements: `AbstractDataProcessor` objects or lists of child `PipelineQueue` objects (fan-out)
- Bounded queue with backpressure (configurable `size`)

### 6.2 PipelineQueue Threading Model

```
                    submit(record)
                         │
                         ▼
               ┌─────────────────┐
               │  PipelineQueue 0 │  (background thread)
               │                 │
               │  queue: [data]  │  ← bounded list, blocks on full
               │                 │
               │  elements:      │
               │  [Processor A]  │  → submitAndCollect(data)
               │  [Processor B]  │  → submitAndCollect(output_A)
               │  [child_queues] │  → fan-out to child PipelineQueues
               └─────────────────┘
                    │         │
          ┌─────────┘         └─────────┐
          ▼                             ▼
  ┌──────────────┐              ┌──────────────┐
  │ PipelineQueue 1│            │ PipelineQueue 2│
  │ [Processor C]  │            │ [Processor D]  │
  └──────────────┘              └──────────────┘
```

**Thread lifecycle:**
1. `PipelineQueue` constructor spawns background thread via `background run()`
2. Thread blocks on `cond.wait()` when queue is empty
3. Records are dequeued and processed through elements sequentially
4. Fan-out: last element can be a list of child `PipelineQueue` objects
5. `waitDone()` waits for queue empty AND `processing == False` (prevents premature return during fan-out)
6. `flushIntern()` calls `flushAndCollect()` on each processor, propagates results downstream
7. Thread decrements `Counter cnt` on exit (must happen before destructor to avoid self-deadlock)

**Backpressure:** When `queue.lsize() == size`, the submitting thread blocks on `cond.wait()` until a record is dequeued. `queue_full_count` tracks backpressure events.

### 6.3 Pipeline Status Codes

| Constant | Value | Description |
|----------|-------|-------------|
| `PS_ABORTED` | "ABORTED" | Pipeline aborted due to error |
| `PS_RUNNING` | "RUNNING" | Pipeline is actively processing |
| `PS_IDLE` | "IDLE" | Pipeline is idle/ready |

### 6.4 Pipeline Metrics System

The metrics system provides real-time observability into pipeline execution.

**PipelineInfo** — high-level pipeline metadata:
```qore
public hashdecl PipelineInfo {
    string name;           # Pipeline name
    *date start_time;      # Processing start time
    *date stop_time;       # Processing end time
    string status;         # PS_* status code
    int record_count;      # Total input records submitted
    int num_queues;        # Number of pipeline queues
    bool bulk;             # Bulk processing mode?
    date duration;         # Total processing time
    float duration_secs;   # Duration as float seconds
    float recs_per_sec;    # Throughput
}
```

**PipelineElementMetrics** — per-processor metrics:
```qore
public hashdecl PipelineElementMetrics {
    string element_id;              # "queue_id:element_index"
    string element_type;            # "processor", "mapper", or "queue"
    *string class_name;             # Processor class name
    int records_processed = 0;      # Records input to this element
    int records_output = 0;         # Records output from this element
    int processing_time_us = 0;     # Total processing time (microseconds)
    *int min_processing_time_us;    # Min per-record processing time
    *int max_processing_time_us;    # Max per-record processing time
    int queue_wait_time_us = 0;     # Time waiting in queue
    *int current_queue_depth;       # Current queue depth (queue elements only)
    *int max_queue_depth;           # Max observed queue depth
    int queue_full_count = 0;       # Backpressure events
    *date first_record_time;        # First record timestamp
    *date last_record_time;         # Last record timestamp
    int error_count = 0;            # Errors encountered
}
```

**PipelineMetricsDelta** — interval-based delta metrics:
```qore
public hashdecl PipelineMetricsDelta {
    int interval_ms;                # Milliseconds since previous snapshot
    int records_submitted = 0;      # Records submitted in this interval
    int records_completed = 0;      # Records completed in this interval
    float throughput_rps = 0.0;     # Records per second
    hash<string, hash<PipelineElementDelta>> element_metrics;
}
```

**PipelineMetricsSnapshot** — complete pipeline snapshot:
```qore
public hashdecl PipelineMetricsSnapshot {
    string pipeline_name;
    string status;
    *date start_time;
    date snapshot_time;
    int total_records_submitted = 0;
    int total_records_completed = 0;
    float throughput_rps = 0.0;
    float avg_latency_us = 0.0;
    hash<string, hash<PipelineElementMetrics>> element_metrics;
    int num_queues;
    bool bulk_mode;
    int total_errors = 0;
    *hash<auto> user_context;
    hash<PipelineMetricsDelta> delta;
}
```

**Metrics collection pattern:**
- `PipelineMetricsCollector` is a thread-safe internal class
- Batches updates (default: flush every 100 records or on idle)
- `recordBatchFlush()` atomically updates all metrics and triggers snapshots
- `AbstractPipelineMetricsCollector` is the user-facing interface; implement `reportMetrics()` and configure via `PipelineOptionInfo.metrics_collector`
- Snapshots are time-based (`getIntervalMs()`) or record-based (`getRecordInterval()`)

### 6.5 Pipeline Options

```qore
public hashdecl PipelineOptionInfo {
    *code<nothing(string, ...)> debug_log;     # Debug logging closure
    *code<nothing(string, ...)> error_log;     # Error logging closure
    *code<nothing(string, ...)> info_log;      # Info logging closure
    *code<nothing()> thread_callback;          # Thread-local setup for new queue threads
    *string name;                              # Pipeline name (auto-generated if missing)
    *AbstractPipelineMetricsCollector metrics_collector;  # Metrics callback
}
```

## 7. Action Session Execution

### 7.1 DpqlActionSession Flow

When a `DPAT_PIPELINE_PROCESSOR` action is executed via `DpqlActionSession` (`DpqlActionSession.qc:490-570`):

1. **Resolve app:** `DataProviderActionCatalog::getAppEx(app_name)`
2. **Resolve connection (optional):** For scheme-based apps, resolve the connection; pipeline processors can operate *without* a connection (unlike most other action types)
3. **Instantiate processor:**
   ```qore
   if (action.cls && action.action_code == DPAT_PIPELINE_PROCESSOR) {
       *hash<auto> extra;
       if (conn) {
           extra.connection = conn;  # Inject connection for API-backed processors
       }
       prov = DataProviderActionCatalog::getDataProviderForAction(action, \options, extra);
   }
   ```
4. **Navigate path:** Walk `action.path` segments via `getChildProviderEx(seg)`, substituting path variables
5. **Return provider:** The processor is ready for pipeline use

**Key difference from other action types:** Pipeline processors are instantiated via `cls` + constructor options, not through a connection's data provider. The connection is passed as an *extra* constructor option for processors that need API access.

### 7.2 getDataProviderForAction()

This method:
1. Calls `load_module(action.cls_mod)` if `cls_mod` is set
2. Resolves the class via `Class::forName(action.cls)`
3. Merges constructor options (those with `loc: "constructor"`) + `extra` options
4. Instantiates the class with the merged options hash

## 8. Thread-Local Data

### 8.1 Pattern

Processors may need context (connection info, user identity, pipeline metadata) that doesn't flow through data records. `AbstractDataProcessor` manages this via thread-local variables:

```qore
# Set before pipeline execution
processor.setThreadLocalData({"user_id": 123, "tenant": "acme"});

# Inside processRecordImpl():
string tenant = get_thread_data("tenant");  # Available during processing
```

### 8.2 Implementation

`submit()`, `processRecord()`, `processRecords()`, `processRecordsBulk()`, and `flushRecords()` all follow the same pattern:

1. Save current thread-local data: `tld = get_all_thread_data()`
2. Set processor's thread-local data: `save_thread_data(thread_local_data)`
3. On exit: clear processor's keys, restore saved data

This ensures thread-local data is scoped to the processing call and doesn't leak.

## 9. Connection-Backed Processor Pattern (BigML Example)

### 9.1 Base Class

`BigMLDataProviderBase` (`BigMLDataProviderBase.qc`) provides:

```qore
public class BigMLDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        const BaseConstructorOptions = {
            "bigmlrestclient": ...,           # Direct REST client object
            "bigmlrestclient_options": ...,   # Options for creating REST client
            "connection": ...,                # Connection object (injected by framework)
            "token": ...,                     # API key (sensitive)
            "username": ...,                  # Username
        };
    }

    private {
        BigMLRestClient::BigMLRestClient rest;
        *BigMLRestClient::BigMLRestConnection conn;
    }

    # Constructor resolves REST client from various option combinations:
    # 1. Direct bigmlrestclient object
    # 2. Connection object (from framework injection)
    # 3. Create from bigmlrestclient_options + token/username
    constructor(*hash<auto> options) { ... }

    # API helpers
    hash<auto> doPost(string path, *hash<auto> body) { ... }
    auto doGet(string path, *hash<auto> params) { ... }
    hash<auto> doDelete(string path) { ... }
    hash<auto> pollUntilFinished(string resource_path, int poll_interval_ms, int timeout_ms) { ... }
}
```

### 9.2 Connection Injection

When the action session resolves a connection-backed processor:
1. The connection object is resolved from the user's named connection
2. It's passed as `extra.connection` to the constructor
3. The base class constructor detects `options.connection instanceof BigMLRestConnection` and extracts the REST client

### 9.3 Dynamic Child Registration

BigML supports dynamically registered child providers for TypeScript actions:

```qore
static registerChild(string name, code generator) {
    dynamic_children{name} = generator;
}

private *AbstractDataProvider getChildProviderImpl(string name) {
    # Check static children first, then dynamic
    if (conn) {
        if (*code gen = dynamic_children{name}) {
            return gen(name, conn);
        }
    }
}
```

## 10. Error Handling

### 10.1 Processor Errors

- `PROCESSOR-ERROR` — thrown when processor is used without configuration (no constructor options) or when neither `submitImpl` nor `processRecordImpl` is overridden
- `CONSTRUCTOR-ERROR` — thrown by `checkOptions()` for invalid constructor options

### 10.2 Pipeline Error Handling

Pipeline errors are caught per-queue:
1. Error logged via `parent.reportError(self, ex)`
2. Metrics updated: `mc.recordError(id_str)`
3. Pipeline may abort depending on configuration
4. Pending metrics batch is flushed on error to avoid data loss

### 10.3 Action Session Errors

- `CONTEXT-ERROR` — connection resolution failure, missing required options, invalid scheme
- Wraps underlying exceptions with context about which app/action failed

## 11. Key Files Reference

| File | Purpose |
|------|---------|
| `qlib/DataProvider/AbstractDataProcessor.qc` | Base processor interface, DPC_* constants, dual API bridging |
| `qlib/DataProvider/AbstractAnalyticsProcessor.qc` | Window-based analytics base class, all event hashdecls |
| `qlib/DataProvider/AbstractDataProvider.qc` | Data provider base, ProviderInfo, pipeline processor type methods |
| `qlib/DataProvider/DataProviderActionCatalog.qc` | Action catalog, DPAT enum, ActionOptionInfo, getActionOptionFromFields() |
| `qlib/DataProvider/DataProviderPipeline.qc` | Pipeline engine, PipelineQueue, metrics system |
| `qlib/DataProvider/DpqlActionSession.qc` | Action session, processor instantiation from cls + options |
| `qlib/DataProvider/QoreProcessorsDataProvider.qc` | Built-in processor root provider and factory |
| `qlib/DataProvider/QoreFilterRecordsProcessor.qc` | Reference filter processor with dynamic options |
| `qlib/DataProvider/DataProvider.qm` | Built-in processor action catalog registrations |
| `qlib/DataProviderML/MLProcessorsDataProvider.qc` | ML processor root provider |
| `qlib/DataProviderML/DataProviderML.qm` | ML processor action catalog registrations |
| `qlib/BigMLDataProvider/BigMLDataProviderBase.qc` | BigML base class, REST client, connection handling |
| `qlib/BigMLDataProvider/BigMLProcessorsDataProvider.qc` | BigML processor root provider |
| `qlib/BigMLDataProvider/BigMLDataProvider.qm` | BigML processor action catalog registrations |
| `qlib/BigMLDataProvider/BigMLPredictionProcessor.qc` | Reference external API processor |

## 12. Design Decisions

### Why processors inherit AbstractDataProvider, not just AbstractDataProcessor

- Processors must participate in the data provider navigation tree for UI discovery
- `AbstractDataProvider` provides `ProviderInfo`, `getStaticInfoImpl()`, `pipeline_processor_options`
- `tryGetActionOptionsForPipelineProcessorWithOptionsImpl()` enables dynamic, context-dependent options
- `getPipelineProcessorOutputTypeImpl()` / `getPipelineProcessorInputTypeImpl()` support type checking

### Why two processing APIs exist

- Closure-based (`submitImpl`/`enqueue`) naturally supports expand (1→N) and aggregate patterns
- Return-based (`processRecordImpl`) is simpler for the common 1:1 transform and filter cases
- The bidirectional bridge with recursion guard means authors pick whichever is more natural; both are first-class

### Why action registration is separate from the processor class

- Action catalog entries contain UI metadata (display names, groups, option presentation, expression support) that doesn't belong in processing logic
- Multiple actions can be backed by the same class with different default options or option subsets
- Action discovery works without instantiating processor classes (no side effects)
- `data_dependent_options`, `on_change`, `has_dependents` are UI concerns that belong in registration

### Why PipelineQueue uses a background thread per queue

- Enables true parallelism in fan-out topologies
- Backpressure via bounded queues prevents memory exhaustion
- Thread-per-queue avoids complex async scheduling while providing isolation
- `processing` flag + `waitDone()` prevents premature pipeline completion during fan-out

### Why metrics use batch flushing

- Per-record metrics updates would create contention on the metrics lock
- Batch accumulation (default: 100 records) amortizes lock overhead
- Flushed on idle, error, and threshold to ensure timely reporting
- `recordBatchFlush()` atomically updates all elements to prevent partial snapshots

### Thread-local data pattern

- Avoids polluting the record stream with infrastructure concerns (connection info, user identity, pipeline metadata)
- Set/cleared around each processing call with save/restore of prior state
- Pipeline threads can use `PipelineOptionInfo.thread_callback` for additional thread-local setup
