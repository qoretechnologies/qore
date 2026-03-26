# Data Provider Development Guide

This guide covers patterns, examples, and common pitfalls for building Qore data provider integrations.

A typical integration consists of:
1. **REST Client Module** (`*RestClient.qm`) - HTTP communication and authentication
2. **Data Provider Module** (`*DataProvider/`) - Data provider API and action catalog

## File Structure

```
qlib/
  MyServiceRestClient.qm           # REST client and connection
  MyServiceDataProvider/
    MyServiceDataProvider.qm       # Module definition, registerApp, actions
    MyServiceDataProviderBase.qc   # Base class with REST client
    MyServiceDataProvider.qc       # Root provider
    MyServiceItemsDataProvider.qc  # Collection provider
    MyServiceEventsDataProvider.qc # Events container
    MyServiceEventProvider.qc      # Event provider
    MyServiceDataTypes.qc          # Data types

examples/test/qlib/MyServiceDataProvider/
    MyServiceDataProvider.qtest    # Integration tests (REQUIRED)
```

---

## REST Client Module

### Connection Scheme Registration

Add to `qlib/ConnectionProvider/ConnectionSchemeCache.qc`:

```qore
const SchemeMap = {
    "myservice": "MyServiceRestClient",
};
```

### Connection Options

Use `apikey` instead of `token` (inherited `token` has special handling that causes issues):

```qore
"options": RestConnection::ConnectionScheme.options + {
    "apikey": <ConnectionOptionInfo>{
        "display_name": "API Key",
        "type": "string",
        "sensitive": True,
        "preselected": True,
    },
    "domain": <ConnectionOptionInfo>{
        "display_name": "Domain",
        "type": "string",
        "default_value": ".com",
        "allowed_values": (
            <AllowedValueInfo>{"value": ".com", "display_name": "US"},
            <AllowedValueInfo>{"value": ".eu", "display_name": "EU"},
        ),
    },
},
```

### Required Connection Options

