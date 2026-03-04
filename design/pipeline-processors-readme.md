# Pipeline Processors — How It Works

## What Are Pipeline Processors?

Pipeline processors are pluggable data transformation units in the Qore DataProvider framework. Feed records in, the processor does something (transform, filter, aggregate, analyze, call an external ML API), and records come out the other side. Chain them together for complex data workflows.

```
[Source] → [Filter] → [Transform] → [ML Predict] → [Aggregate] → [Sink]
```

There are **49 processors** across 3 modules: 18 built-in Qore processors, 10 ML processors (native C++), and 14 BigML processors (cloud API), plus 7 third-party processors (Hugging Face, OpenAI, Anthropic, Google Gemini, etc.).

## Quick Start: Using a Processor

### Standalone — Single Record

```qore
# Create processor with options
QoreSelectFieldsProcessor proc({"fields": "name,email"});

# Process one record
*auto result = proc.processRecord({"name": "Alice", "email": "a@b.com", "age": 30});
# result = {"name": "Alice", "email": "a@b.com"}

# Filter: returns NOTHING when record doesn't match
QoreFilterRecordsProcessor filter({"where_cond": expr});
*auto filtered = filter.processRecord({"status": "inactive"});
# filtered = NOTHING (record dropped)
```

### Standalone — Multiple Records

```qore
# Process a list
QoreSelectFieldsProcessor proc({"fields": "name,score"});
list<auto> records = (
    {"name": "Alice", "score": 95, "age": 30},
    {"name": "Bob", "score": 87, "age": 25},
);
*list<auto> results = proc.processRecords(records);
# results = ({"name": "Alice", "score": 95}, {"name": "Bob", "score": 87})
```

### Standalone — Aggregate Processor (Accumulate + Flush)

```qore
QoreGroupByProcessor grouper({
    "group_by": "department",
    "aggregates": "count() as total, avg(salary) as avg_salary",
});

# Feed records — returns NOTHING (accumulating)
grouper.processRecord({"department": "Eng", "salary": 120000});
grouper.processRecord({"department": "Eng", "salary": 130000});
grouper.processRecord({"department": "Sales", "salary": 90000});

# Flush emits aggregated results
*list<auto> results = grouper.flushRecords();
# results = (
#   {"department": "Eng", "total": 2, "avg_salary": 125000.0},
#   {"department": "Sales", "total": 1, "avg_salary": 90000.0},
# )
```

### Standalone — Analytics Processor (Windowed)

```qore
QoreMovingAverageProcessor ma({
    "fields": "temperature",
    "window_size": 3,
});

# Each record emits a moving average event (sliding window, record-level mode)
auto e1 = ma.processRecord({"temperature": 20.0});
# e1 = {event_type: "moving-average", field: "temperature", sma: 20.0, ema: 20.0, ...}

auto e2 = ma.processRecord({"temperature": 22.0});
# e2.sma = 21.0  (average of [20, 22])

auto e3 = ma.processRecord({"temperature": 25.0});
# e3.sma = 22.33  (average of [20, 22, 25])

auto e4 = ma.processRecord({"temperature": 30.0});
# e4.sma = 25.67  (window slides: [22, 25, 30])
```

### Standalone — Closure-Based API (Fan-Out/Advanced)

```qore
# submitAndCollect() is the convenience wrapper
QoreSelectFieldsProcessor proc({"fields": "name"});
*list<auto> results = proc.submitAndCollect({"name": "Alice", "age": 30});
# results = ({"name": "Alice"},)

# Raw closure API — for maximum control
list<auto> output;
code enqueue = sub (auto data) { push output, data; };
proc.submit(enqueue, {"name": "Bob", "age": 25});
# output = ({"name": "Bob"},)

# Flush aggregate/analytics processors
*list<auto> flushed = proc.flushAndCollect();
```

### In a Pipeline (Multi-Processor Chain)

