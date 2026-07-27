# JSON Module Enhancement Design: Streaming and Schema Support

## Overview

This document outlines the design for three major enhancements to the Qore JSON module:

1. **SAX-style Streaming JSON Parser** - Memory-efficient incremental parsing
2. **NDJSON/JSON Lines Support** - Line-delimited JSON streaming
3. **JSON Schema Validation** - Schema-based validation and type generation

## 1. SAX-Style Streaming JSON Parser

### Motivation

The current `parse_json()` function loads the entire JSON document into memory before returning. For large JSON files (multi-GB), this is impractical. A SAX-style parser emits events as it parses, allowing processing without full document materialization.

### Feasibility in C++

**Absolutely feasible.** JSON SAX parsing is simpler than XML because JSON has only 7 value types:
- Object (start/end)
- Array (start/end)
- String
- Number
- Boolean (true/false)
- Null

The existing parser in `ql_json.qpp` is already a recursive descent parser. The modification involves:
1. Adding an event callback mechanism
2. Optionally building values only when requested

Several C++ libraries demonstrate this approach:
- **RapidJSON** - SAX-style API with `Handler` concept
- **simdjson** - On-demand parsing with lazy evaluation
- **nlohmann/json** - SAX parser with `json_sax` interface

### API Design

#### 1.1 Event Types

```qore
# JSON SAX event types
const JsonSaxEvent = {
    "ObjectStart": 1,    # {
    "ObjectEnd": 2,      # }
    "ArrayStart": 3,     # [
    "ArrayEnd": 4,       # ]
    "Key": 5,            # object key string
    "String": 6,         # string value
    "Number": 7,         # number value (int or float)
    "Boolean": 8,        # true or false
    "Null": 9,           # null
};
```

#### 1.2 Event Data Structure

```qore
hashdecl JsonSaxEventInfo {
    int type;           # JsonSaxEvent constant
    *string key;        # for Key events
    *auto value;        # for String, Number, Boolean events
    int depth;          # nesting depth (0 = root)
    int line;           # source line number
    int column;         # source column
}
```

#### 1.3 Core Classes (C++ Binary Module)

```qore
#! SAX-style JSON parser that emits events
class JsonSaxParser {
    #! Create parser with event callback
    constructor(code<void(hash<JsonSaxEventInfo>)> event_handler);

    #! Parse JSON string
    parse(string json);

    #! Parse from input stream (incremental)
    parseStream(InputStream stream, *int chunk_size);

    #! Stop parsing (can be called from event handler)
    stop();
}
```

#### 1.4 Iterator Classes (Qore Module)

```qore
#! Iterator that yields complete values at a specified JSON path/depth
class JsonSaxIterator inherits AbstractIterator {
    #! Create iterator for elements at specified path
    #! @param stream input stream
    #! @param element_path JSON Pointer path to iterate (e.g., "/items" or "/data/records")
    constructor(InputStream stream, string element_path);

    #! Create iterator from file location
    constructor(string location, string element_path);

    bool next();
    auto getValue();
    bool valid();
}

#! Input stream variant
class InputStreamJsonSaxIterator inherits JsonSaxIterator {
    constructor(InputStream stream, string element_path, *string encoding);
}
```

#### 1.5 Example Usage

```qore
# Event-driven parsing
JsonSaxParser parser(sub (hash<JsonSaxEventInfo> event) {
    switch (event.type) {
        case JsonSaxEvent.ObjectStart:
            printf("Object started at depth %d\n", event.depth);
            break;
        case JsonSaxEvent.Key:
            printf("Key: %s\n", event.key);
            break;
        case JsonSaxEvent.String:
            printf("String value: %s\n", event.value);
            break;
    }
});
parser.parseStream(file.getInputStream());

# Iterator-based parsing (like XML SaxIterator)
# Given: {"data": {"items": [{"id": 1}, {"id": 2}, {"id": 3}]}}
InputStreamJsonSaxIterator it(stream, "/data/items");
while (it.next()) {
    hash<auto> item = it.getValue();  # Returns {"id": 1}, then {"id": 2}, etc.
    process_item(item);
}
```

### Implementation Strategy

1. **Modify existing parser** in `ql_json.qpp`:
   - Add `JsonSaxHandler` abstract class with virtual methods for each event
   - Create `JsonSaxParser` that takes a handler/callback
   - The existing `get_json_value()` becomes the basis for `parse_sax_value()`

2. **Add streaming support**:
   - Buffer management for `InputStream` reading
   - Handle partial JSON across buffer boundaries
   - Track position (line/column) for error messages

3. **Implement iterators** in Qore:
   - `JsonSaxIterator` uses `JsonSaxParser` internally
   - Accumulates values at target path
   - Yields complete objects/arrays when path depth returns to target

