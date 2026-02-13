# Data Provider Implementation Checklist

Comprehensive checklist combining structural verification and quality validation for data provider modules. Run this checklist before marking any data provider work complete.

See also: [data-provider-development-guide.md](data-provider-development-guide.md) for patterns and examples.

---

## 1. REST Client Module

### Connection Setup
- [ ] Scheme registered in `qlib/ConnectionProvider/ConnectionSchemeCache.qc` -> `SchemeMap`
- [ ] `auto_url: True` if URL is built from options
- [ ] `getConfig()` builds URL from domain/region options (not hardcoded)
- [ ] Ping path configured correctly (no leading slash if base URL has path)
- [ ] API version in default URL (e.g., `/v3`, `/books/v3`)

### Connection Options
- [ ] Uses `apikey` instead of inherited `token` (avoids special handling issues)
- [ ] Required options declared via `required_options` string (not `required: True` on individual options)
- [ ] Options with finite allowed values use `allowed_values` field explicitly (never describe allowed values as text in `desc` or `short_desc`)

### OAuth2 (if applicable)
- [ ] `DefaultAuthArgs` has `"access_type": "offline"` and `"prompt": "consent"` for refresh tokens
- [ ] `oauth2_auth_args` in `DefaultOptions` and `ConnectionScheme`
- [ ] `getConnectionOptions(*hash<auto> rtopts)` signature correct (accepts runtime options)
- [ ] `getConnectionOptions()` passes `rtopts` to parent
- [ ] `getConnectionOptions()` sets `oauth2_auth_args` if not present
- [ ] OAuth2 auth/token URLs built dynamically from domain (not hardcoded)
- [ ] `getConnectionOptions()` **always** rebuilds OAuth2 URLs (not just when unset)
- [ ] `setUpdateOptionsCode()` auto-corrects URLs on connection load
- [ ] `setUpdateOptionsCode()` fixes ping paths with leading slashes
- [ ] Tested with non-default region (e.g., `.eu`)

---

## 2. Data Provider Module

### Registration
- [ ] Factory registered in `qlib/DataProvider/DataProvider.qc` -> `FactoryMap`
- [ ] `registerApp()` called with correct fields
- [ ] `groups` uses `AppGroup` enum values (not strings)

### App Info
- [ ] `display_name` is user-friendly ("Zoho Books" not "zohobooks")
- [ ] `short_desc` under 80 chars
- [ ] `desc` explains what the service is
- [ ] Logo provided with correct MIME type

---

## 3. Action Registration

### All Actions
- [ ] Each action has `display_name`, `short_desc`, `desc`
- [ ] `groups` organizes actions logically
- [ ] Each option has `display_name` and `short_desc`
- [ ] Complex options have `desc` explaining format/structure
- [ ] Option `allowed_values` declared explicitly in the option definition (never as text in `desc` or `short_desc`)

### DPAT_FIND / DPAT_FIND_SINGLE Actions (CRITICAL)
- [ ] **Every** action option exists in the data provider's `SearchOptions`
- [ ] `ProviderInfo` includes `"search_options": SearchOptions`
- [ ] `searchRecordsImpl()` handles all `SearchOptions`
- [ ] DPAT_FIND_SINGLE points to collection provider (e.g., `/items`, not `/items/get`)

### DPAT_API Actions
- [ ] Provider has `"supports_request": True` in `ProviderInfo`
- [ ] Provider implements `doRequestImpl()`
- [ ] `getRequestTypeWithDataImpl()` validates dynamic fields (if applicable)
- [ ] ISO timestamp fields use `DateType`, not `SoftStringType`
- [ ] Required fields match API documentation

### DPAT_EVENT Actions
- [ ] Provider has `"supports_observable": True` in `ProviderInfo`
- [ ] `action_val` matches an event key in `getEventTypesImpl()`

---

## 4. Action Option Sufficiency

The goal: actions should expose enough API functionality to be genuinely useful, not just the bare minimum. For each action, ask: *"If I were using this action, would I be frustrated that I can't set X?"*

### Create Actions
- [ ] Common optional fields exposed (notes, description, dates, references, custom fields)
- [ ] Coverage comparable to similar providers (if Salesforce has 10 fields, why do we have 3?)

### Update Actions
- [ ] Commonly-changed fields are updatable
- [ ] Useful API-supported fields are not hidden

### List/Find Actions
- [ ] API-supported filters exposed (date ranges, status, related entity filters)
- [ ] Users can meaningfully narrow down results

---

## 5. Option Allowed Values

**Every option with a finite set of allowed values MUST declare them explicitly using `allowed_values`** - this enables dropdown generation in the UI. Describing allowed values in text descriptions provides a poor UX because the UI cannot parse free-text descriptions into dropdown options.