```qore
# Create pipeline with options
DataProviderPipeline pipeline(<PipelineOptionInfo>{
    "name": "sales-analysis",
    "debug_log": sub (string fmt, ...) { logger.debug(fmt, argv); },
    "error_log": sub (string fmt, ...) { logger.error(fmt, argv); },
});

# Append processors (executed in order)
pipeline.append(new QoreFilterRecordsProcessor({"where_cond": active_expr}));
pipeline.append(new QoreSelectFieldsProcessor({"fields": "region,revenue,date"}));
pipeline.append(new QoreMovingAverageProcessor({"fields": "revenue", "window_size": 7}));

# Feed records
foreach hash<auto> record in (records) {
    pipeline.submit(record);
}

# Wait for processing to complete
pipeline.waitDone();

# Flush any pending aggregate/analytics results
pipeline.flush();

# Get pipeline info
hash<PipelineInfo> info = pipeline.getInfo();
# info.record_count, info.duration_secs, info.recs_per_sec, etc.
```

## Six Processor Types

| Type | Constant | Cardinality | Behavior | Examples |
|------|----------|-------------|----------|----------|
| **TRANSFORM** | `DPC_TRANSFORM` | 1 → 1 | Every input produces exactly one output | `select-fields`, `set-fields`, `predict` |
| **FILTER** | `DPC_FILTER` | 1 → 0..1 | Pass matching records, drop others | `filter`, `record-limit` |
| **EXPAND** | `DPC_EXPAND` | 1 → N | One input produces multiple outputs | (custom implementations) |
| **AGGREGATE** | `DPC_AGGREGATE` | N → M | Accumulate records; emit on `flush()` | `group-by`, `batch-predict`, all ML algorithms |
| **SINK** | `DPC_SINK` | N → 0 | Consume records, write externally | (custom implementations) |
| **ANALYTICS** | `DPC_ANALYTICS` | N → M | Windowed streaming analysis; emit typed events | `moving-average`, `anomaly-detection`, `trend-analysis` |

## Writing a New Processor

### Transform Processor (Complete Example)

```qore
public namespace MyModule {
#! Converts specified string fields to uppercase
public class UpperCaseProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info — declares this as a pipeline processor
        const ProviderInfo = <DataProviderInfo>{
            "type": "UpperCaseProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options — defines the processor's configuration
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Field Names",
                "short_desc": "Comma-separated field names to uppercase",
                "desc": "A comma-separated list of field names whose string values will be "
                    "converted to uppercase; non-string fields are left unchanged",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
        };
    }

    private {
        #! Parsed field name list
        list<string> field_list;
        #! Whether the processor has been configured
        bool configured = False;
    }

    #! No-arg constructor — REQUIRED for data provider navigation
    /** The UI instantiates processors without arguments to inspect metadata (ProviderInfo,
        getName, getDesc). This constructor must not throw.
    */
    constructor() {
    }

    #! Constructor with options — called during pipeline execution
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        field_list = map trim($1), copts.fields.split(","), trim($1).val();
        if (!field_list) {
            throw "CONSTRUCTOR-ERROR", "no valid field names provided in 'fields' option";
        }
        configured = True;
    }

    #! Returns the processor name (used as child name in root provider)
    string getName() {
        return "uppercase";
    }

    #! Returns a description for UI display
    *string getDesc() {
        return "Converts specified string fields to uppercase; non-string fields are passed through unchanged";
    }

    #! Returns static provider info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record and returns the result
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR", "UpperCaseProcessor: not configured; provide constructor options";
        }
        hash<auto> result = record;
        foreach string f in (field_list) {
            if (exists result{f} && result{f}.typeCode() == NT_STRING) {
                result{f} = result{f}.upr();
            }
        }
        return result;
    }

    #! Does not support bulk API
    private bool supportsBulkApiImpl() {
        return False;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_TRANSFORM;
    }
}
}
```

### Filter Processor