Mark connection options as required via `required_options` in the `ConnectionScheme` (NOT `required: True` in individual options - that field doesn't exist in `ConnectionOptionInfo`):

```qore
const ConnectionScheme = <ConnectionSchemeInfo>{
    "options": RestConnection::ConnectionScheme.options + {
        "account_id": <ConnectionOptionInfo>{...},
        "apikey": <ConnectionOptionInfo>{...},
    },
    "required_options": "account_id,apikey",  // Comma-separated string
};
```

### Auto-URL Connections

For `auto_url: True`, implement `getConfig()`:

```qore
private static hash<auto> getConfig(hash<auto> config) {
    string domain = config.opts.domain ?? ".com";
    return config + {"url": sprintf("myscheme://api.service%s/v1", domain)};
}
```

### OAuth2 Configuration

**Critical for refresh tokens:**

```qore
public {
    const DefaultAuthArgs = {
        "access_type": "offline",
        "prompt": "consent",
    };
}
```

**In getConnectionOptions() - always rebuild URLs from domain:**

```qore
*hash<auto> getConnectionOptions(*hash<auto> rtopts) {
    hash<auto> rv = RestConnection::getConnectionOptions(rtopts);

    string domain = rv.domain ?? ".com";
    rv.oauth2_auth_url = sprintf("https://accounts.service%s/oauth/v2/auth", domain);
    rv.oauth2_token_url = sprintf("https://accounts.service%s/oauth/v2/token", domain);

    if (!rv.oauth2_auth_args) {
        rv.oauth2_auth_args = DefaultAuthArgs;
    }
    return rv;
}
```

### Ping Path Rules

- **With base path in URL** (e.g., `/books/v3`): Use relative path `organizations` (no leading slash)
- **Without base path**: Use absolute path `/v3/organizations`

Leading slash = absolute path that bypasses base URL path.

---

## Data Provider Module

### Factory Registration

Add to `qlib/DataProvider/DataProvider.qc`:

```qore
const FactoryMap = {
    "myservice": "MyServiceDataProvider",
};
```

**Without this entry:** Module loads fine but doesn't appear in Qorus apps list.

### App Registration

```qore
DataProviderActionCatalog::registerApp(<DataProviderAppInfo>{
    "name": AppName,
    "display_name": "My Service",
    "short_desc": "Integration with My Service",
    "desc": "Provides integration with [My Service](https://myservice.com) for managing "
        "resources, contacts, and billing.\n\n"
        "Supported operations:\n"
        "- **Resources**: create, update, delete, and list\n"
        "- **Contacts**: create, search, and manage\n"
        "- **Billing**: invoice creation and payment tracking",
    "scheme": "myservice",
    "logo": MyServiceLogo,
    "logo_file_name": "myservice-logo.svg",
    "logo_mime_type": MimeTypeSvg,
    "groups": (AppGroup::Finance,),  // Use enum, not strings!
});
```

### Description Formatting

- **`short_desc`**: Plain text, under 80 characters, single sentence — no markdown
- **`desc`**: Markdown-formatted text rendered in the UI. Use:
  - Backticks for code/field references: `` `field_name` ``, `` `True` ``, `` `False` ``, `` `NOTHING` ``
  - Backticks for numeric values with semantic meaning: `` `0` ``–`` `255` ``, `` `-100` ``–`` `100` ``
  - Backticks for URL schemes: `` `http://` ``, `` `mcp://` ``
  - Backticks for format/type values: `` `xlsx` ``, `` `csv` ``, `` `json` ``
  - **Bold** for emphasis on important caveats: `**Note**:`, `**Important**:`
  - Bullet lists for enumerating options or capabilities
  - `[links](url)` for documentation references
  - `\n\n` for paragraph breaks in multi-paragraph descriptions
  - Capital letter at the start of every sentence

Short descriptions (1-2 simple sentences) don't need markdown — focus formatting effort on longer, complex descriptions.

#### Long Descriptions (>500 chars)

Descriptions for complex options (draw commands, field format specifications, multi-feature configurations) should be structured with bold section headers and bullet lists:

```qore
"desc": "A hash describing the CSV fields. Keys are field names. Values are "
    "[field types](url) or a hash with the following keys:\n"
    "- `type`: a [field type string](url)\n"
    "- `format`: for `date` types, a [date format](url); for `int`, `float`, or "
        "`number` types, a number format string\n"
    "- `timezone`: for `date` types only; overrides any default time zone\n\n"
    "**Note**: setting this value also sets `headers` if not set automatically.",
```

For very long descriptions with many items (e.g., 20+ draw commands), group related items under bold section headers:

```qore
"desc": "List of draw command hashes. Each command has an `op` key.\n\n"
    "**Color and Style**:\n"
    "- `set_source_rgb` (r, g, b) — color values 0.0–1.0\n"
    "- `set_source_rgba` (r, g, b, a)\n\n"
    "**Paths**:\n"
    "- `move_to` (x, y)\n"
    "- `line_to` (x, y)\n\n"
    "**Drawing**:\n"
    "- `stroke` — draw the current path outline\n"
    "- `fill` — fill the current path",
```

#### Common Description Mistakes

| Mistake | Wrong | Correct |
|---------|-------|---------|
| Bare boolean | `If True then...` | `` If `True` then... `` |
| Bare nothing | `returns nothing` | `` returns `NOTHING` `` |
| Single quotes for code | `'xlsx' format` | `` `xlsx` format `` |
| Lowercase sentence start | `if true then...` | `If true then...` |
| Missing opening backtick | `` field_name` `` | `` `field_name` `` |
| Bare field reference | `see ssl_key_path` | `` see `ssl_key_path` `` |

#### Data Examples in Descriptions

When a field accepts structured data (JSON objects/arrays, CSV, delimited strings, regex patterns, or other format-specific syntax), the `desc` field **must** include:

1. A summary sentence describing what the field expects
2. A structured property list (for JSON objects) or format specification (for delimited strings)
3. A concrete `**Example**:` with backtick-wrapped sample data

```qore
# Poor: generic description
"desc": "JSON array of text annotation objects to add",

# Good: structured with properties and example
"desc": "JSON array of text annotation objects to add to the PDF.\n\n"
    "**Annotation object properties**:\n"
    "- `text` (required) — text to add\n"
    "- `x`, `y` (required) — position coordinates in points\n"
    "- `size` — font size (default: `12`)\n"
    "- `fontName` — font name (default: `Arial`)\n\n"
    "**Example**: `[{\"x\": 100, \"y\": 200, \"text\": \"Hello\", \"size\": 14}]`",
```

Fields that need data examples:
- JSON object/array fields — document properties and provide a sample object
- CSV/delimited format fields — document delimiter, column structure, and provide a sample
- Regex/pattern fields — describe syntax and provide an example pattern
- Fields with non-obvious format requirements (page ranges, coordinate systems, hex colors)

#### List Types vs Delimited Strings

When a field logically accepts multiple values of the same type (e.g., multiple URLs, IDs, tags), use a **list type** rather than a delimited string:

```qore
# Poor: comma-delimited string
"url": {
    "type": StringOrNothingType,
    "desc": "Comma-separated URLs to process",
},

# Good: proper list type with join in doRequestImpl()
"url": {
    "type": SoftListOrNothingType,
    "desc": "Source URLs to process",
},

# In doRequestImpl():
if (req.url.typeCode() == NT_LIST) {
    req.url = (foldl $1 + "," + $2, req.url);
}
```

**Why**: List types render as proper list inputs in the UI (add/remove items). Delimited strings require users to manually concatenate values.

**When to keep strings**: When the delimiter format includes structured sub-fields (e.g., `page;fieldName;value`) or range syntax (e.g., `0, 2-5, 7-`), keep as `StringOrNothingType` since list semantics don't apply.

### Base Class Pattern

```qore
public class MyDataProviderBase inherits AbstractDataProvider {
    private { MyRestClient rest; }

    constructor(MyRestClient rest) { self.rest = rest; }

    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        rest = copts.myrestclient ?? new MyRestClient({"apikey": copts.apikey});
    }
}
```

### Reference Data (Dropdowns)

For `ref_data` in action options:

```qore
*hash<string, bool> getSupportedReferenceData() {
    return {"customers": True, "items": True};
}

private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> opts) {
    if (type == "customers") {
        return map <AllowedValueInfo>{"value": $1.id, "display_name": $1.name},
               doGet("/customers").content;
    }
}
```

### Request Type Fields Pattern

Request type classes should declare a `const Fields` hash in a `public {}` block. This allows action registration code to reference fields directly via `ClassName::Fields` without instantiating the type. The same constant is used in the constructor to add fields to the type.

**Important:** Type classes **must** be declared inside the `public namespace` block in the `.qc` file for `ClassName::Fields` to be resolvable from the `.qm` file. Classes declared outside the namespace block cannot have their constants referenced via `ClassName::ConstantName` from other module files.

```qore
public namespace MyServiceDataProvider {
    # Type classes MUST be inside the namespace block

