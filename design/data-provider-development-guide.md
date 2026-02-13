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
    "desc": "Full description...",
    "scheme": "myservice",
    "logo": MyServiceLogo,
    "logo_file_name": "myservice-logo.svg",
    "logo_mime_type": MimeTypeSvg,
    "groups": (AppGroup::Finance,),  // Use enum, not strings!
});
```

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

Standard request/response pattern. For dynamic options, implement `getRequestTypeWithDataImpl()`.

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