```qore
public namespace MyModule {
#! Drops records where a specified field is empty/null
public class DropEmptyFieldProcessor inherits DataProvider::AbstractDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "type": "DropEmptyFieldProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_FILTER,
            "pipeline_processor_options": ConstructorOptions,
        };

        const ConstructorOptions = {
            "field": <DataProviderOptionInfo>{
                "display_name": "Field Name",
                "short_desc": "Field that must have a value",
                "desc": "Records where this field is NOTHING, empty string, or zero are dropped",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
        };
    }

    private { string field_name; bool configured = False; }

    constructor() {}

    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        field_name = copts.field;
        configured = True;
    }

    string getName() { return "drop-empty"; }
    *string getDesc() { return "Drops records where a specified field is empty or null"; }
    hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR", "DropEmptyFieldProcessor: not configured";
        }
        # Return NOTHING to drop, return record to pass through
        return record{field_name}.val() ? record : NOTHING;
    }

    private bool supportsBulkApiImpl() { return False; }
    private int getProcessorCapabilitiesImpl() { return DPC_FILTER; }
}
}
```

### Aggregate Processor

Aggregators accumulate records in `processRecordImpl()` (returning NOTHING) and emit results in `flushRecordsImpl()`:

```qore
public namespace MyModule {
#! Counts records and emits a single count result on flush
public class CountProcessor inherits DataProvider::AbstractDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "type": "CountProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        const ConstructorOptions = {
            "count_field": <DataProviderOptionInfo>{
                "display_name": "Count Field Name",
                "short_desc": "Name of the output count field",
                "desc": "The name of the field in the output record that contains the count",
                "type": AbstractDataProviderTypeMap."string",
                "default_value": "count",
            },
        };
    }

    private { int count = 0; string count_field = "count"; bool configured = False; }

    constructor() {}

    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.count_field.val()) {
            count_field = copts.count_field;
        }
        configured = True;
    }

    string getName() { return "count"; }
    hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Accumulate — return NOTHING (don't emit yet)
    private *auto processRecordImpl(auto record) {
        ++count;
        return NOTHING;  # CRITICAL: aggregators must return NOTHING during accumulation
    }

    #! Emit accumulated result on flush
    private *list<auto> flushRecordsImpl() {
        if (!configured || count == 0) {
            return;
        }
        *list<auto> result = ({count_field: count},);
        count = 0;  # Reset for potential reuse
        return result;
    }

    private int getProcessorCapabilitiesImpl() { return DPC_AGGREGATE; }
}
}
```

### Expand Processor

Expand processors generate multiple output records from a single input. Use the closure-based API (`submitImpl`) for fan-out:

```qore
public namespace MyModule {
#! Explodes an array field into individual records
public class ExplodeArrayProcessor inherits DataProvider::AbstractDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "type": "ExplodeArrayProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_EXPAND,
            "pipeline_processor_options": ConstructorOptions,
        };

        const ConstructorOptions = {
            "array_field": <DataProviderOptionInfo>{
                "display_name": "Array Field",
                "short_desc": "Field containing the list to explode",
                "desc": "The name of the field containing a list; one output record is "
                    "created per list element, with all other fields copied",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
        };
    }

    private { string array_field; bool configured = False; }

    constructor() {}
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        array_field = copts.array_field;
        configured = True;
    }

    string getName() { return "explode-array"; }
    hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Use closure-based API for expand (emits multiple records)
    private submitImpl(code enqueue, auto _data) {
        if (!configured) {
            throw "PROCESSOR-ERROR", "ExplodeArrayProcessor: not configured";
        }
        hash<auto> record = _data;
        auto arr = record{array_field};
        if (arr.typeCode() != NT_LIST) {
            # Not a list — pass through unchanged
            enqueue(record);
            return;
        }
        # Emit one record per array element
        hash<auto> base = record - array_field;
        foreach auto element in (arr) {
            enqueue(base + {array_field: element});
        }
    }

    private bool supportsBulkApiImpl() { return False; }
    private int getProcessorCapabilitiesImpl() { return DPC_EXPAND; }
}
}
```

### Analytics Processor (Windowed)

Extend `AbstractAnalyticsProcessor` for streaming analytics with window management:

```qore
public namespace MyModule {
#! Computes running min/max per window
public class MinMaxProcessor inherits DataProvider::AbstractAnalyticsProcessor {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "type": "MinMaxProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_ANALYTICS,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Reuse base options: "fields" (comma-separated) + "window_size" (int)
        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor() {}

    constructor(hash<auto> options) {
        # initAnalytics() validates options, parses fields, initializes circular buffers
        initAnalytics(options, ConstructorOptions);
    }

    string getName() { return "min-max"; }
    hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Tumbling window mode: emit one event when window is full
    int getAnalyticsMode() {
        return ANALYTICS_MODE_WINDOW;
    }

    #! Analyze a complete window of values for one field
    hash<auto> analyzeWindow(string field, list<float> values, int total_count) {
        float min_val = values[0];
        float max_val = values[0];
        foreach float v in (values) {
            if (v < min_val) min_val = v;
            if (v > max_val) max_val = v;
        }
        return {
            "event_type": "min-max",
            "field": field,
            "window_size": values.lsize(),
            "window_count": window_counts{field},
            "min_val": min_val,
            "max_val": max_val,
            "range": max_val - min_val,
        };
    }

    #! NOT USED in window mode — required because abstract
    hash<auto> analyzeRecord(string field, float value, list<float> history, int total_count) {
        return {};
    }

    AbstractDataProviderType getAnalyticsOutputType() {
        return AbstractDataProviderTypeMap."hash";
    }
}
}
```

### Connection-Backed External API Processor (BigML Pattern)

For processors that call external APIs, inherit from a module-specific base class that handles connection resolution:

```qore
public namespace MyModule {
#! Enriches records via an external prediction API
public class MyApiPredictionProcessor inherits MyApiDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "type": "MyApiPredictionProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! BaseConstructorOptions provides: connection, restclient, token, etc.
        const ConstructorOptions = BaseConstructorOptions + {
            "model_id": <DataProviderOptionInfo>{
                "display_name": "Model ID",
                "short_desc": "ID of the prediction model",
                "desc": "The unique identifier of the model to use for predictions",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "input_fields": <DataProviderOptionInfo>{
                "display_name": "Input Fields",
                "short_desc": "Record fields to send as input",
                "desc": "Comma-separated list of field names to send to the prediction API",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
        };
    }

    private { string model_id; list<string> input_fields; bool configured = False; }

    constructor() {}

    #! Call parent constructor to resolve connection/REST client
    constructor(hash<auto> options) : MyApiDataProviderBase(options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        model_id = copts.model_id;
        input_fields = map trim($1), copts.input_fields.split(","), trim($1).val();
        configured = True;
    }

    string getName() { return "predict"; }
    hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR", "MyApiPredictionProcessor: not configured";
        }
        # Extract input fields from record using hash slice
        hash<auto> input_data = record{input_fields,};

        # Call external API (inherited from base class)
        hash<auto> response = doPost("predictions", {
            "model": model_id,
            "input_data": input_data,
        });

        # Return enriched record (original + prediction results)
        return record + {
            "prediction": response.prediction,
            "confidence": response.confidence,
        };
    }

    #! Declare output type for UI schema display
    private *AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return new HashDeclDataType(
            TypedHash::forName("MyModule::PredictionEventInfo"));
    }

    private bool supportsBulkApiImpl() { return False; }
    private int getProcessorCapabilitiesImpl() { return DPC_TRANSFORM; }
}
}
```

**Base class pattern** (handles connection injection from the framework):

```qore
public class MyApiDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        const BaseConstructorOptions = {
            "connection": <DataProviderOptionInfo>{
                "display_name": "API Connection",
                "short_desc": "Connection object (injected by framework)",
                "type": AbstractDataProviderType::get(new Type("MyModule::MyRestConnection")),
                "desc": "Connection object; injected automatically for scheme-based apps",
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "API key for authentication",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "API key for authentication",
                "sensitive": True,
            },
        };
    }

    private { MyRestClient rest; }

    private constructor() {}

    constructor(*hash<auto> options) {
        if (options.connection instanceof MyRestConnection) {
            rest = options.connection.get(True);
        } else {
            rest = new MyRestClient(options);
        }
    }

    hash<auto> doPost(string path, *hash<auto> body) {
        hash<auto> response = rest.post(path, body);
        return response.body;
    }
}
```