    public class MyCreateRequestDataType inherits HashDataType {
        public {
            const Fields = {
                "name": {
                    "type": StringType,
                    "display_name": "Name",
                    "short_desc": "Resource name",
                    "desc": "Name of the resource to create",
                    "example_value": "My Resource",
                },
                "type": {
                    "type": StringType,
                    "display_name": "Type",
                    "short_desc": "Resource type",
                    "desc": "Type of resource",
                    "allowed_values": (
                        <AllowedValueInfo>{"value": "standard", "display_name": "Standard"},
                        <AllowedValueInfo>{"value": "premium", "display_name": "Premium"},
                    ),
                },
                "description": {
                    "type": StringOrNothingType,
                    "display_name": "Description",
                    "short_desc": "Resource description",
                    "desc": "Optional description of the resource",
                },
            };
        }

        constructor() {
            addQoreFields(Fields);
        }
    }
}  # end namespace
```

Data provider classes should declare `ResponseType` and `RequestType` as static members:

```qore
public class MyCreateDataProvider inherits MyDataProviderBase {
    public {
        static MyCreateRequestDataType RequestType();
        static MyResponseDataType ResponseType();
    }
    # ...
}
```

These are referenced in action registration as `MyCreateDataProvider::ResponseType`.

---

## Action Registration

### DPAT_FIND - Critical Rule

**Every action option MUST exist in the data provider's `SearchOptions`.**

Framework passes action options to `searchRecordsImpl()` via `search_options`. Missing options cause:
```
SEARCH-OPTION-ERROR: invalid options: ["organization_id"]; supported options: [...]
```

**Pattern:**

```qore
// Action registration
"options": {
    "organization_id": <ActionOptionInfo>{...},
    "status": <ActionOptionInfo>{...},
},

// Data provider - MUST match
const SearchOptions = {
    "organization_id": <DataProviderOptionInfo>{...},
    "status": <DataProviderOptionInfo>{...},
};

const ProviderInfo = <DataProviderInfo>{
    "search_options": SearchOptions,  // Required!
};
```

### DPAT_FIND_SINGLE

Point to **collection provider**, add ID as search option:

```qore
// Action
"path": "/items",  // Collection, not /items/get
"action_code": DPAT_FIND_SINGLE,
"options": {
    "itemID": <ActionOptionInfo>{"required": True, "ref_data": "items"},
},

// Data provider SearchOptions
"itemID": <DataProviderOptionInfo>{...},

// In searchRecordsImpl
if (search_options.itemID) {
    return single item by ID;
}
```

### DPAT_API

Standard request/response pattern. **Every DPAT_API action MUST have `options` and `output_type`** — without them the action appears in the catalog but is completely unusable (no form fields shown to users, no output schema).

Use `DataProviderActionCatalog::getActionOptionFromFields()` to generate action options from request type fields. This ensures action options stay in sync with the request type and inherit field metadata (descriptions, types, allowed values, example values).

**Preferred pattern — use class `Fields` constants:**

Request type classes should declare a `const Fields` in a `public {}` block (see [Request Type Fields Pattern](#request-type-fields-pattern) below). Reference fields via `ClassName::Fields` in action registration, and use `DataProviderClassName::ResponseType` for output_type:

```qore
DataProviderActionCatalog::registerAction(<DataProviderActionInfo>{
    "app": AppName,
    "path": "/resource/create",
    "action": "resource-create",
    "display_name": "Create Resource",
    "short_desc": "Create a new resource",
    "desc": "Create a new resource with the given parameters",
    "action_code": DPAT_API,
    "options": DataProviderActionCatalog::getActionOptionFromFields(
        MyCreateRequestDataType::Fields{"name", "type", "value"}, {
            "preselected": True,
            "required": True,
        },
    ) + DataProviderActionCatalog::getActionOptionFromFields(
        MyCreateRequestDataType::Fields - ("name", "type", "value"),
    ),
    "output_type": MyCreateDataProvider::ResponseType,
});
```

**Hash slice note:** `ClassName::Fields{"key1", "key2"}` (multiple keys) returns a hash slice. For a single key, use `ClassName::Fields{"key",}` (trailing comma) to force list context — without it, `ClassName::Fields{"key"}` returns a single value, not a hash.

**Alternative pattern — instantiate the type:**

If the request type class doesn't have a `Fields` constant, instantiate the type and use `getFields()`:

```qore
MyCreateRequestDataType CreateRequestType();

"options": DataProviderActionCatalog::getActionOptionFromFields(
    CreateRequestType.getFields(){"name", "type", "value"}, {
        "preselected": True,
        "required": True,
    },
) + DataProviderActionCatalog::getActionOptionFromFields(
    CreateRequestType.getFields() - ("name", "type", "value"),
),
"output_type": AbstractDataProviderType::get(
    new Type("hash<MyCreateResponse>")),