- [ ] Data provider options with enumerated values use `allowed_values` field
- [ ] Connection options with enumerated values use `allowed_values` field
- [ ] Action options with enumerated values use `allowed_values` or `ref_data` fields
- [ ] No option has allowed values described only in `desc`, `short_desc`, or `display_name` text

### Example (correct)
```qore
"status": <DataProviderOptionInfo>{
    "display_name": "Status",
    "short_desc": "Filter by invoice status",
    "type": AbstractDataProviderTypeMap."string",
    "allowed_values": (
        <AllowedValueInfo>{"value": "draft", "display_name": "Draft"},
        <AllowedValueInfo>{"value": "sent", "display_name": "Sent"},
        <AllowedValueInfo>{"value": "paid", "display_name": "Paid"},
    ),
},
```

### Example (wrong - no dropdown possible)
```qore
"status": <DataProviderOptionInfo>{
    "display_name": "Status",
    "short_desc": "Filter by status (draft, sent, or paid)",  // BAD: values in text
    "type": AbstractDataProviderTypeMap."string",
},
```

---

## 6. ref_data for ID Fields

- [ ] Options ending in `_id`, `Id`, or `ID` have `ref_data` attribute (exception: `organization_id` which comes from connection)
- [ ] `getSupportedReferenceData()` returns all `ref_data` types used in actions
- [ ] `getReferenceDataImpl()` handles each type and returns `list<hash<AllowedValueInfo>>`

---

## 7. Type Safety

- [ ] Structured data uses custom `HashDataType` classes, not generic `hash`
- [ ] Lists specify element types where known
- [ ] Required fields use non-optional types (`StringType` not `*string`)
- [ ] Optional fields use optional types (`*string`, `*int`)
- [ ] Event provider constructor options are optional (values come from context)

---

## 8. Event/Observable Providers

### Implementation
- [ ] `getEventTypesImpl()` returns `hash<string, hash<DataProviderMessageInfo>>`
- [ ] Each event has `"desc"` and `"type"` fields
- [ ] `getExampleEventDataImpl()` is implemented (not just inherited)
- [ ] `observersReady()` is implemented (starts polling/webhook registration)
- [ ] `ProviderInfo` includes `"supports_observable": True`
- [ ] Constructor options are optional types (`*string` not `string`)

### Example Event Data Quality
- [ ] `getExampleEventDataImpl()` tries real API data first with try/catch
- [ ] Falls back to `getFakeExampleData()` on failure
- [ ] Fake data has resource-specific fields (not just `{id: "123", created_time: "..."}`)
- [ ] Example data fields match what's defined in `getEventTypesImpl()` type

### Events Container
- [ ] `ProviderInfo` has `"supports_children": True` and `"children_can_support_observers": True`
- [ ] `getChildProviderNamesImpl()` returns event provider names
- [ ] `getChildProviderImpl()` creates event provider without constructor options

---

## 9. Dynamic Options (if applicable)

- [ ] Action has `"data_dependent_options": True`
- [ ] Structural determinate option has `"structural_determinate": True` and `"on_change": ("refetch",)`
- [ ] `getRequestTypeWithOptionsImpl()` implemented (UI field rendering)
- [ ] `getRequestTypeWithDataImpl()` implemented (request validation)
- [ ] Both methods handle missing/null option values gracefully

---

## 10. API Coverage

For each major resource (identified by having a list action), verify CRUD coverage:

- [ ] Create action
- [ ] Get single action (DPAT_FIND_SINGLE)
- [ ] Update action (if API supports)
- [ ] Delete action (if API supports)
- [ ] List action (DPAT_FIND)
- [ ] Trigger for new items (if useful)

---

## 11. Naming Consistency

- [ ] Action names follow `verb-noun` pattern (create-invoice, list-contacts)
- [ ] Consistent verbs: create, get, update, delete, list, email, search
- [ ] Option names use consistent `_id` suffix (not mixed `_id` and `Id`)
- [ ] Group names use Title Case

---

## 12. Integration Tests

- [ ] Tests exist in `examples/test/qlib/{ModuleName}/`
- [ ] Tests verify data persistence (not just 200 OK)
- [ ] Tests run against real API
- [ ] Tests clean up test data
- [ ] Tests call `getExampleEventData()` for each trigger and verify expected fields
- [ ] `%modern` directive used (not individual parse directives)
- [ ] Test file has executable permission (`chmod +x`)

---

## 13. Build System and Documentation

### Build Registration (MANDATORY - grep for module name in both files)
- [ ] `CMakeLists.txt` has `qore_user_module()` entry for REST client module
- [ ] `CMakeLists.txt` has `qore_user_module()` entry for DataProvider module
- [ ] `Makefile.am` has module in appropriate list (single-file or directory)
- [ ] `Makefile.am` has packaging rules for directory modules

### Documentation
- [ ] Entry in `doxygen/lang/120_modules.dox.tmpl`
- [ ] Release note in `doxygen/lang/900_release_notes.dox.tmpl`
- [ ] Copyright year is current (2026)