## Registration Checklist

To make a processor available in the UI, you need 3 things:

### 1. Root Provider (Child Registration)

Add to the parent data provider's `Children` map and `getChildProviderImpl()`:

```qore
# In the root provider class:
public {
    const Children = {
        "my-processor": "MyProcessorClass",
        # ... other children
    };
}

private *list<string> getChildProviderNamesImpl() {
    return keys Children;
}

private *AbstractDataProvider getChildProviderImpl(string name) {
    switch (name) {
        case "my-processor":
            return new MyProcessorClass();
        # ... other cases
    }
}
```

### 2. Action Catalog Registration

In the `.qm` module file's `init {}` block. This makes the processor appear in the UI action palette:

```qore
DataProviderActionCatalog::registerAction(<DataProviderActionInfo>{
    # Identity
    "app": MyApp::AppName,
    "path": "/my-processor",
    "action": "my-processor",

    # Display
    "display_name": "My Processor",
    "short_desc": "Brief description for action list",
    "desc": "Full description with **markdown** formatting.\n\n"
        "Can include multiple paragraphs and `code examples`.",

    # Processor config
    "action_code": DPAT_PIPELINE_PROCESSOR,
    "processor_capabilities": DPC_TRANSFORM,
    "cls": "MyModule::MyProcessorClass",

    # Categorization
    "groups": ("Transformation",),

    # Options — converted from ConstructorOptions to ActionOptionInfo format
    "options": DataProviderActionCatalog::getActionOptionFromFields(
        MyProcessorClass::ConstructorOptions, {
            "preselected": True,
            "loc": "constructor",
        }
    ),
});
```

**Option registration patterns:**

```qore
# All options with same attributes
"options": DataProviderActionCatalog::getActionOptionFromFields(
    ProcessorClass::ConstructorOptions, {
        "preselected": True,
        "loc": "constructor",
    }
),

# Selective: only some options required + preselected, others optional
"options": DataProviderActionCatalog::getActionOptionFromFields(
    ProcessorClass::ConstructorOptions{"required_field1", "required_field2",}, {
        "preselected": True,
        "required": True,
        "loc": "constructor",
    }
) + DataProviderActionCatalog::getActionOptionFromFields(
    ProcessorClass::ConstructorOptions{"optional_field",}, {
        "loc": "constructor",
    }
),

# Dependent options: model_type determines other options' visibility
"data_dependent_options": True,
"options": DataProviderActionCatalog::getActionOptionFromFields(
    ProcessorClass::ConstructorOptions{"model_type",}, {
        "preselected": True,
        "required": True,
        "loc": "constructor",
        "has_dependents": True,
        "on_change": ("refetch",),  # UI refetches options when this changes
    }
) + DataProviderActionCatalog::getActionOptionFromFields(
    ProcessorClass::ConstructorOptions{"objective_field",}, {
        "loc": "constructor",
    }
),
```

### 3. Factory (if new module)

If creating a new module (not adding to an existing root provider), create a factory and register in `FactoryMap`.

## Dynamic Options (Expression Builder)

Override `tryGetActionOptionsForPipelineProcessorWithOptionsImpl()` for context-dependent options:

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
            "supports_expressions": True,       # Enable expression builder widget
            "server_expression_handling": True,  # Server evaluates the expression
            "default_view": UiOptionView::Expression,
            "preselected": True,
            "loc": "constructor",
        },
    };
}
```

## Output Types

Declare output types for UI schema display and downstream type checking:

```qore
private *AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
    return new HashDeclDataType(
        TypedHash::forName("MyModule::MyEventInfo"));
}
```

## Pipeline Metrics

Monitor pipeline execution with the metrics system:

```qore
class MyMetricsCollector inherits AbstractPipelineMetricsCollector {
    reportMetrics(hash<PipelineMetricsSnapshot> snapshot) {
        printf("Pipeline %s: %d/%d records, %.1f rps, %d errors\n",
            snapshot.pipeline_name,
            snapshot.total_records_completed,
            snapshot.total_records_submitted,
            snapshot.throughput_rps,
            snapshot.total_errors);

        # Per-element metrics
        foreach hash<auto> i in (snapshot.element_metrics.pairIterator()) {
            hash<PipelineElementMetrics> em = i.value;
            printf("  %s (%s): %d in / %d out, avg %.1f us/rec\n",
                em.element_id, em.class_name ?? em.element_type,
                em.records_processed, em.records_output,
                em.records_processed > 0
                    ? (em.processing_time_us / em.records_processed).toFloat() : 0.0);
        }
    }