```

**Pattern — all fields optional (no required/preselected split):**

```qore
"options": DataProviderActionCatalog::getActionOptionFromFields(
    MyGetInfoRequestDataType::Fields,
),
```

For dynamic options, implement `getRequestTypeWithDataImpl()`.

### DPAT_EVENT

```qore
"action_code": DPAT_EVENT,
"action_val": "item-created",  // Must match getEventTypesImpl() key
```

---

## Event Providers

### Key Pattern: Options from Context

Event providers are created via path navigation **without constructor options**. Get values from context in `observersReady()`.

```qore
public class MyEventProvider inherits MyDataProviderBase, DelayedObservable {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "supports_observable": True,
        };

        // Options MUST be optional types
        const LocalOptions = {
            "itemID": <DataProviderOptionInfo>{
                "type": AbstractDataProviderTypeMap."*string",  // *string, not string
            },
        };
    }

    private { *string itemID; }

    observersReady() {
        if (!itemID) {
            *hash<auto> ctx = DataProviderDataContextHelper::getHash();
            itemID = ctx.itemID;
        }
        if (!itemID) {
            throw "ERROR", "itemID required";
        }
        // Start polling/webhooks
    }

    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            "item-created": <DataProviderMessageInfo>{
                "desc": "Fires when item created",
                "type": AbstractDataProviderType::get(AutoHashType),
            },
        };
    }

    private auto getExampleEventDataImpl(string event_id) {
        try {
            return doGet("/items", {"limit": 1}).items[0];
        } catch () {
            return {"item_id": "123", "name": "Example"};
        }
    }
}
```

### Events Container

```qore
public class MyEventsProvider inherits MyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "supports_children": True,
            "children_can_support_observers": True,
        };
    }

    private *list<string> getChildProviderNamesImpl() {
        return ("item-created", "item-updated");
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        return new MyEventProvider(rest);  // No options passed
    }
}
```

### Webhook Enrichment (Minimal Payload)

Some APIs send minimal webhook payloads containing only an object ID or resource URL instead of the full object data. In this case, `handleWebhookEvent()` must make an additional API request to fetch the complete object before notifying observers.

**When to use:** The external API's webhook payload lacks the fields needed by the event data type — typically just an ID, resource URL, or a small subset of fields.

**Pattern:**

```qore
public class MyEventProvider inherits MyDataProviderBase, DelayedObservable {
    private {
        *hash<auto> webhook_info;
        *string webhook_id;
    }

    observersReady() {
        # Create local webhook endpoint
        webhook_info = DataProvider::DataProviderWebhook::createWebhook({
            "method": "POST",
            "callback": \handleWebhookEvent(),
        });

        # Register webhook with external API
        hash<auto> response = doPost("/webhooks", {
            "url": webhook_info.url,
            "event": "item.created",
        });
        webhook_id = response.id;
    }

    private handleWebhookEvent(hash<auto> payload) {
        # payload is minimal, e.g.: {"id": "abc123", "event": "item.created"}

        # Fetch the full object from the API
        hash<auto> full_record;
        try {
            full_record = doGet("/items/" + payload.id);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "handleWebhookEvent: error fetching full record: %s",
                get_exception_string(ex));
            return;
        }

        # Notify observers with the enriched data
        notifyObservers(getEventType(), full_record);
    }

    private stopEventsIntern() {
        # Delete webhook from remote API
        if (webhook_id) {
            try {
                doDelete("/webhooks/" + webhook_id);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "error deleting webhook: %s", get_exception_string(ex));
            }
            remove webhook_id;
        }

        # Delete local webhook endpoint
        if (webhook_info) {
            DataProvider::DataProviderWebhook::deleteWebhook(webhook_info);
            remove webhook_info;
        }
    }
}
```

**If the payload contains a resource URL** (instead of just an ID), parse it to extract the path:

```qore
private hash<auto> fetchResourceData(string resource_url) {
    hash<auto> url_info = parse_url(resource_url);
    string path = url_info.path ?? "";
    if (url_info.query) {
        path += "?" + url_info.query;
    }
    return doGet(path);
}
```

**Reference implementation:** `ShipStationBaseEventDataProvider.qc` — receives only a `resource_url` in the webhook payload, fetches the full order/shipment via REST, then notifies observers with the enriched data.

---

## Dynamic Options

For fields that depend on a previous selection (e.g., form fields based on selected form).

### Action Registration

```qore
"data_dependent_options": True,
"options": {
    "tableID": <ActionOptionInfo>{
        "structural_determinate": True,
        "on_change": ("refetch",),
        "ref_data": "tables",
    },
},
```

### Data Provider Implementation

Implement **both** methods:

| Method | When Called | Purpose |
|--------|-------------|---------|
| `getRequestTypeWithOptionsImpl()` | UI requests options | Fields shown in UI |
| `getRequestTypeWithDataImpl()` | Request execution | Validates request data |

```qore
private *AbstractDataProviderType getRequestTypeWithOptionsImpl(*hash<auto> options) {
    *string table_id = options.tableID.value ?? options.tableID;
    if (!table_id) {
        return RequestType;
    }
    return buildDynamicType(table_id);
}

