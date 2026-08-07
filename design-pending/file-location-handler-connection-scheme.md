# `conn://` — Named-Connection File Locations for `FileLocationHandler`

**Status:** Design proposal. Not implemented.
**Issue:** [#5386](https://github.com/qoretechnologies/qore/issues/5386).
**Branch:** `feature/5386_file_app_improvements`.

**Goal:** let any `FileLocationHandler` location string name a *connection* plus a
*path*, so that every place in the product that already accepts a file location
string (`FileLocationHandler::getTextFileFromLocation()` and friends) can read
and write files through configured, credentialed, monitored connections —
including SaaS file services (S3, Dropbox, Google Drive, OneDrive) that have no
URL-expressible file scheme at all.

**Companions:**
- [`cloud-storage-qlib-modules.md`](cloud-storage-qlib-modules.md) — migrating
  Amazon S3, Google Drive, and Dropbox from the v8/TS app catalog to native qlib
  modules. Co-designed with this proposal; see §11 for combined sequencing
- `qlib/FileLocationHandler/` — the handler framework being extended
- `qlib/ConnectionProvider/` — connection registry and `get_connection()`
- `qlib/DataProvider/DataProviderActionCatalog.qc` — app/action catalog
- `design/data-provider-development-guide.md`

---

## 1. Motivation

`FileLocationHandler` location strings are a pervasive currency in the codebase.
They are consumed by `Util`, `CsvUtil`, `FixedLengthUtil`, `Swagger`, `OpenApi3`,
`RestClient`, `HttpServerUtil`, `EdifactUtil`, `Hl7v2Util`, `DataProviderML`,
`JsonFileDataProvider`, `ConnectionProvider::HttpConnection` (for
`ssl_cert_path` / `ssl_key_path`), and the FTP data providers, among others.

Today the supported schemes are fixed and *self-contained*: `data`, `file`,
`ftp`, `ftps`, `http`, `https`, `rest`, `rests`, `sftp`
(`FileLocationHandler.qc:258`). Every one of them requires the caller to embed
the complete coordinates — host, credentials, TLS material, timeouts — in the
location string or in its `{...}` option suffix.

That has three consequences:

1. **Credentials leak into data.** An `sftp://user:password@host/path` location
   string is stored, logged, and passed around as ordinary configuration text.
2. **No reuse of configured connections.** A site that has already configured,
   pinged, monitored, and OAuth2-authorized a connection cannot point a file
   location at it. The location string must duplicate the configuration.
3. **SaaS file stores are unreachable.** S3, Dropbox, Google Drive, and OneDrive
   are exposed as *data provider apps with actions*, not as URL schemes. There is
   no location string that can name a file in any of them, so none of the
   location-consuming modules above can read or write those files.

A `conn://` scheme fixes all three with one addition: the connection supplies the
credentials and the base location, and the location string supplies only the
path.

---

## 2. What exists today (survey)

### 2.1 `FileLocationHandler`

- `FileLocationHandler::getInfo()` (`FileLocationHandler.qc:481`) splits the
  location into scheme / options / location, looks up the handler in a static
  scheme→handler cache, validates the parsed `{...}` options against the
  handler's declared read or write option set, applies defaults and required
  checks, strips the scheme, and dispatches.
- Handlers implement `AbstractFileLocationHandler` — eight abstract methods:
  text read, binary read, poller, stream reader, binary stream, write, output
  stream, and the two option-info accessors.
- Registration is open: `FileLocationHandler::registerHandler()` /
  `tryRegisterHandler()`, plus dynamic registration from the
  `QORE_FILE_LOCATION_HANDLERS` environment variable
  (`FileLocationHandler.qc:576`).
- The module's only hard dependency is `Util`. External subsystems are pulled in
  lazily: `FileLocationHandlerSftp::getSftpClient()` does
  `load_module("ssh2")` + `create_object("SFTPClient", ...)` and holds the
  result in an `object` variable (`FileLocationHandlerSftp.qc:245-252`). **That
  idiom is the template for this design.**

### 2.2 `ConnectionProvider`

- `get_connection(string conn)` resolves a connection name through the modules
  named in `QORE_CONNECTION_PROVIDERS` and returns an `AbstractConnection`
  (`ConnectionProvider.qm:651`).
- `AbstractConnection` already exposes everything needed:
  `get(bool connect, *hash rtopts)` → the live client object;
  `getUrl()` / `getSafeUrl()`; `getType()`; `getAppName()`;
  `hasDataProvider()` / `getDataProvider(*hash)` /
  `getDataProvider(string subtype, *hash)`; `getDataProviderSubtypes()`;
  `getFeatures()` with `CF_*` feature-name constants;
  `supportsPollingApi()` / `startPollConnect()`.
- Connection classes relevant here: `FilesystemConnection` (schemes `file`,
  `dir`; `getImpl()` returns a `Dir` rooted at `urlh.path`), `FtpConnection`,
  `HttpConnection`, `HttpBasedConnection`, plus `SftpConnection` from the `ssh2`
  module and ~200 REST connection schemes in
  `ConnectionSchemeCache::SchemeMap`.

### 2.3 File-capable data providers and apps

There is already a de-facto file vocabulary in the data provider layer, but it
is **not standardized** — every app spells it differently:

| App | read action | read path option | read result | write action | write path option | write data option |
|---|---|---|---|---|---|---|
| `FileDataProvider` (server FS) | `get` | `path` | `data` field | `create` | `path` | `data` |
| `FtpClientDataProvider` | `get-file` | `path` | `data` field | `send-file` | `path` | `data` |
| `OneDriveDataProvider` | `download-file` | item id | **`download_url` — a URL, not content** | `upload-file` | `parent_id` | `file` (`FileDataType`) |
| Amazon S3 (v8/TS) | `get_file` | `object_key` (+ `bucket_name`) | `content`, base64 | `upload_file` | `object_key` (+ `bucket_name`) | `file` |
| Dropbox (v8/TS) | `download_file` | `path` | `content`, base64 | `upload_file` | `path` | `file` |
| Google Drive (v8/TS) | `get_file` | file id | **metadata only — no content** | `upload_file` | `folder` + `file_name` | `file` |

Two structural facts fall out of that table:

- The **shape** of a file operation is uniform (locate a thing by path/key/id;
  get or put bytes), but the **names** are not. A convention-based scheme
  ("call the action named `get-file`") would work for two of the six and fail
  for the rest.
- Two apps do not return content at all. OneDrive returns a pre-authenticated
  `download_url`; Google Drive's `get_file` returns metadata with a
  `web_content_link`. The contract must model "read yields a URL to follow" as a
  first-class mode, and Google Drive additionally **needs a new
  `download_file` action added to the app** before it can serve reads.

`DataProvider::FileDataType` (`qlib/DataProvider/FileDataType.qc`) is already the
common file payload type — `{name, content (base64), mime_type}` — and is what
`type: 'file'` in the TS toolkit maps to. It is the natural write payload.

`DataProviderActionCatalog::getDataProviderForAction()`
(`DataProviderActionCatalog.qc:1763`) plus
`AbstractDataProvider::getChildProviderPath()`
(`AbstractDataProvider.qc:4558`) and `doRequest()`
(`AbstractDataProvider.qc:3183`) are the generic invocation path for any
registered action.

---

## 3. Proposed location syntax

```
conn://<connection-name>/<path>[{<options>}]
```

- Scheme `conn`, with `connection` registered as an alias. Neither collides with
  an existing `FileLocationHandler` scheme, and `getInfo()`'s scheme regex
  (`x/^(\w+):\/\//`) accepts both.
- Everything up to the first `/` is the connection name; everything after is the
  path.
- **The path is always relative to the connection's own base location.** That is
  the property that makes the scheme worth having: the connection owns the
  host, the credentials, and the root, and the location string owns only the
  leaf. `..` segments that would escape the connection root are rejected.
- The existing generic `{...}` option suffix works unchanged.

Examples:

```
conn://prod-sftp/outbound/orders-20260805.csv
conn://cust-files/reports/q3.xlsx{encoding=UTF-8}
conn://my-s3/invoices/2026/08/inv-1001.pdf
conn://my-s3/other-bucket/key/path.pdf{container=other-bucket}
conn://corp-dropbox/Shared/specs/api.yaml
conn://corp-onedrive/01ABCDEF...{path_is_id=true}
```

Handler-level options, in addition to whatever the resolved target accepts:

| Option | Type | Meaning |
|---|---|---|
| `encoding` | string | text encoding for text reads/writes |
| `subtype` | string | data provider subtype passed to `AbstractConnection::getDataProvider(subtype)` |
| `container` | string | bucket / drive / parent-folder override (otherwise taken from the connection or from the first path segment) |
| `path_is_id` | bool | the path is an opaque object ID, not a hierarchical path |
| `follow_url` | bool | for URL-mode reads, fetch the returned URL (default `True`) |
| `conn_rtopts` | hash | runtime options forwarded to `AbstractConnection::get()` |

---

## 4. Resolution tiers

`FileLocationHandlerConnection` resolves the connection once, then dispatches
through the first tier that applies.

### Tier 1a — scheme redirect (cheap, covers the URL-expressible connections)

Many connections are exactly "an existing `FileLocationHandler` scheme plus
credentials plus a base path". For those, the connection does not need any file
I/O code at all — it only needs to say *where to send the request*:

```qore
#! Describes how to satisfy a file location through an existing handler scheme
public hashdecl FileLocationRedirect {
    #! target scheme, e.g. "sftp"
    string scheme;
    #! the location string for that scheme, without the scheme prefix
    string location;
    #! options to merge into the request options
    *hash<auto> opts;
}
```

`FileLocationHandlerConnection` builds the redirect and re-enters the normal
dispatch path for `redirect.scheme`. Every existing handler's blocking, streaming,
and polling implementation is reused verbatim — **no I/O code is duplicated**.

Covers: `FilesystemConnection` (`file`/`dir` → `file://<root>/<path>`),
`FtpConnection` (→ `ftp(s)://user:pass@host/<base>/<path>`), `SftpConnection`
(→ `sftp://...`), and plain `HttpConnection` (→ `http(s)://...`).

The URL used to build the redirect is the credential-bearing `getUrl()`; **every
error message and log line must use `getSafeUrl()`** so passwords never reach
exception text.

### Tier 1b — native handler object (for non-URL-expressible state)

REST connections carry state that cannot be put in a URL: OAuth2 access tokens
with automatic refresh, SPNEGO/Kerberos config, client certificates supplied as
data rather than paths, per-connection headers. For those, the connection hands
back a *live client object* instead of a URL:

```qore
# in AbstractConnection — declared as "object" deliberately; see §6
const string CF_FILE_LOCATIONS = "file-locations";

bool supportsFileLocations()                       { return supportsFileLocationsImpl(); }
object getFileLocationHandler()                    { return getFileLocationHandlerImpl(); }
*hash<FileLocationRedirect> getFileLocationRedirect(string path, bool write) { ... }

private bool   supportsFileLocationsImpl()         { return False; }
private object getFileLocationHandlerImpl()        { throw "FILE-LOCATION-UNSUPPORTED", ...; }
```

To make this cheap to implement, `FileLocationHandlerHttp` and
`FileLocationHandlerRest` should gain a constructor (or a protected setter) that
accepts an injected, already-authenticated client object, so a REST connection's
handler is a three-line subclass rather than a reimplementation.

### Tier 2 — declarative data provider file API (covers the SaaS apps)

For app-backed connections, add a declarative contract to the action catalog
that says which actions implement read and write and how their options and
responses map onto file semantics. This is the piece that makes S3, Dropbox,
Google Drive, and OneDrive reachable.

```qore
#! Read modes
public const string DPFR_DATA = "data";   #!< the response field holds the file data
public const string DPFR_URL  = "url";    #!< the response field holds a URL to fetch

#! Write payload formats
public const string DPFW_FILE   = "file";     #!< FileDataType hash: {name, content, mime_type}
public const string DPFW_BASE64 = "base64";
public const string DPFW_RAW    = "raw";

#! Describes how an application's actions map onto file operations
public hashdecl DataProviderFileApiInfo {
    #! read (download) action name in the app's action catalog
    string read_action;
    #! option in the read action that takes the path / object key / id
    string read_path_option;
    #! option that takes a container (bucket / drive / parent folder), if any
    *string read_container_option;
    #! DPFR_DATA or DPFR_URL
    string read_mode = DPFR_DATA;
    #! response field holding the data (DPFR_DATA) or the URL (DPFR_URL)
    string read_data_field;
    #! "base64" or "raw"; applies to DPFR_DATA
    string read_data_encoding = "base64";
    #! response field holding the MIME type, if any
    *string read_mime_type_field;

    #! write (upload) action name; if unset the app is read-only
    *string write_action;
    *string write_path_option;
    *string write_container_option;
    #! option that takes the file payload
    *string write_data_option;
    #! DPFW_FILE, DPFW_BASE64, or DPFW_RAW
    string write_data_format = DPFW_FILE;

    #! paths are opaque IDs rather than hierarchical paths
    bool path_is_id = False;
    #! fixed options added to every call
    *hash<auto> options_add;
}

# registration / lookup
DataProviderActionCatalog::registerAppFileApi(string app, hash<DataProviderFileApiInfo> info);
*hash<DataProviderFileApiInfo> DataProviderActionCatalog::getAppFileApi(string app);
```

Dispatch for tier 2:

1. `app = conn.getAppName()`; `info = getAppFileApi(app)`; if none → tier 3.
2. `action = DataProviderActionCatalog::getAppActionEx(app, info.read_action)`.
3. `dp = conn.getDataProvider(action.subtype ?? opts.subtype)`, then
   `dp.getChildProviderPath(action.path)`.
4. `dp.doRequest({info.read_path_option: path} + container + info.options_add)`.
5. Extract `info.read_data_field`; decode per `read_data_encoding`, or, for
   `DPFR_URL`, re-enter `FileLocationHandler` with the returned URL when
   `follow_url` is set.

Concrete registrations (the modules register these in their own `init` blocks;
v8/TS apps get a new `files: {...}` key on `TQoreAppWithActions` in `ts-toolkit`
that the v8 module forwards to `registerAppFileApi()`):

| App | `read_action` | `read_path_option` | `read_mode` | `read_data_field` | `write_action` | `write_data_option` |
|---|---|---|---|---|---|---|
| `file` | `get` | `path` | data | `data` | `create` | `data` (raw) |
| `ftp` | `get-file` | `path` | data | `data` | `send-file` | `data` (raw) |
| OneDrive | `download-file` | item id (`path_is_id`) | **url** | `download_url` | `upload-file` | `file` |
| Amazon S3 | `get_file` | `object_key` (container `bucket_name`) | data | `content` (base64) | `upload_file` | `file` |
| Dropbox | `download_file` | `path` | data | `content` (base64) | `upload_file` | `file` |
| Google Drive | *(needs a new `download_file` action)* | file id (`path_is_id`) | data | — | `upload_file` | `file` |

### Tier 3 — no support

Throw `LOCATION-ERROR` naming the connection, its type, its app (if any), and
the reason — with `getSafeUrl()`, never `getUrl()`.

---

## 5. Streaming, polling, and async

- **Tier 1a** inherits the delegate handler's behavior exactly, including
  `getIoPollerForLocation()` where the delegate supports it and `UNIMPLEMENTED`
  where it does not (SFTP).
- **Tier 1b** inherits whatever the injected-client handler supports; REST
  connections get polling via the existing
  `FileLocationHandlerRest::getIoPollerForLocationImpl()` path.
- **Tier 2** is request/response only. Streams are synthesized with the
  `StreamPipe` + background-thread pattern already used by the FTP and SFTP
  handlers (`FileLocationHandlerSftp.qc:113-126`), including the
  `OutputStreamWrapper` + `WaitGroup` error-propagation discipline —
  `on_exit io_counter.done()` **outside** the `try` so `waitForIo()` observes a
  recorded error before the wait group drains. `getIoPollerForLocation()` throws
  `UNIMPLEMENTED` for tier 2, matching the SFTP precedent.
- For `DPFR_URL` reads, the poller *can* be supported: resolve the URL with a
  blocking action call, then return the HTTP handler's poller for it. Worth
  doing in a later phase, not phase 1.

---

## 6. Module dependency direction

`FileLocationHandler` must **not** gain `%requires ConnectionProvider`.

`FileLocationHandler` is a leaf utility with a single dependency (`Util`), and it
sits *below* `ConnectionProvider` in practice — `RestClient` requires both, and
`ConnectionProvider::HttpConnection` documents its `ssl_cert_path` /
`ssl_key_path` options as being resolved through `FileLocationHandler`. Adding a
hard edge would drag `DataProvider`, `AsyncSocketIo`, `reflection`,
`ProviderIndexUtil`, and `Logger` into every consumer of a file location, and it
would turn any future `ConnectionProvider` → `FileLocationHandler` edge into a
hard cycle.

Instead, follow the module's own existing idiom: `FileLocationHandler::init()`
registers `FileLocationHandlerConnection` for `conn` and `connection`
unconditionally, and the handler does `load_module("ConnectionProvider")` +
`call_function("ConnectionProvider::get_connection", name)` on first use,
holding the result in an `object`. Exactly what
`FileLocationHandlerSftp::getSftpClient()` does with `ssh2`. Failure to load
becomes a clear `LOCATION-ERROR` rather than a link-time cost for everyone.

The tier-2 hashdecl and registration API belong in `DataProvider`, which
`ConnectionProvider` already re-exports, so app modules can register without
new dependencies.

---

## 7. One required core change: option validation

`FileLocationHandler::getInfo()` validates the parsed `{...}` options against the
option set declared by the handler *for the scheme in the location string*
(`FileLocationHandler.qc:498-506`). For `conn://` that is the connection
handler's own set — but the pass-through options legitimately belong to whatever
target the connection resolves to, which is not known until after resolution.

Add a small, backward-compatible opt-out to `AbstractFileLocationHandler`:

```qore
#! Returns True if the handler validates its own options; the framework skips pre-validation
bool selfValidatesOptions() { return selfValidatesOptionsImpl(); }
private bool selfValidatesOptionsImpl() { return False; }
```

`getInfo()` skips the unknown-option check when this returns `True`. The
connection handler then validates precisely, once, against the resolved target —
producing a *better* error than the current one because it can name the actual
target scheme. Default `False` keeps every existing handler's behavior identical.

The alternative — having the connection handler declare the union of every
registered handler's options — is rejected: it makes the option set
order-dependent on module load and produces useless error messages.

---

## 8. Security

Named-connection resolution widens what a location string can reach: a string
that previously could only name a URL can now name any configured system,
including internal ones. Location strings are frequently derived from user or
partner data. Therefore:

1. **Credential hygiene.** Error messages, exception `arg` values, and log lines
   use `AbstractConnection::getSafeUrl()`, never `getUrl()`. This must be
   asserted in tests, not just documented.
2. **Path confinement.** Tier 1a resolves the joined path and rejects any result
   outside the connection root (`..` traversal), before any I/O.
3. **Opt-in gating.** Add a static filter hook so an embedding application can
   restrict which connections are reachable through file locations:
   ```qore
   FileLocationHandler::setConnectionFilter(*code<bool(string conn_name, bool write)> filter);
   ```
   with a `QORE_FILE_LOCATION_CONNECTIONS` environment variable as the
   no-code default (unset = all connections allowed, preserving the principle of
   least surprise for the common trusted-configuration case; sites that accept
   untrusted location strings set it).
4. **Functional domains.** No new C++ code is involved, so no new `QDOM_*`
   tagging is required, but the handler inherits the sandbox restrictions of
   whatever delegate it dispatches to — which is the correct behavior and should
   be stated in the docs.

---

## 9. Error handling

All failures raise `LOCATION-ERROR` (the module's existing convention), with the
original exception preserved in `arg` where one exists:

| Condition | Message content |
|---|---|
| `ConnectionProvider` not loadable | name the module and the underlying load error |
| `QORE_CONNECTION_PROVIDERS` unset | say so explicitly — this is the most common first-run failure |
| unknown connection name | the name, and the providers that were tried |
| connection blocked by filter | the name only |
| connection supports no file access | name, type, app, and the tiers that were tried |
| path escapes the connection root | the offending relative path, never the absolute one |
| app read action returns no data field | the app, action, and expected field name |

---

## 10. Testing

New `examples/test/qlib/FileLocationHandler/` coverage, using
`%prepend-module-path` for local modules and `TmpDir`/`TmpFile` from `FsUtil`:

- **Unit:** location parsing — name/path split, empty path, path with `{}`
  options, path containing `{`, name containing a dot or hyphen, `conn://` with
  no path, `connection://` alias.
- **Tier 1a integration:** a stub connection provider module registering a
  `FilesystemConnection` over a `TmpDir`; round-trip text and binary read/write,
  stream reader, output stream, append, encoding conversion.
- **Path confinement (negative):** `conn://c/../../etc/passwd` and
  `conn://c/a/../../../x` must throw and must not touch the filesystem.
- **Tier 2 integration:** a stub app + data provider registering a
  `DataProviderFileApiInfo`, exercising `DPFR_DATA`/base64, `DPFR_DATA`/raw,
  `DPFR_URL` with `follow_url` true and false, container options, and
  `path_is_id`.
- **Negative:** unknown connection; `QORE_CONNECTION_PROVIDERS` unset;
  connection with no file support; app whose declared read action does not
  exist; read action whose response lacks `read_data_field`; write attempted
  against a read-only app API (no `write_action`).
- **Credential-leak assertions:** force each failure path on a connection whose
  URL contains a password and assert the password appears in neither `ex.desc`
  nor `ex.arg` nor the log.
- **Option validation:** an unknown option must still be rejected — via the
  resolved target, with the target scheme named in the message.
- **Filter hook:** allowed and denied connections, for read and write
  independently.

No C++ changes, so no valgrind run is required.

---

## 11. Phasing

| Phase | Content | Ships value |
|---|---|---|
| 1 | `conn` / `connection` schemes; lazy `ConnectionProvider` load; `selfValidatesOptions()`; tier 1a redirect; `FilesystemConnection` + `FtpConnection` + `SftpConnection` + `HttpConnection`; path confinement; filter hook; full test suite | filesystem/FTP/SFTP/HTTP connections usable as file locations |
| 2 | `DataProviderFileApiInfo` + `registerAppFileApi()`; tier 2 dispatch; registrations for `FileDataProvider`, `FtpClientDataProvider`, `OneDriveDataProvider` | Qore-side file apps reachable |
| 3 | Native S3 / Google Drive / Dropbox qlib modules — see [`cloud-storage-qlib-modules.md`](cloud-storage-qlib-modules.md) | SaaS file stores reachable, with streaming |
| 4 | Tier 1b injected-client handlers for REST connections and the three migrated modules; poller support for `DPFR_URL` reads | OAuth2 REST connections + non-blocking reads |
| 5 | *(optional, deferred)* `files: {...}` key in `ts-toolkit` `TQoreAppWithActions`; v8 module forwards it, for the ~80 apps that stay in TS | remaining TS apps reachable |

Phase 1 is self-contained and useful on its own; each later phase is additive
and cannot regress the earlier ones.

**Phase 3 changed shape** once the cloud-storage migration entered scope. It was
originally "add a `files: {...}` key to `ts-toolkit` and register file APIs for
the TS S3 and Dropbox apps, plus a new `download_file` action for the TS Google
Drive app." Since those three apps are unreleased and are being migrated to
native qlib modules, that TS-side plumbing leaves the critical path: the migrated
modules implement tier 1b directly, which gives real streaming rather than
base64-in-JSON action dispatch. The TS bridge survives as an optional phase 5 for
the apps that stay in TypeScript. See §7 of the companion document for the
combined A–G sequencing.

---

## 12. Open questions

1. **Scheme name.** `conn://` is proposed for brevity, `connection://` as the
   alias. If a shorter or more explicit name is preferred (`c://`, `cx://`), it
   must be decided before phase 1 ships, since location strings become
   persisted configuration.
2. **Container in the path vs. in options.** For S3 the bucket could be the
   first path segment (`conn://s3/bucket/key`) or an option
   (`conn://s3/key{container=bucket}`). The proposal supports both, with the
   connection's own configured bucket as the default; if only one is wanted,
   the option form is the less ambiguous.
3. **Qorus interaction.** Qorus has its own connection namespace and its own
   file-location conventions; whether `conn://` should resolve against the
   Qorus connection registry directly or through `QORE_CONNECTION_PROVIDERS`
   needs a decision from that side.
4. **Google Drive read.** `get_file` returns metadata only — there is no action
   that returns bytes. Resolved by the migration: the native
   `GoogleDriveDataProvider` implements `download-file` against
   `alt=media`. If the migration does not proceed, a `download_file` action must
   be added to the TS app instead.
5. **Listing.** This proposal covers read and write of a single file only.
   Directory listing through a connection (`FilePoller`-style) is a natural
   follow-on but is deliberately out of scope.