    int getIntervalMs() { return 5000; }     # Report every 5 seconds
    int getRecordInterval() { return 1000; }  # Or every 1000 records
}

DataProviderPipeline pipeline(<PipelineOptionInfo>{
    "name": "my-pipeline",
    "metrics_collector": new MyMetricsCollector(),
});
```

## Error Handling

### Common Processor Errors

| Error | Cause | Fix |
|-------|-------|-----|
| `PROCESSOR-ERROR: not configured` | No-arg constructor used for execution | Pass constructor options hash |
| `PROCESSOR-ERROR: neither submitImpl nor processRecordImpl overridden` | Missing processing method | Override at least one |
| `CONSTRUCTOR-ERROR: option 'x' is required` | Missing required option | Provide all required options |
| `PIPELINE-SUBMISSION-ABORTED` | Pipeline shutting down or aborted | Check `pipeline.getInfo().status` |

### External API Error Handling

```qore
private *auto processRecordImpl(auto record) {
    try {
        hash<auto> response = doPost("prediction", input_data);
        return enriched_record;
    } catch (hash<ExceptionInfo> ex) {
        if (ex.err == "BIGML-API-ERROR") {
            # Option 1: Skip record (return NOTHING)
            # Option 2: Rethrow to abort pipeline
            throw "PROCESSOR-ERROR", sprintf("API error: %s", ex.desc);
        }
        rethrow;
    }
}
```

## Testing Processors

### Test Commands

```bash
# Core processors
qore examples/test/qlib/DataProvider/QoreProcessors.qtest -v

# Analytics processors
qore examples/test/qlib/DataProvider/QoreAnalyticsProcessors.qtest -v

# Statistical processors
qore examples/test/qlib/DataProvider/QoreStatisticalProcessors.qtest -v

# ML processors (requires ml module)
qore examples/test/qlib/DataProviderML/DataProviderMLProcessors.qtest -v

# BigML processors (requires BIGML_USERNAME + BIGML_API_KEY env vars)
qore examples/test/qlib/BigMLDataProvider/BigMLDataProvider.qtest -v
```

### Test Pattern

```qore
#!/usr/bin/env qore
%new-style
%strict-args
%require-types

%requires QUnit
%requires DataProvider

class MyProcessorTest inherits QUnit::Test {
    constructor() : Test("MyProcessorTest", "1.0") {
        addTestCase("provider info", \testProviderInfo());
        addTestCase("transform", \testTransform());
        addTestCase("filter", \testFilter());
        addTestCase("aggregate", \testAggregate());
        set_return_value(main());
    }

    testProviderInfo() {
        # Test no-arg constructor (navigation mode)
        MyProcessor proc();
        hash<DataProviderInfo> info = proc.getStaticInfoImpl();
        assertTrue(info.supports_pipeline_processor);
        assertEq(DPC_TRANSFORM, info.processor_capabilities);
        assertTrue(exists info.pipeline_processor_options);
        assertEq("my-processor", proc.getName());
    }

    testTransform() {
        UpperCaseProcessor proc({"fields": "name,email"});
        auto result = proc.processRecord({"name": "alice", "email": "a@b.com", "age": 30});
        assertEq("ALICE", result.name);
        assertEq("A@B.COM", result.email);
        assertEq(30, result.age);  # Non-target fields unchanged

        # Test with processRecords (list)
        auto results = proc.processRecords(({"name": "x"}, {"name": "y"}));
        assertEq(2, results.lsize());
        assertEq("X", results[0].name);

        # Test capabilities
        assertEq(DPC_TRANSFORM, proc.getProcessorCapabilities());
        assertFalse(proc.supportsBulkApi());
    }