private *AbstractDataProviderType getRequestTypeWithDataImpl(auto req) {
    if (req.typeCode() != NT_HASH) {
        return RequestType;
    }
    *string table_id = req.tableID.value ?? req.tableID;
    if (!table_id) {
        return RequestType;
    }
    return buildDynamicType(table_id);
}
```

---

## Table Data Providers (Record-Based CRUD)

Table data providers expose a collection/table as a record-based interface with support for search
(DPAT_FIND), create, upsert, update, delete, and bulk operations. The key class is
`AbstractDataProvider` with its `*Impl` methods.

### Provider Info Declaration

Declare capabilities in `ProviderInfo`:

```qore
const ProviderInfo = <DataProviderInfo>{
    "desc": "My collection table data provider",
    "type": "MyCollectionTableDataProvider",
    "has_record": True,
    "supports_read": True,
    "supports_native_search": True,
    "supports_create": True,
    "supports_upsert": True,
    "supports_update": True,
    "supports_delete": True,
    "supports_bulk_create": True,
    "supports_bulk_upsert": True,
    "search_options": GenericRecordSearchOptions{"columns", "limit", "offset", "requires_result"} + {
        "query": <DataProviderOptionInfo>{...},
        "filter": <DataProviderOptionInfo>{...},
    },
    "create_options": WriteOptions,
    "upsert_options": WriteOptions,
};
```

### Record Type

Define the record type as a `HashDataType` subclass:

```qore
public class MyScoredPointRecordType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "id": {"type": AbstractDataProviderTypeMap."auto", ...},
            "score": {"type": SoftFloatOrNothingType, ...},
            "payload": {"type": AutoHashOrNothingType, ...},
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}
```

### Key Implementation Methods

| Method | Purpose |
|--------|---------|
| `searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options)` | Returns `AbstractDataProviderRecordIterator` |
| `createRecordImpl(hash<auto> rec, *hash<auto> create_options)` | Creates a record, returns it |
| `upsertRecordImpl(hash<auto> rec, *hash<auto> upsert_options)` | Upserts, returns `UpsertResultInserted` or `UpsertResultUpdated` |
| `updateRecordsImpl(hash<auto> set, *hash<auto> where_cond, *hash<auto> search_options)` | Returns count updated |
| `deleteRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options)` | Returns count deleted |
| `getRecordTypeImpl(*hash<auto> search_options)` | Returns `*hash<string, AbstractDataField>` — call `.getFields()` on the data type instance |
| `getStaticInfoImpl()` | Returns `ProviderInfo` |

### Record Iterator Pattern

The iterator wraps backend query results:

```qore
public class MySearchRecordIterator inherits DataProvider::AbstractDataProviderRecordIterator {
    private:internal {
        const RecordType = new MyScoredPointRecordType();
        Qore::ListHashIterator i;
    }

    constructor(RestClient rest, string collection, *hash<auto> where_cond,
            *hash<auto> search_options)
            : AbstractDataProviderRecordIterator(search_options.requires_result) {
        hash<auto> body = buildRequestBody(search_options);
        // ... process where_cond, merge with body ...
        *list<auto> results = rest.post(uri, body).body.result.items;
        i = new ListHashIterator(results);
    }