### C++ Implementation Sketch

```cpp
// Event handler interface
class JsonSaxHandler {
public:
    virtual bool onObjectStart() = 0;
    virtual bool onObjectEnd() = 0;
    virtual bool onArrayStart() = 0;
    virtual bool onArrayEnd() = 0;
    virtual bool onKey(const QoreString& key) = 0;
    virtual bool onString(const QoreString& value) = 0;
    virtual bool onNumber(int64 value) = 0;
    virtual bool onNumber(double value) = 0;
    virtual bool onBoolean(bool value) = 0;
    virtual bool onNull() = 0;
    // return false to stop parsing
};

// SAX parser function
int parse_json_sax(const char* buf, int& line_number,
                   JsonSaxHandler& handler, ExceptionSink* xsink);
```

---

## 2. NDJSON/JSON Lines Support

### Motivation

NDJSON (Newline Delimited JSON) is a standard format for streaming JSON data where each line is a complete JSON value. Used by:
- OpenAI streaming API responses
- Log aggregation systems
- Data pipelines (Apache Kafka, AWS Kinesis)
- MongoDB import/export

### Specification Compliance

Following the [NDJSON Specification](https://github.com/ndjson/ndjson-spec):
- Media type: `application/x-ndjson`
- File extension: `.ndjson`
- Each line is valid JSON followed by `\n` (0x0A)
- Optional `\r\n` (0x0D 0x0A) line endings accepted
- UTF-8 encoding required
- Empty lines are ignored

### API Design

#### 2.1 Reader Classes

```qore
#! Iterator for reading NDJSON streams
class NdjsonIterator inherits AbstractIterator {
    #! Create from input stream
    constructor(InputStream stream, *hash<auto> options);

    #! Create from file location
    constructor(string location, *hash<auto> options);

    #! Options:
    #! - skip_empty: bool (default True) - skip empty lines
    #! - skip_invalid: bool (default False) - skip invalid JSON lines
    #! - max_line_length: int (default 10MB) - maximum line buffer size

    bool next();
    auto getValue();       # Returns parsed JSON value
    string getLine();      # Returns raw line (for debugging)
    int getLineNumber();   # Current line number
    bool valid();
}

#! Input stream variant with encoding support
class InputStreamNdjsonIterator inherits NdjsonIterator {
    constructor(InputStream stream, *string encoding, *hash<auto> options);
}
```

#### 2.2 Writer Classes

```qore
#! Writer for NDJSON streams
class NdjsonWriter {
    #! Create writer to output stream
    constructor(OutputStream stream, *hash<auto> options);

    #! Options:
    #! - line_ending: string (default "\n") - "\n" or "\r\n"
    #! - flush_each: bool (default False) - flush after each line

    #! Write a JSON value as a line
    write(auto value);

    #! Flush the stream
    flush();

    #! Close the stream
    close();
}
```

#### 2.3 Convenience Functions

```qore
#! Parse NDJSON string, returns list of values
list<auto> parse_ndjson(string ndjson, *hash<auto> options);

#! Generate NDJSON string from list of values
string make_ndjson(list<auto> values, *hash<auto> options);
```

#### 2.4 Example Usage

```qore
# Reading NDJSON log file
InputStreamNdjsonIterator it(FileInputStream(log_file));
while (it.next()) {
    hash<auto> entry = it.getValue();
    if (entry.level == "ERROR") {
        process_error(entry);
    }
}

# Writing NDJSON stream
NdjsonWriter writer(http_response.getOutputStream());
for (int i = 0; i < 1000; i++) {
    writer.write({"id": i, "timestamp": now_us()});
}
writer.close();

# Streaming HTTP response (e.g., OpenAI-style)
while (auto chunk = stream.read()) {
    NdjsonIterator it(StringInputStream(chunk));
    while (it.next()) {
        process_chunk(it.getValue());
    }
}
```

### Implementation Notes

1. **Line buffering**: Read chunks from stream, split on newlines
2. **Partial line handling**: Buffer incomplete lines across reads
3. **Memory limits**: Enforce `max_line_length` to prevent OOM
4. **Error recovery**: `skip_invalid` option for fault-tolerant parsing

---

## 3. JSON Schema Validation

### Motivation

JSON Schema provides:
1. **Validation** - Verify JSON data conforms to expected structure
2. **Documentation** - Self-describing API contracts
3. **Code generation** - Generate Qore types from schemas
4. **Consistency** - Swagger/OpenAPI3/AsyncAPI all use JSON Schema internally

### Specification Compliance

Target: **JSON Schema Draft 2020-12** (latest stable)
- Core vocabulary: structure, references
- Validation vocabulary: type, format, constraints
- Applicator vocabulary: if/then/else, allOf/anyOf/oneOf

### API Design

#### 3.1 Schema Class

```qore
#! JSON Schema validator and processor
class JsonSchema {
    #! Load schema from hash
    constructor(hash<auto> schema);

    #! Load schema from JSON string
    constructor(string schema_json);

    #! Load schema from file/URL location
    static JsonSchema load(string location);

    #! Validate data against schema
    #! @return True if valid
    #! @throws JSON-SCHEMA-VALIDATION-ERROR if invalid (when throw_on_error=True)
    bool validate(auto data, *bool throw_on_error);

    #! Validate and return detailed errors
    hash<JsonSchemaValidationResult> validateWithErrors(auto data);

    #! Get schema as hash
    hash<auto> getSchema();

    #! Get schema ID/URI
    *string getId();

    #! Resolve $ref references (dereference)
    JsonSchema dereference();
}
```

#### 3.2 Validation Result

```qore
hashdecl JsonSchemaValidationError {
    string path;          # JSON Pointer to error location
    string keyword;       # Failed keyword (e.g., "type", "required")
    string message;       # Human-readable error message
    auto expected;        # Expected value/type
    auto actual;          # Actual value
}

hashdecl JsonSchemaValidationResult {
    bool valid;
    list<hash<JsonSchemaValidationError>> errors;
}
```

#### 3.3 Type Generation

```qore
#! Generate Qore type information from JSON Schema
class JsonSchemaTypeGenerator {
    constructor(JsonSchema schema);

    #! Generate AbstractDataProviderType for the schema
    AbstractDataProviderType generateType();

    #! Generate hashdecl definition string
    string generateHashdecl(string name);

    #! Get field definitions for data provider
    hash<string, AbstractDataField> getFields();
}
```

#### 3.4 Schema Registry

```qore
#! Registry for schema resolution and caching
class JsonSchemaRegistry {
    #! Register a schema by URI
    register(string uri, JsonSchema schema);

    #! Resolve a schema by URI (with caching)
    JsonSchema resolve(string uri);

    #! Clear cache
    clear();
}
```

#### 3.5 Example Usage

```qore
# Basic validation
JsonSchema schema('{
    "type": "object",
    "properties": {
        "name": {"type": "string"},
        "age": {"type": "integer", "minimum": 0}
    },
    "required": ["name"]
}');

hash<auto> data = {"name": "John", "age": 30};
if (schema.validate(data)) {
    printf("Valid!\n");
}

# Validation with errors
hash<auto> bad_data = {"age": -5};
hash<JsonSchemaValidationResult> result = schema.validateWithErrors(bad_data);
if (!result.valid) {
    for (hash<JsonSchemaValidationError> err in result.errors) {
        printf("Error at %s: %s\n", err.path, err.message);
    }
}
# Output:
# Error at : missing required property "name"
# Error at /age: value -5 is less than minimum 0

# Type generation for data providers
JsonSchemaTypeGenerator gen(schema);
AbstractDataProviderType type = gen.generateType();
```

### Supported Keywords (Draft 2020-12)

#### Core
- `$schema`, `$id`, `$ref`, `$defs`, `$anchor`

#### Validation
- **Type**: `type`, `enum`, `const`
- **Numeric**: `minimum`, `maximum`, `exclusiveMinimum`, `exclusiveMaximum`, `multipleOf`
- **String**: `minLength`, `maxLength`, `pattern`, `format`
- **Array**: `minItems`, `maxItems`, `uniqueItems`, `items`, `prefixItems`, `contains`
- **Object**: `properties`, `patternProperties`, `additionalProperties`, `required`, `propertyNames`, `minProperties`, `maxProperties`

#### Applicators
- `allOf`, `anyOf`, `oneOf`, `not`
- `if`, `then`, `else`

#### Format (with optional strict validation)
- `date-time`, `date`, `time`, `duration`
- `email`, `uri`, `uri-reference`, `uuid`
- `ipv4`, `ipv6`
- `regex`, `json-pointer`

### Implementation Strategy

1. **Pure Qore implementation** initially (no C++ required)
   - Schema parsing and representation
   - Validation logic
   - Error collection

2. **Performance optimization** later if needed
   - Critical path validation in C++
   - Compiled validators for repeated use

3. **Integration points**:
   - `JsonSchemaDataProvider` - validates records against schema
   - Swagger/OpenAPI integration - reuse existing type converters

---

## 4. Data Provider Integration

### JsonSaxDataProvider

```qore
#! Data provider for streaming JSON parsing
public class JsonSaxDataProvider inherits AbstractDataProvider {
    public {
        const ConstructorOptions = {
            "location": <DataProviderOptionInfo>{
                "display_name": "JSON Location",
                "short_desc": "The location of the JSON data to parse",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "File path, URL, or other location resolvable by FileLocationHandler",
            },
            "stream": <DataProviderOptionInfo>{
                "display_name": "Input Stream",
                "short_desc": "An input stream for JSON data",
                "type": AbstractDataProviderType::get(new Type("InputStream")),
                "desc": "An input stream for JSON data; mutually exclusive with location",
            },
            "element_path": <DataProviderOptionInfo>{
                "display_name": "Element Path",
                "short_desc": "JSON Pointer path to elements to iterate",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "JSON Pointer path (RFC 6901) to the array elements to iterate",
                "required": True,
            },
        };
    }
    # ... implementation using JsonSaxIterator
}
```

### NdjsonDataProvider

```qore
#! Data provider for NDJSON/JSON Lines files
public class NdjsonDataProvider inherits AbstractDataProvider {
    public {
        const ConstructorOptions = {
            "location": <DataProviderOptionInfo>{
                "display_name": "NDJSON Location",
                "short_desc": "The location of the NDJSON data",
                "type": AbstractDataProviderType::get(StringType),
            },
            "stream": <DataProviderOptionInfo>{
                "display_name": "Input Stream",
                "type": AbstractDataProviderType::get(new Type("InputStream")),
            },
            "skip_invalid": <DataProviderOptionInfo>{
                "display_name": "Skip Invalid Lines",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "Skip lines that are not valid JSON instead of throwing an error",
                "default_value": False,
            },
        };
    }
    # ... implementation using NdjsonIterator
}
```

### JsonSchemaDataProvider

```qore
#! Data provider that validates records against a JSON Schema
public class JsonSchemaDataProvider inherits AbstractDataProvider {
    public {
        const ConstructorOptions = {
            "schema": <DataProviderOptionInfo>{
                "display_name": "JSON Schema",
                "short_desc": "The JSON Schema to validate against",
                "type": AbstractDataProviderType::get(StringOrHashType),
                "desc": "JSON Schema as string, hash, or file location",
                "required": True,
            },
            "source": <DataProviderOptionInfo>{
                "display_name": "Source Data Provider",
                "type": AbstractDataProviderType::get(new Type("AbstractDataProvider")),
                "desc": "The source data provider to read and validate records from",
            },
        };
    }
    # ... validates each record against schema
}
```

---

## 5. Module Organization

### New Files

```
qlib/
├── JsonSaxDataProvider/
│   ├── JsonSaxDataProvider.qm
│   ├── JsonSaxDataProvider.qc
│   └── JsonSaxDataProviderFactory.qc
├── NdjsonDataProvider/
│   ├── NdjsonDataProvider.qm
│   ├── NdjsonDataProvider.qc
│   ├── NdjsonDataProviderFactory.qc
│   ├── NdjsonIterator.qc
│   └── NdjsonWriter.qc
├── JsonSchema/
│   ├── JsonSchema.qm
│   ├── JsonSchema.qc
│   ├── JsonSchemaValidator.qc
│   ├── JsonSchemaTypeGenerator.qc
│   ├── JsonSchemaRegistry.qc
│   └── JsonSchemaDataProvider.qc

src/
├── QC_JsonSaxParser.qpp      # C++ SAX parser class
├── QC_JsonSaxIterator.qpp    # C++ iterator class
└── (existing files)
```

### Version Planning

- **v1.11**: NDJSON support (pure Qore, quick win)
- **v1.12**: SAX-style streaming (C++ implementation)
- **v1.13**: JSON Schema validation (pure Qore initially)

---

## 6. Testing Strategy

### SAX Parser Tests
- Large file parsing (multi-GB)
- Memory usage verification (constant memory)
- All JSON value types
- Nested structures at various depths
- Unicode and escape sequences
- Error handling (malformed JSON)
- Stream interruption handling

### NDJSON Tests
- Standard compliance
- Empty lines handling
- Invalid line recovery
- Large line handling
- Concurrent read/write
- Various line endings

### JSON Schema Tests
- All keyword validation
- $ref resolution
- Nested schemas
- Error message quality
- Type generation accuracy
- Integration with existing Swagger/OpenAPI code

---

## 7. Dependencies and Compatibility

### No External Dependencies Required
- SAX parser: Extends existing C++ code
- NDJSON: Pure Qore using existing `parse_json()`
- JSON Schema: Pure Qore

### Qore Version Requirements
- Minimum: Qore 2.0 (for modern features)
- Recommended: Latest stable

### Backward Compatibility
- All new functionality is additive
- Existing APIs unchanged
- New module imports required for new features
