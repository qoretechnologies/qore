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
- [ ] `groups` includes at least one `AppGroup` enum value (enforced at runtime by `RequiredAppKeys`)
- [ ] `groups` uses only `AppGroup` enum values from `qlib/DataProvider/AppGroup.qc` (not raw strings)
- [ ] `groups` assigned appropriately (see [AppGroup Reference](#appgroup-reference) below)

### App Info
- [ ] `display_name` is user-friendly ("Zoho Books" not "zohobooks")
- [ ] `short_desc` under 80 chars, **plain text** (no markdown formatting)
- [ ] `desc` is **markdown-formatted** text explaining what the service is (see [Markdown in Descriptions](#markdown-in-descriptions))
- [ ] Logo provided with correct MIME type

---

## 3. Action Registration

### All Actions
- [ ] Each action has `display_name`, `short_desc`, `desc`
- [ ] `short_desc` is **plain text** (no markdown)
- [ ] `desc` is **markdown-formatted** (see [Markdown in Descriptions](#markdown-in-descriptions))
- [ ] `groups` organizes actions logically
- [ ] Each option has `display_name` and `short_desc`
- [ ] Complex options have `desc` explaining format/structure (**markdown-formatted**)
- [ ] Option `allowed_values` declared explicitly in the option definition (never as text in `desc` or `short_desc`)

### DPAT_FIND / DPAT_FIND_SINGLE Actions (CRITICAL)
- [ ] **Every** action option exists in the data provider's `SearchOptions`
- [ ] `ProviderInfo` includes `"search_options": SearchOptions`
- [ ] `searchRecordsImpl()` handles all `SearchOptions`
- [ ] DPAT_FIND_SINGLE points to collection provider (e.g., `/items`, not `/items/get`)

### DPAT_API Actions
- [ ] Provider has `"supports_request": True` in `ProviderInfo`
- [ ] Provider implements `doRequestImpl()`
- [ ] **Action has `options` populated** via `DataProviderActionCatalog::getActionOptionFromFields()` (without this, the action shows no form fields and is unusable)
- [ ] **Action has `output_type` set** to the response type (e.g., `MyDataProvider::ResponseType`)
- [ ] Required/core fields passed with `{"preselected": True, "required": True}`; remaining fields added separately as optional
- [ ] Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns the value, not a hash — causes `OPTION-ERROR` at module load)
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

## 5. Action Options and Output Types (CRITICAL)

**Action `options` and `output_type` are the primary user interface** — they determine what users see in the action form and what output fields are available for mapping. An action without `options` appears in the catalog but shows an empty, unusable form. An action without `output_type` gives users no visibility into the response.

### options
- [ ] **Every action has `options` populated** — no exceptions
- [ ] Options generated via `DataProviderActionCatalog::getActionOptionFromFields()` from request type fields (keeps options in sync with request types)
- [ ] Preferred: use `ClassName::Fields{"key1", "key2"}` (class constant) instead of `instance.getFields(){"key1", "key2"}`
- [ ] Core/required fields passed with `{"preselected": True, "required": True}`
- [ ] Remaining fields added as optional (no extra flags)
- [ ] Single-key hash slices use trailing comma: `Fields{"key",}` (without trailing comma, `Fields{"key"}` returns a value, not a hash)

### output_type
- [ ] **Every action has `output_type` set** — preferably via `DataProviderClassName::ResponseType` (class-level static member)
- [ ] Response type accurately reflects all fields returned by `doRequestImpl()`

### example_value on Request Type Fields
- [ ] Key numeric fields have `example_value` (e.g., width: `612.0`, height: `792.0` for document dimensions)
- [ ] Key text fields have `example_value` (e.g., text: `"Hello, World!"`)
- [ ] Example values are realistic and representative of typical usage

---

## 6. Option Allowed Values

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

## 7. ref_data for ID Fields

- [ ] Options ending in `_id`, `Id`, or `ID` have `ref_data` attribute (exception: `organization_id` which comes from connection)
- [ ] `getSupportedReferenceData()` returns all `ref_data` types used in actions
- [ ] `getReferenceDataImpl()` handles each type and returns `list<hash<AllowedValueInfo>>`

---

## 8. Type Safety

- [ ] Structured data uses custom `HashDataType` classes, not generic `hash`
- [ ] Lists specify element types where known
- [ ] Required fields use non-optional types (`StringType` not `*string`)
- [ ] Optional fields use optional types (`*string`, `*int`)
- [ ] Event provider constructor options are optional (values come from context)
- [ ] Request type classes declare `const Fields` in a `public {}` block (enables `ClassName::Fields` in action registration)
- [ ] Type classes are declared **inside** the `public namespace ModuleName { ... }` block (not outside it — class constants are unresolvable from `.qm` if outside)
- [ ] Data provider classes declare `static ... ResponseType()` and `static ... RequestType()` in `public {}` blocks

---

## 9. Event/Observable Providers

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

## 10. Dynamic Options (if applicable)

- [ ] Action has `"data_dependent_options": True`
- [ ] Structural determinate option has `"structural_determinate": True` and `"on_change": ("refetch",)`
- [ ] `getRequestTypeWithOptionsImpl()` implemented (UI field rendering)
- [ ] `getRequestTypeWithDataImpl()` implemented (request validation)
- [ ] Both methods handle missing/null option values gracefully

---

## 11. API Coverage

For each major resource (identified by having a list action), verify CRUD coverage:

- [ ] Create action
- [ ] Get single action (DPAT_FIND_SINGLE)
- [ ] Update action (if API supports)
- [ ] Delete action (if API supports)
- [ ] List action (DPAT_FIND)
- [ ] Trigger for new items (if useful)

---

## 12. Option Preselection

The UI uses `preselected: True` on action options to determine which fields to show upfront in the form. Without preselection, actions with no required options show an empty form, and users must manually discover available options.

### Rules
- [ ] Actions with **no required options** have at least 2-3 commonly-used options marked `"preselected": True`
- [ ] Actions with required options have those required options marked `"preselected": True`
- [ ] Key optional options that are commonly used are also preselected (e.g., `status`, `limit`, `name`)
- [ ] Metadata, advanced, and system fields are NOT preselected (`meta_data`, `resource_version`, `channel`)

### What to Preselect
- All required options
- ID/lookup fields (`id`, `customer_id`)
- Common filter fields (`status`, `limit`, date ranges)
- Fields that define the core purpose of the action (`amount`, `name`, `email`)

### Example (correct)
```qore
"id": <ActionOptionInfo>{
    "display_name": "Invoice ID",
    "type": AbstractDataProviderTypeMap."string",
    "required": True,
    "preselected": True,   # Shown upfront
},
"status": <ActionOptionInfo>{
    "display_name": "Status",
    "type": AbstractDataProviderTypeMap."string",
    "preselected": True,   # Common filter, shown upfront
},
"meta_data": <ActionOptionInfo>{
    "display_name": "Metadata",
    "type": AbstractDataProviderTypeMap."hash",
    # No preselected - advanced option, hidden by default
},
```

---

## 13. Naming Consistency

- [ ] Action names follow `verb-noun` pattern (create-invoice, list-contacts)
- [ ] Consistent verbs: create, get, update, delete, list, email, search
- [ ] Option names use consistent `_id` suffix (not mixed `_id` and `Id`)
- [ ] Group names use Title Case

---

## 14. Integration Tests

- [ ] Tests exist in `examples/test/qlib/{ModuleName}/`
- [ ] Tests verify data persistence (not just 200 OK)
- [ ] Tests run against real API
- [ ] Tests clean up test data
- [ ] Tests call `getExampleEventData()` for each trigger and verify expected fields
- [ ] `%modern` directive used (not individual parse directives)
- [ ] Test file has executable permission (`chmod +x`)

---

## 15. Build System and Documentation

### Build Registration (MANDATORY - grep for module name in both files)
- [ ] `CMakeLists.txt` has `qore_user_module()` entry for REST client module
- [ ] `CMakeLists.txt` has `qore_user_module()` entry for DataProvider module
- [ ] `Makefile.am` has module in appropriate list (single-file or directory)
- [ ] `Makefile.am` has packaging rules for directory modules

### Documentation
- [ ] Entry in `doxygen/lang/120_modules.dox.tmpl`
- [ ] Release note in `doxygen/lang/900_release_notes.dox.tmpl`
- [ ] Copyright year is current (2026)

---

## 16. Markdown in Descriptions

All `desc` fields throughout the data provider framework are rendered as **markdown** in the UI. The `short_desc` field is **plain text** (no markdown). This distinction is critical for UX — long descriptions become much more readable with proper formatting.

### Rules
- [ ] `short_desc` is plain text, under 80 chars, single sentence
- [ ] `desc` uses markdown formatting for readability
- [ ] Code references use backticks: `` `field_name` ``, `` `True` ``, `` `NOTHING` ``
- [ ] Multiple options/alternatives use bullet lists, not inline prose
- [ ] Important caveats use **bold** for emphasis
- [ ] URLs use markdown links: `[display text](url)`
- [ ] Multi-sentence descriptions use line breaks for paragraph separation
- [ ] Enumerations of values that aren't in `allowed_values` use bullet lists or backtick-separated lists
- [ ] Sentences in `desc` start with a capital letter
- [ ] No typos (common: "maxium" → "maximum", "ysed" → "used")
- [ ] No double spaces in descriptions
- [ ] All backtick pairs are matched (no unclosed/missing opening backticks)

### Common Anti-Patterns to Check
- [ ] **Bare `True`/`False`/`NOTHING`/`nothing`/`null`**: Must be backtick-wrapped in `desc` (e.g., `` `True` ``, `` `False` ``, `` `NOTHING` ``)
- [ ] **Bare field/option names**: References to other fields or options must use backticks (e.g., `` `ssl_key_location` ``, `` `header_names` ``)
- [ ] **Bare numeric values in context**: Meaningful numeric values should use backticks (e.g., `` `0` ``–`` `255` ``, `` `-100` ``–`` `100` ``)
- [ ] **Single-quoted values instead of backticks**: Use `` `xlsx` `` not `'xlsx'` for code values in markdown
- [ ] **Lowercase sentence starts**: Every sentence in `desc` must start with a capital letter (e.g., "If `true`..." not "if `true`...")
- [ ] **URLs/schemes without backticks**: Protocol schemes like `` `mcp://` ``, `` `http://` `` should be backtick-wrapped
- [ ] **Class/type names without backticks**: Class names, type names, and API values referenced in prose should be backtick-wrapped

### Long Description Formatting (>500 chars)

Descriptions longer than ~500 characters (typically action options describing complex structures like draw commands, CSV field formats, or multi-feature configurations) need structured formatting beyond simple backtick additions:

- [ ] **Organized with bold section headers**: Group related items under `**Section Name**:` headers
- [ ] **Bullet lists for enumerations**: Each command, option, or feature gets its own `- ` bullet
- [ ] **Opening summary sentence**: Start with a concise 1-sentence summary before the detailed breakdown
- [ ] **Consistent formatting within bullets**: Each bullet uses backticks for the command/option name, followed by a brief description

#### Example (wall of text — poor UX)
```qore
"desc": "List of draw command hashes with an op key. Supported operations: set_source_rgb "
    "(r, g, b), set_source_rgba (r, g, b, a), move_to (x, y), line_to (x, y), "
    "rectangle (x, y, width, height), arc (xc, yc, radius, angle1, angle2), "
    "curve_to (x1, y1, x2, y2, x3, y3), stroke, fill, set_font_size (size), "
    "show_text (text, x, y), translate (tx, ty), scale (sx, sy), rotate (angle), "
    "create_linear_gradient, create_radial_gradient, save, restore",
```

#### Example (structured markdown — good UX)
```qore
"desc": "List of draw command hashes. Each command has an `op` key specifying the "
    "operation and operation-specific parameters.\n\n"
    "**Color and Style**:\n"
    "- `set_source_rgb` (r, g, b) — color values 0.0–1.0\n"
    "- `set_source_rgba` (r, g, b, a) — with alpha transparency\n\n"
    "**Paths**:\n"
    "- `move_to` (x, y) — move to point without drawing\n"
    "- `line_to` (x, y) — draw line to point\n"
    "- `rectangle` (x, y, width, height)\n"
    "- `arc` (xc, yc, radius, angle1, angle2)\n"
    "- `curve_to` (x1, y1, x2, y2, x3, y3) — cubic Bézier\n\n"
    "**Drawing**:\n"
    "- `stroke` — draw the current path outline\n"
    "- `fill` — fill the current path\n\n"
    "**Text**:\n"
    "- `set_font_size` (size)\n"
    "- `show_text` (text, x, y)",
```

### Where `desc` Fields Appear
- App registration (`registerApp` → `DataProviderAppInfo`)
- Action registration (`registerAction` → `DataProviderActionInfo`)
- Action options (`ActionOptionInfo`)
- Data provider options (`DataProviderOptionInfo`)
- Request/response type fields (in `Fields` constants)
- Event types (`DataProviderMessageInfo`)
- Connection options (`ConnectionOptionInfo`)
- Provider info (`DataProviderInfo`)

### Example (plain text — poor UX)
```qore
"desc": "The maximum number of records to return or to affect; if more records are returned or "
    "affected, the operation results in an error; for data provider supporting transactions; this "
    "will normally result in a transaction rollback if a transaction is in progress. This option is "
    "normally enforced externally from the server interfaced by the data provider",
```

### Example (markdown — good UX)
```qore
"desc": "The maximum number of records to return or to affect.\n\n"
    "If more records are returned or affected, the operation results in an error. "
    "For data providers supporting transactions, this will normally result in a "
    "**transaction rollback** if a transaction is in progress.\n\n"
    "**Note**: this option is normally enforced externally from the server interfaced "
    "by the data provider (unlike `limit` which is enforced server-side).",
```

### Short Descriptions That Don't Need Markdown
Short `desc` values (1-2 simple sentences) are fine as-is — don't add markdown just for the sake of it. Focus markdown formatting on:
- Descriptions longer than ~120 characters
- Descriptions listing multiple options or alternatives
- Descriptions with code references, field names, or API values
- Descriptions explaining complex behavior with conditions or caveats

---

## AppGroup Reference

Every app registered with `DataProviderActionCatalog::registerApp()` **must** include at least one group from the
`DataProvider::AppGroup` enum (defined in `qlib/DataProvider/AppGroup.qc`). This is enforced at runtime by
`RequiredAppKeys` in `DataProviderActionCatalog`.

Apps may belong to multiple groups where appropriate (e.g., MQTT belongs to both `Messaging` and `Iot`).

Use the fully qualified enum path (`DataProvider::AppGroup::XYZ`) from modules outside the `DataProvider` namespace.
Within the `DataProvider` namespace, the shorter `AppGroup::XYZ` form works.

### Available Groups

| Enum Value | Display Name | Use For |
|---|---|---|
| `AccountingErp` | Accounting & ERP | Zoho Books, FreshBooks, Wave, Cin7, Unleashed, Zoho Invoice, Business Central |
| `AiLlm` | AI & Language Models | OpenAI, MCP, AI/ML tools |
| `Analytics` | Analytics & Reporting | DataProviderML, analytics platforms |
| `ApiIntegration` | API & Integration | REST client, SOAP, MCP, Qorus remote, generic API tools |
| `CloudStorage` | Cloud Storage & File Management | WebDAV, cloud file services |
| `CrmSales` | CRM & Sales Management | Salesforce, Dynamics CRM/CDS |
| `CustomerSupport` | Customer Support & Helpdesk | ServiceNow |
| `Databases` | Databases & Backend Services | DB, ElasticSearch, Redis, Memcached, MongoDB |
| `DataTransformation` | Data Transformation | CSV, FixedLength, EDIFACT, Generator, Tar, Zip |
| `DesignCreative` | Design & Creative Tools | ImageMagick |
| `DevOps` | DevOps & Cloud Infrastructure | Qorus integration engine |
| `DocumentSigning` | Document Signing & Contracts | DocuSign, signing services |
| `Documents` | Documents & Documentation | Word, PDF |
| `Ecommerce` | E-commerce Platforms | Zoho Inventory, Square |
| `Email` | Email & Email Marketing | SMTP, Gmail, Mailgun, POP3 |
| `FileSystem` | File System & Local Storage | File data provider, file poller |
| `FileTransfer` | File Transfer Protocols | FTP, SFTP |
| `FormsSurveys` | Forms, Surveys & Scheduling | Jotform |
| `GoogleWorkspace` | Google Workspace Suite | Gmail, Google Calendar |
| `Hospitality` | Hospitality & Property Management | Mews |
| `Hr` | HR & People Management | HR platforms |
| `Iot` | IoT & Smart Building | Empathic Building, BusyLight, MQTT |
| `Marketing` | Marketing Automation | Marketing platforms |
| `Messaging` | Messaging & Real-time Communication | WebSocket, SSE, Discord, Kafka, MQTT |
| `Notifications` | Notifications & Alerts | Notification services |
| `Payments` | Payment Processing | Square |
| `ProjectManagement` | Project & Task Management | Linear |
| `Shipping` | Shipping & Logistics | Shippo, ShipStation |
| `SocialMedia` | Social Media Management | Social media platforms |
| `Spreadsheets` | Spreadsheets & Data Tables | Excel |
| `VersionControl` | Version Control & Code Repositories | Git platforms |
| `VideoConferencing` | Video Conferencing & Meetings | Video meeting platforms |
| `Weather` | Weather | Weather services |
| `WebAutomation` | Web Scraping & Automation | Web scraping tools |

### Example

```qore
# Single group (from outside DataProvider namespace)
DataProviderActionCatalog::registerApp(<DataProviderAppInfo>{
    ...
    "groups": (DataProvider::AppGroup::Messaging,),
    ...
});

# Multiple groups
DataProviderActionCatalog::registerApp(<DataProviderAppInfo>{
    ...
    "groups": (DataProvider::AppGroup::Messaging, DataProvider::AppGroup::Iot,),
    ...
});
```