    bool valid() { return i.valid(); }
    hash<auto> getValue() { return i.getValue(); }
    *hash<string, DataProvider::AbstractDataField> getRecordType() { return RecordType.getFields(); }
    private bool nextImpl() { return i.next(); }
}
```

### Bulk Operations

Implement `getBulkInserter()` and `getBulkUpserter()` returning
`AbstractDataProviderBulkOperation` subclasses. The bulk operation collects records via
`queueData()` and sends them in batches via `flushImpl()`.

### Tables Container Pattern

A parent "tables" provider lists available tables/collections as child providers:

```qore
public class MyTablesDataProvider inherits MyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "supports_children": True,
            "children_can_support_records": True,
        };
    }

    private *list<string> getChildProviderNamesImpl() {
        return map $1.name, rest.get("/collections").body.result.collections;
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        // Verify collection exists before returning provider
        if (!collectionExists(name)) { return; }
        return new MyCollectionTableDataProvider(rest, name);
    }
}
```

---

## Server-Side Search Expressions

Search expressions enable the standard DataProvider expression interface (`where_cond` with
expression trees) for backend-specific query languages. This is a critical feature for integrating
with the DPQL query language and UI-driven search builders.

### Architecture

The pattern has three components:

1. **Expression definitions** (`*Defs.qc`): A constant mapping DP operator names to expression
   metadata (`exp`) and implementation closures (`impl`)
2. **Provider info**: Declares supported expressions and `supports_search_expressions: True`
3. **Iterator**: `processExpressionArg()` method that walks the expression tree and calls `impl`
   closures to build the backend-specific query

### How the Framework Processes where_cond

The `AbstractDataProvider` base class has two overloads for each search/update/delete method:

- `*hash<DataProviderExpression> where_cond` — for pre-built expression trees; calls
  `processSearchParameters()` to validate, then passes through to `*Impl()`
- `*hash<auto> where_cond` — for plain hashes; calls `getSearchExpression()` which:
  - If `supports_search_expressions` is True: converts plain hash to expression tree (each
    `key: value` pair becomes a `DP_SEARCH_OP_EQ` expression, multiple pairs wrapped in `DP_OP_AND`)
  - Otherwise: calls `processFieldValues()` for simple field validation

**Important**: When `supports_search_expressions` is True, ALL `*Impl()` methods receive
`DataProviderExpression` where_cond, even for update and delete. Your implementation must handle
this (see "Update/Delete with Expressions" below).

### Expression Definitions File

Create a `*Defs.qc` file with the expression mapping. Each entry has:
- `"exp"`: The expression info from `AbstractDataProvider::GenericExpressions{DP_OP_*}`
- `"impl"`: A closure that builds the backend-specific query structure

**Example** (Qdrant vector database → JSON filters):

```qore
public const MyExpressions = {
    DP_OP_AND: {
        "exp": AbstractDataProvider::GenericExpressions{DP_OP_AND},
        "impl": hash<auto> sub (MyRecordIterator iter, list<auto> args) {
            return {"must": map iter.processExpressionArg($1), args};
        },
    },
    DP_OP_OR: {
        "exp": AbstractDataProvider::GenericExpressions{DP_OP_OR},
        "impl": hash<auto> sub (MyRecordIterator iter, list<auto> args) {
            return {"should": map iter.processExpressionArg($1), args};
        },
    },
    DP_SEARCH_OP_EQ: {
        "exp": AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_EQ},
        "impl": hash<auto> sub (string field, auto value) {
            return {"key": field, "match": {"value": value}};
        },
    },
    // ... other operators ...
};
```

**Example** (Salesforce → SOQL strings):

```qore
public const SoqlExpressions = {
    DP_OP_AND: {
        "exp": AbstractDataProvider::GenericExpressions{DP_OP_AND},
        "impl": string sub (object iter, list<auto> args) {
            list<string> clauses = map iter.processExpressionArg($1), args;
            return "(" + (foldl $1 + " and " + $2, clauses) + ")";
        },
    },
    DP_SEARCH_OP_EQ: {
        "exp": AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_EQ},
        "impl": string sub (object iter, string field, auto value) {
            return sprintf("%s = %s", field, iter.getArgValue(field, value));
        },
    },
    // ...
};
```

### Declaring Expressions in ProviderInfo

```qore
const ProviderInfo = <DataProviderInfo>{
    // ... other fields ...
    "expressions": cast<hash<string, hash<DataProviderExpressionInfo>>>(
        map {$1.key: $1.value.exp}, MyExpressions.pairIterator()
    ),
    "supports_search_expressions": True,
};
```

### processExpressionArg() Method

The record iterator needs a public `processExpressionArg()` method that logical operator `impl`
closures can call recursively:

```qore
hash<auto> processExpressionArg(hash<DataProviderExpression> exp) {
    *hash<auto> expinfo = MyExpressions{exp.exp};
    if (!expinfo) {
        throw "WHERE-ERROR", sprintf("unknown operator %y; known: %y", exp.exp, keys MyExpressions);
    }

    # Logical operators (AND, OR, NOT): pass all args to impl for recursive processing
    if (exp.exp == DP_OP_AND || exp.exp == DP_OP_OR || exp.exp == DP_SEARCH_OP_NOT) {
        return call_function_args(expinfo.impl, (self, exp.args));
    }

    # Comparison operators: first arg is field reference, rest are values
    string field = cast<hash<DataProviderFieldReference>>(exp.args[0]).field;

    # IN operator: collect all value args into a list
    if (exp.exp == DP_SEARCH_OP_IN) {
        list<auto> values;
        for (int j = 1; j < exp.args.size(); ++j) {
            if (exp.args[j].typeCode() == NT_LIST) {
                values += exp.args[j];
            } else {
                push values, exp.args[j];
            }
        }
        return call_function_args(expinfo.impl, (field, values));
    }

    # BETWEEN: field + lo + hi
    if (exp.exp == DP_SEARCH_OP_BETWEEN) {
        return call_function_args(expinfo.impl, (field, exp.args[1], exp.args[2]));
    }

    # Standard comparison: field + value
    return call_function_args(expinfo.impl, (field, exp.args[1]));
}
```

### Processing where_cond in the Iterator

The iterator constructor processes `where_cond` to build the backend filter:

```qore
constructor(RestClient rest, string collection, *hash<auto> where_cond,
        *hash<auto> search_options) {
    // ... build body from search_options ...

    if (where_cond) {
        *hash<auto> where_filter = processWhereCondition(where_cond);
        if (where_filter) {
            where_filter = ensureValidFilter(where_filter);
            if (body.filter) {
                // Merge with search_options filter using AND
                body.filter = {"must": (where_filter, body.filter)};
            } else {
                body.filter = where_filter;
            }
        }
    }
}

private *hash<auto> processWhereCondition(hash<auto> where_cond) {
    if (where_cond.exp) {
        return processExpressionArg(cast<hash<DataProviderExpression>>(where_cond));
    }
    // Plain hash: convert each key: value to equality conditions
    list<auto> conditions;
    foreach hash<auto> pair in (where_cond.pairIterator()) {
        push conditions, {"key": pair.key, "match": {"value": pair.value}};
    }
    if (conditions.size() == 1) { return conditions[0]; }
    return {"must": conditions};
}
```

### Update/Delete with Expressions

When `supports_search_expressions` is True, the framework converts ALL where_cond hashes to
`DataProviderExpression` objects — including for update and delete operations. If your update/delete
methods extract fields from where_cond (e.g., `where_cond.id`), you must handle the
`DataProviderExpression` case:

```qore
private list<auto> getPointIds(*hash<auto> where_cond, string operation) {
    if (!where_cond) {
        throw "ERROR", "missing 'id' in where_cond";
    }

    # Handle DataProviderExpression (from framework expression conversion)
    if (where_cond instanceof hash<DataProviderExpression>) {
        return extractIdsFromExpression(
            cast<hash<DataProviderExpression>>(where_cond), operation);
    }

    # Plain hash fallback
    return where_cond.id.typeCode() == NT_LIST ? where_cond.id : (where_cond.id,);
}