    testFilter() {
        DropEmptyFieldProcessor proc({"field": "email"});
        # Pass through
        assertTrue(exists proc.processRecord({"email": "a@b.com"}));
        # Drop
        assertNothing(proc.processRecord({"email": ""}));
        assertNothing(proc.processRecord({"name": "no-email"}));
    }

    testAggregate() {
        CountProcessor proc({"count_field": "total"});
        # Accumulation returns NOTHING
        assertNothing(proc.processRecord({"x": 1}));
        assertNothing(proc.processRecord({"x": 2}));
        assertNothing(proc.processRecord({"x": 3}));
        # Flush returns accumulated result
        *list<auto> results = proc.flushRecords();
        assertEq(1, results.lsize());
        assertEq(3, results[0].total);
        # Flush again — nothing pending
        assertNothing(proc.flushRecords());
    }
}
```

## Debugging Tips

1. **Processor returns NOTHING unexpectedly** — For filters, NOTHING means "drop record". For transforms, make sure you're not accidentally returning NOTHING from `processRecordImpl()`.

2. **Aggregate processor never emits** — Call `flush()` or `flushRecords()` after all records are submitted. Aggregators only emit during flush.

3. **PROCESSOR-ERROR: not configured** — The no-arg constructor was used instead of the options constructor. Ensure pipeline execution passes options correctly.

4. **Analytics processor emits wrong number of events** — Check `getAnalyticsMode()`: `ANALYTICS_MODE_WINDOW` emits once per full window; `ANALYTICS_MODE_RECORD` emits once per record. Window mode also emits partial windows on flush.

5. **Options don't appear in UI** — Check that action registration includes `"loc": "constructor"` in `getActionOptionFromFields()`. Without `loc`, the UI doesn't know where to place the option.

6. **Connection not available** — Ensure the base class constructor handles the `connection` option injected by the framework. `BaseConstructorOptions` must include a `"connection"` entry.

7. **Multiple outputs from transform** — If `processRecordImpl()` returns a list, it's interpreted as multiple output records. Wrap in a single-element list if the output is actually a list value: `return (list_value,)`.

## Available Processors — Complete Reference

### Built-in (18) — `qlib/DataProvider/`

| Name | Capability | Key Options | Description |
|------|-----------|-------------|-------------|
| `filter` | FILTER | `where_cond` (expression) | Drop non-matching records |
| `select-fields` | TRANSFORM | `fields` (comma-sep, supports `new=old`) | Select/rename fields |
| `set-fields` | TRANSFORM | `fields` (comma-sep `name=value`) | Set field values |
| `remove-fields` | TRANSFORM | `fields` (comma-sep) | Remove fields |
| `search-replace` | TRANSFORM | `fields`, `pattern`, `replacement` | Regex search-replace |
| `record-limit` | FILTER | `limit` (int) | Limit record count |
| `group-by` | AGGREGATE | `group_by`, `aggregates` | Group + aggregate (count/sum/avg/min/max) |
| `running-stats` | ANALYTICS | `fields`, `window_size` | count/sum/avg/stddev/variance per window |
| `moving-average` | ANALYTICS | `fields`, `window_size`, `ema_alpha` | SMA + EMA per record |
| `trend-analysis` | ANALYTICS | `fields`, `window_size` | slope/intercept/r_squared/direction per record |
| `anomaly-detection` | ANALYTICS | `fields`, `window_size`, `threshold` | Z-score anomalies per record |
| `percentile` | ANALYTICS | `fields`, `window_size`, `percentiles` | Quantiles/IQR/skewness/kurtosis per window |
| `correlation` | ANALYTICS | `fields`, `window_size` | Pairwise correlation matrix per window |
| `cross-correlation` | ANALYTICS | `field_x`, `field_y`, `window_size`, `max_lag` | Lag correlation per window |
| `histogram` | ANALYTICS | `fields`, `window_size`, `num_bins` | Bin counts/frequencies per window |
| `rate-of-change` | ANALYTICS | `fields`, `window_size` | First/second derivatives per record |
| `ewma-control-chart` | ANALYTICS | `fields`, `window_size`, `ema_alpha` | EWMA UCL/LCL monitoring per record |
| `seasonality-detection` | ANALYTICS | `fields`, `window_size`, `min_period`, `max_period` | Autocorrelation periodicity per window |

### ML (9+1) — `qlib/DataProviderML/`

| Name | Capability | Key Options | Algorithm |
|------|-----------|-------------|-----------|
| `isolation-forest` | AGGREGATE | `fields`, `window_size`, `n_trees`, `sample_size`, `contamination` | Anomaly detection |
| `lof` | AGGREGATE | `fields`, `window_size`, `k_neighbors`, `contamination` | Local Outlier Factor |
| `dbscan-clustering` | AGGREGATE | `fields`, `window_size`, `eps`, `min_points` | Density clustering |
| `kmeans-clustering` | AGGREGATE | `fields`, `window_size`, `k_clusters`, `max_iterations` | K-Means clustering |
| `gmm` | AGGREGATE | `fields`, `window_size`, `num_components`, `covariance_type` | Gaussian mixtures |
| `pca` | AGGREGATE | `fields`, `window_size`, `num_components` | Dimensionality reduction |
| `linear-regression` | AGGREGATE | `fields`, `window_size`, `target_field` | Linear regression |
| `holt-winters` | ANALYTICS | `fields`, `window_size`, `alpha`, `beta`, `gamma`, `seasonal_period` | Exponential smoothing |
| `seasonal-decomposition` | ANALYTICS | `fields`, `window_size`, `seasonal_period` | Trend/seasonal/residual |
| `onnx-model` | AGGREGATE | `model_path`, `fields`, `window_size` | ONNX Runtime (optional) |

### BigML (14) — `qlib/BigMLDataProvider/`

| Name | Capability | Key Options | Description |
|------|-----------|-------------|-------------|
| `predict` | TRANSFORM | `model`, `input_fields` | Per-record ML prediction |
| `anomaly-score` | TRANSFORM | `anomaly`, `input_fields`, `threshold` | Per-record anomaly scoring |
| `centroid` | TRANSFORM | `cluster`, `input_fields` | Per-record cluster assignment |
| `topic-distribution` | TRANSFORM | `topicmodel`, `input_fields` | Per-record topic distribution |
| `projection` | TRANSFORM | `pca`, `input_fields` | Per-record PCA projection |
| `linear-regression` | TRANSFORM | `linearregression`, `input_fields` | Per-record linear regression prediction |
| `forecast` | TRANSFORM | `timeseries`, `input_fields`, `horizon` | Per-record time series forecast |
| `logistic-regression` | TRANSFORM | `logisticregression`, `input_fields` | Per-record classification prediction |
| `association-set` | TRANSFORM | `association`, `input_fields` | Per-record association rule matching |
| `train-and-predict` | AGGREGATE | `model_type`, `input_fields`, `objective_field`, `window_size` | Train + predict in one step |
| `batch-predict` | AGGREGATE | `model`, `input_fields`, `window_size` | Batch prediction |
| `batch-anomaly-score` | AGGREGATE | `anomaly`, `input_fields`, `window_size`, `threshold` | Batch anomaly scoring |
| `batch-centroid` | AGGREGATE | `cluster`, `input_fields`, `window_size` | Batch cluster assignment |
| `batch-projection` | AGGREGATE | `pca`, `input_fields`, `window_size` | Batch PCA projection |

## Further Reading

- [Pipeline Processor Design](pipeline-processors.md) — Full architecture, class hierarchy, API reference, design decisions
- [ML Architecture](ml-architecture.md) — C++ ML module and DataProviderML processor layer
- [Data Provider Development Guide](data-provider-development-guide.md) — General data provider patterns