private list<auto> extractIdsFromExpression(hash<DataProviderExpression> exp, string op) {
    switch (exp.exp) {
        case DP_SEARCH_OP_EQ: {
            if (exp.args[0] instanceof hash<DataProviderFieldReference>
                    && cast<hash<DataProviderFieldReference>>(exp.args[0]).field == "id") {
                auto val = exp.args[1];
                return val.typeCode() == NT_LIST ? val : (val,);
            }
            break;
        }
        case DP_OP_AND: {
            foreach auto arg in (exp.args) {
                if (arg instanceof hash<DataProviderExpression>) {
                    try { return extractIdsFromExpression(cast<hash<DataProviderExpression>>(arg), op); }
                    catch () {}
                }
            }
            break;
        }
    }
    throw "ERROR", sprintf("where_cond must reference 'id' field for %s", op);
}
```

### Dynamic Payload Fields

If your backend has dynamic/schemaless fields (e.g., Qdrant payloads, MongoDB documents), override
`searchAcceptsForeignField()` to allow field names not in the record type:

```qore
bool searchAcceptsForeignField(string field) {
    return True;
}
```

Without this, the framework rejects plain hash where_cond keys that aren't in the declared record
type.

### Supported Operators

The standard DataProvider operators available in `AbstractDataProvider::GenericExpressions`:

| Operator | Constant | Expression Args | Description |
|----------|----------|-----------------|-------------|
| AND | `DP_OP_AND` | `(expr, expr, ...)` | Logical AND of sub-expressions |
| OR | `DP_OP_OR` | `(expr, expr, ...)` | Logical OR of sub-expressions |
| NOT | `DP_SEARCH_OP_NOT` | `(expr,)` | Logical negation |
| EQ | `DP_SEARCH_OP_EQ` | `(field_ref, value)` | Equality |
| NE | `DP_SEARCH_OP_NE` | `(field_ref, value)` | Not equal |
| LT | `DP_SEARCH_OP_LT` | `(field_ref, value)` | Less than |
| LE | `DP_SEARCH_OP_LE` | `(field_ref, value)` | Less than or equal |
| GT | `DP_SEARCH_OP_GT` | `(field_ref, value)` | Greater than |
| GE | `DP_SEARCH_OP_GE` | `(field_ref, value)` | Greater than or equal |
| BETWEEN | `DP_SEARCH_OP_BETWEEN` | `(field_ref, lo, hi)` | Range (exclusive) |
| IN | `DP_SEARCH_OP_IN` | `(field_ref, val, ...)` | Value in list |
| REGEX | `DP_SEARCH_OP_REGEX` | `(field_ref, pattern)` | Regex match |

### Constructing Expression Trees in Tests

```qore
# Simple equality
hash<DataProviderExpression> eq_expr = <DataProviderExpression>{
    "exp": DP_SEARCH_OP_EQ,
    "args": (
        <DataProviderFieldReference>{"field": "city"},
        "Berlin",
    ),
};

# Compound: AND(EQ, GT)
hash<DataProviderExpression> and_expr = <DataProviderExpression>{
    "exp": DP_OP_AND,
    "args": (
        <DataProviderExpression>{
            "exp": DP_SEARCH_OP_EQ,
            "args": (<DataProviderFieldReference>{"field": "country"}, "Germany"),
        },
        <DataProviderExpression>{
            "exp": DP_SEARCH_OP_GT,
            "args": (<DataProviderFieldReference>{"field": "population"}, 1000000),
        },
    ),
};

# IN with multiple values
hash<DataProviderExpression> in_expr = <DataProviderExpression>{
    "exp": DP_SEARCH_OP_IN,
    "args": (
        <DataProviderFieldReference>{"field": "city"},
        ("Berlin", "Paris", "Moscow"),
    ),
};

# Plain hash where_cond (implicit EQ, framework converts to expressions)
AbstractDataProviderRecordIterator iter = prov.searchRecords({"city": "Berlin"}, {"limit": 10});
```

### Reference Implementations

- **Qdrant** (JSON filters): `qlib/QdrantDataProvider/QdrantDataProviderDefs.qc`,
  `QdrantSearchRecordIterator.qc`, `QdrantCollectionTableDataProvider.qc`
- **Salesforce** (SOQL strings): `qlib/SalesforceRestDataProvider/SalesforceRestDataProviderDefs.qc`,
  `SalesforceRestRecordIterator.qc`, `SalesforceRestObjectDataProvider.qc`

---

## Common Pitfalls

### 1. Using `token` instead of `apikey`
Inherited `token` option has special handling. Use custom name like `apikey`.

### 2. Required options in event provider constructors
Event providers created via path navigation without options. Use optional types (`*string`), get values from context in `observersReady()`.

### 3. Wrong path for DPAT_FIND_SINGLE
Point to collection provider (`/items`), not single item provider (`/items/get`).

### 4. Missing `auto_url` URL construction
For `auto_url: True`, must implement `getConfig()` to build URL from options.

### 5. Missing reference data methods
For `ref_data` in action options, implement `getSupportedReferenceData()` and `getReferenceDataImpl()`.

### 6. Hardcoded OAuth2 URLs
For regional services (Zoho, Salesforce), build OAuth2 URLs dynamically from domain option. Hardcoded URLs break for non-US users.

### 7. Leading slash in ping path
If base URL has path component, ping path must be relative (no leading slash) or it bypasses the base path.

### 8. JSON vs Form-Encoded
Some APIs (JotForm) expect form-encoded data, not JSON. API may return 200 OK but silently ignore JSON body. Always verify data format per endpoint.

```qore
// For form-encoded APIs
rest.setSerialization("text");
on_exit rest.setSerialization(old);
rest.post(path, encoded_body, NOTHING, {"Content-Type": "application/x-www-form-urlencoded"});
```

### 9. Missing FactoryMap entry
Module works when loaded directly but doesn't appear in Qorus apps. Add to `DataProvider.qc` -> `FactoryMap`.

### 10. Allowed values in option descriptions
Never describe allowed values in the `desc` or `short_desc` text. Always declare them explicitly using the `allowed_values` field in `DataProviderOptionInfo`, `ConnectionOptionInfo`, or `ActionOptionInfo`. Text descriptions of allowed values cannot be parsed by the UI for dropdown generation.

### 11. Missing `options` or `output_type` in action registration
Every `registerAction()` call **must** include `options` (generated via `getActionOptionFromFields()`) and `output_type`. Without `options`, the action shows an empty form and users cannot configure it. Without `output_type`, output fields are invisible. This is the primary user interface — actions without these fields are unusable.

### 12. Field ordering in request types
Fields with `required_groups` must be declared first in the `Fields` constant, followed by required fields, then optional fields. This ensures the UI presents primary input options at the top of the form. When `required_groups` fields are scattered among optional fields, users may not see the main input choices without scrolling.

### 12a. `required_groups` must be inside `attr` for nested type fields
When defining `required_groups` on fields inside a `HashDataType` subclass (used via `addQoreFields()`), the `required_groups` key **must** be inside the `attr` hash. `QoreDataField` only reads attributes from `h.attr` — top-level keys like `"required_groups"` on the field hash are silently ignored.

**Wrong** — `required_groups` at top level of field hash (ignored by `QoreDataField`):
```qore
"content": {
    "display_name": "Content",
    "type": AbstractDataProviderTypeMap."*string",
    "required_groups": ("message_content",),  # WRONG: silently ignored
},
```

**Correct** — `required_groups` inside `attr`:
```qore
"content": {
    "display_name": "Content",
    "type": AbstractDataProviderTypeMap."*string",
    "attr": {
        "required_groups": ("message_content",),
    },
},
```

Note: For top-level action option fields processed by `getActionOptionFromFields()`, either location works because that method explicitly reads `required_groups` from the field hash. But for fields in nested types (e.g., list element types), only the `attr` approach works.

### 13. Single-key hash slicing
`Fields{"key"}` or `getFields(){"key"}` returns the value at that key (single-value dereference). For a single-key hash slice that returns a hash, use a trailing comma: `Fields{"key",}`. This is critical when passing fields to `getActionOptionFromFields()`, which expects a hash of field definitions — passing a single field's value instead of a hash causes `OPTION-ERROR` at module load time.

### 14. Type classes outside namespace block
Type classes declared in a `.qc` file **must** be inside the `public namespace ModuleName { ... }` block. If declared outside it (even in the same file), their class constants (e.g., `Fields`) cannot be resolved via `ClassName::ConstantName` from the `.qm` file, causing `PARSE-EXCEPTION: cannot resolve bareword` errors at module load time. See [Request Type Fields Pattern](#request-type-fields-pattern).

### 15. Description formatting issues
`desc` fields are rendered as markdown in the UI. Common mistakes that degrade readability:
- **Bare `True`/`False`/`NOTHING`**: Always wrap in backticks — `` `True` ``, `` `False` ``, `` `NOTHING` ``
- **Bare field/option names**: Use backticks for cross-references — `` `ssl_key_path` ``, `` `header_names` ``
- **Single quotes for code values**: Use backticks not single quotes — `` `xlsx` `` not `'xlsx'`
- **Lowercase sentence starts**: Every sentence in `desc` must start with a capital letter
- **Wall-of-text long descriptions**: Descriptions >500 chars should use bold section headers (`**Section**:`) and bullet lists (`- `item``) — see [Description Formatting](#description-formatting)
- **Unclosed backticks**: Always verify backtick pairs are matched; a missing opening or closing backtick breaks markdown rendering for the rest of the description

### 15. Wrong `getRecordTypeImpl()` return type

Collection data providers (those supporting `DPAT_FIND`) must override `getRecordTypeImpl()` with the correct signature and return type. The base class declares:

```qore
private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
    throwUnimplementedException();
}
```

**Wrong** — returns the data type object, not its fields:
```qore
private *AbstractDataProviderType getRecordTypeImpl() {
    return new MyRecordDataType();  # WRONG: causes RUNTIME-TYPE-ERROR
}
```

**Correct** — returns the fields hash with matching signature:
```qore
private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
    return new MyRecordDataType().getFields();  # Correct
}
```

Returning `new XxxDataType()` directly creates a no-arg overload that does not properly override the base method. When the action catalog calls `getRecordType()` to acquire the output type description for a `DPAT_FIND` action, the base class tries to return the result of `getRecordTypeImpl()` as `*hash<string, AbstractDataField>`, causing:

```
RUNTIME-TYPE-ERROR: <return statement> expects type '*hash<string, object<AbstractDataField>>',
but got an object of class 'MyRecordDataType' instead
```

The fix is to always call `.getFields()` on the data type instance and use the correct method signature.
