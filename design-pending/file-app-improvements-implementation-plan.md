# Implementation Plan — issue #5386, `feature/5386_file_app_improvements`

**Issue:** [#5386](https://github.com/qoretechnologies/qore/issues/5386)
**Branch:** `feature/5386_file_app_improvements`

**Design documents:**
- [`file-location-handler-connection-scheme.md`](file-location-handler-connection-scheme.md) — the `conn://` scheme
- [`cloud-storage-qlib-modules.md`](cloud-storage-qlib-modules.md) — native S3 / Google Drive / Dropbox modules

**Normative references.** Every phase conforms to these; deviations are called
out explicitly where they occur:
- `design/qore-module-structure.md`
- `design/data-provider-development-guide.md`
- `design/data-provider-checklist.md`
- `design/data-provider-semantic-string-types.md`

**Reference implementation.** `qlib/OneDriveDataProvider/` is the structural
template for all three new modules — the newest cloud-storage module in the
repo, already async-first on `RestClientIo`, with the flat-`ChildMap` and
event-provider shapes this plan reuses.

---

## 0. Ground rules for the whole branch

### 0.1 Identity is preserved

App names and icons carry over from the v8/TS apps **unchanged** — these are the
same apps reimplemented, not new ones. Values taken from each TS app's
`constants.ts` and `index.ts`:

| TS app | `AppName` const | Logo file | Connection scheme | Factory name |
|---|---|---|---|---|
| `apps/amazon-s3` | `AmazonS3` | `amazon-s3-logo.svg` | `s3` | `s3` |
| `apps/google-drive` | `GoogleDrive` | `google-drive-logo.svg` | `gdrive` | `gdrive` |
| `apps/dropbox` | `Dropbox` | `dropbox-logo.svg` | `dropbox` | `dropbox` |

Logos are produced by base64-decoding the `*_APP_LOGO` constant in each TS
`constants.ts` and writing the SVG into the module directory.

**Correction required during extraction:** `design/data-provider-checklist.md`
mandates square icons, and the current Dropbox SVG is `width="1200"
height="800"` with `viewBox="-35.3175 -50 306.085 300"`. Normalize each
extracted logo to square dimensions rather than copying the asset verbatim.

Per the checklist's App Icon Convention the logo is a separate file loaded at
module level, never an inlined string, following
`qlib/OneDriveDataProvider/OneDriveDataProviderDefs.qc:31`:

```qore
public const AmazonS3Logo = File::readTextFile(get_script_dir() + "/amazon-s3-logo.svg");
```

### 0.2 Registration is a three-place operation

Omitting any one of these fails silently, in a different way each time:

| Place | Entry | Symptom if missing |
|---|---|---|
| `qlib/ConnectionProvider/ConnectionSchemeCache.qc` → `SchemeMap` | `"s3": "AwsRestClient"` | every connection becomes `(InvalidConnection)`, `type: "invalid"`, no `app`; silently filtered out of the action picker |
| `qlib/DataProvider/DataProvider.qc` → `FactoryMap` | `"s3": "AwsS3DataProvider"` | module loads fine, app never appears in the apps list |
| `CMakeLists.txt` | `qore_user_module("qlib/AwsS3DataProvider" "amazon-s3-logo.svg")` | not installed, no docs target |

The `qore_user_module()` second argument is **extra resource files for the doc
build**, not dependencies — dependencies are derived from `%requires`. This was
mis-documented in both `design/qore-module-structure.md` and the
`QORE_USER_MODULE` header comment; corrected on develop in `650ae74c2`.

### 0.3 Per-phase definition of done

No phase is complete until all of:

- `cmake --build build --target <Module>-qmod` succeeds for every touched qlib
  module. **A stale `.qmod` silently shadows edited `.qm`/`.qc` sources — the
  edited code simply never runs, with no error.** This is the single most common
  way to lose an afternoon on this branch.
- `./run_tests.sh -d qlib/<Module>` passes with no warnings and no errors, run
  under `qore --enable-debug`.
- The relevant boxes in `design/data-provider-checklist.md` are ticked.
- `cmake --build build --target docs-<Module>` builds clean. Doc tables use the
  Qore pipe format (`|!Header|!Header` / `|cell|cell`), never Markdown —
  `QORE_DOX_TABLE_STRICT` is on in this repo and fails the build otherwise.
- No valgrind run: there are no C++ changes anywhere in this branch.

---

## Phase A — `conn://` core + tier 1a redirect

Self-contained and shippable alone; nothing later depends on reaching a SaaS
file store.

### A.1 `FileLocationHandler` core change

`qlib/FileLocationHandler/FileLocationHandler.qc`:

```qore
# in AbstractFileLocationHandler, alongside the other non-abstract wrappers
#! Returns True if the handler validates its own options
/** The framework skips its own unknown-option check when this returns True; the
    handler is then responsible for validating options itself.  Needed by handlers
    that do not know the effective target until after resolving the location.
*/
bool selfValidatesOptions() {
    return selfValidatesOptionsImpl();
}

private bool selfValidatesOptionsImpl() {
    return False;
}
```

In `FileLocationHandler::getInfo()` the unknown-option check at
`FileLocationHandler.qc:503-506` becomes conditional:

```qore
if (!handler.selfValidatesOptions() && info.opts
        && (*hash<auto> erropts = info.opts - (keys handler_opts))) {
    throw "LOCATION-ERROR", ...;
}
```

Default-value and required-option processing (the `foreach` immediately after)
is **unchanged** and still runs for every handler.

Scheme registration in `FileLocationHandler::init()`:

```qore
FileLocationHandler::registerHandler("conn", new FileLocationHandlerConnection());
FileLocationHandler::registerHandler("connection", new FileLocationHandlerConnection());
```

Connection filter hook on the `FileLocationHandler` class:

```qore
static setConnectionFilter(*code<bool(string conn_name, bool write)> filter);
static *code<bool(string, bool)> getConnectionFilter();
```

with `QORE_FILE_LOCATION_CONNECTIONS` (a `PathSep`-separated allow-list) as the
no-code default, read in `checkDynamicInit()`. Unset means all connections are
allowed, preserving least surprise for the common trusted-configuration case.

`FileLocationHandler.qm`: add the new class to the mainpage class list, add a
`@subsection filelocationhandler_v3_1` release note, bump `version = "3.1"`.

### A.2 New handler — `FileLocationHandlerConnection.qc`

```qore
class FileLocationHandlerConnection inherits AbstractFileLocationHandler {
    public {
        const ConnectionLocationOpts = {
            "encoding":    <FileHandlerOptionInfo>{"type": "string", "desc": "..."},
            "subtype":     <FileHandlerOptionInfo>{"type": "string", "desc": "..."},
            "container":   <FileHandlerOptionInfo>{"type": "string", "desc": "..."},
            "path_is_id":  <FileHandlerOptionInfo>{"type": "bool", "default_value": False, "desc": "..."},
            "follow_url":  <FileHandlerOptionInfo>{"type": "bool", "default_value": True,  "desc": "..."},
            "conn_rtopts": <FileHandlerOptionInfo>{"type": "hash",  "desc": "..."},
        };
    }

    private bool selfValidatesOptionsImpl() { return True; }
    private hash<string, hash<FileHandlerOptionInfo>> getReadOptionsImpl()  { return ConnectionLocationOpts; }
    private hash<string, hash<FileHandlerOptionInfo>> getWriteOptionsImpl() { return ConnectionLocationOpts; }
    # ... the eight AbstractFileLocationHandler overrides, all funneling through resolve()
}
```

Resolution is lazy, with **no `%requires ConnectionProvider`** (design §6 —
`FileLocationHandler` is a leaf utility with only a `Util` dependency and sits
below `ConnectionProvider` in practice):

```qore
private object getConnection(string name) {
    try {
        load_module("ConnectionProvider");
    } catch (hash<ExceptionInfo> ex) {
        throw "LOCATION-ERROR", sprintf("cannot resolve connection %y; the ConnectionProvider "
            "module cannot be loaded: %s", name, ex.desc), ex.arg;
    }
    return call_function("ConnectionProvider::get_connection", name);
}
```

held in an `object`, exactly as `FileLocationHandlerSftp::getSftpClient()` does
with `ssh2` (`FileLocationHandlerSftp.qc:245-252`).

Resolution order per call:

1. Split `<name>/<path>` at the first `/`.
2. Apply the connection filter; deny ⇒ `LOCATION-ERROR` naming only the
   connection name.
3. Resolve the connection.
4. **Tier 1a:** `*hash<FileLocationRedirect> r = conn.getFileLocationRedirect(path, write)`.
   If set, validate `opts` against the target handler's option set (naming the
   *target* scheme in any error) and re-enter dispatch for `r.scheme`.
5. **Tier 1b** (phase F): `conn.supportsFileLocations()` ⇒
   `conn.getFileLocationHandler()` and delegate.
6. **Tier 2** (phase D): app file-API lookup.
7. Otherwise `LOCATION-ERROR` naming the connection, its type, its app, and the
   tiers tried.

**Path confinement** happens before any I/O: resolve the joined path and reject
any result outside the connection root. **Every** error message, exception
`arg`, and log line uses `getSafeUrl()`, never `getUrl()`.

### A.3 `ConnectionProvider` additions

`qlib/ConnectionProvider/ConnectionProvider.qm` — new hashdecl:

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

`qlib/ConnectionProvider/AbstractConnection.qc`:

```qore
#! Connection feature: file-location support
const string CF_FILE_LOCATIONS = "file-locations";

bool supportsFileLocations() { return supportsFileLocationsImpl(); }

#! Returns an object implementing FileLocationHandler::AbstractFileLocationHandler
/** NOTE: the return type is deliberately "object", not the concrete handler class.
    Declaring the real type would create a hard ConnectionProvider -> FileLocationHandler
    dependency; FileLocationHandler is a leaf utility that sits below this module (see
    design-pending/file-location-handler-connection-scheme.md §6).  Do not "tidy" this.
*/
object getFileLocationHandler() { return getFileLocationHandlerImpl(); }

*hash<FileLocationRedirect> getFileLocationRedirect(string path, bool write) {
    return getFileLocationRedirectImpl(path, write);
}

private bool supportsFileLocationsImpl() { return False; }
private object getFileLocationHandlerImpl() {
    throw "FILE-LOCATION-UNSUPPORTED",
        sprintf("connection type %y does not support file locations", getType());
}
private *hash<FileLocationRedirect> getFileLocationRedirectImpl(string path, bool write) {}
```

Implement `getFileLocationRedirectImpl()` in:

| Class | File | Redirect |
|---|---|---|
| `FilesystemConnection` | `qlib/ConnectionProvider/FilesystemConnection.qc` | `file://` + `normalize_dir(urlh.path + "/" + path)` |
| `FtpConnection` | `qlib/ConnectionProvider/FtpConnection.qc` | `ftp`/`ftps` + credential-bearing URL + base path + path |
| `HttpConnection` | `qlib/ConnectionProvider/HttpConnection.qc` | `http`/`https` + URL + path |
| `SftpConnection` | **`module-ssh2` — separate repo** | `sftp://` + URL + path |

`SftpConnection` lives in another repository. If it cannot land in step,
`conn://` over SFTP falls back to tier 3 with a clear error and the qore-side
tests cover filesystem/FTP/HTTP only — track it as a follow-on rather than
blocking phase A.

Bump `ConnectionProvider.qm` to `version = "3.2"` with a release-notes entry.

### A.4 Tests — `examples/test/qlib/FileLocationHandler/`

New `FileLocationHandlerConnection.qtest` (exec bit set, `%modern`,
`%prepend-module-path` for local modules, `TmpDir`/`TmpFile` from `FsUtil`,
hard `%requires` since everything involved ships with Qore).

The suite needs a stub connection-provider module registered via
`QORE_CONNECTION_PROVIDERS`, exposing `get_mod_connection()` /
`get_mod_connections()` over a `FilesystemConnection` rooted at a `TmpDir`.

| Group | Cases |
|---|---|
| Parsing | name/path split; empty path; `{}` options; path containing `{`; name with `.` and `-`; `connection://` alias |
| Tier 1a round trip | text read/write; binary read/write; `getStreamReaderFromLocation()`; `getOutputStreamForLocation()` incl. `close()` + `waitForIo()`; `append`; encoding conversion |
| Path confinement | `conn://c/../../etc/passwd` and `conn://c/a/../../../x` both throw **and touch nothing** |
| Credential hygiene | force every failure path on a connection whose URL carries a password; assert it appears in neither `ex.desc`, `ex.arg`, nor the log |
| Negative | unknown connection; `QORE_CONNECTION_PROVIDERS` unset; connection with no file support; unknown option — error must name the **resolved target** scheme |
| Filter hook | allowed and denied, read and write independently; env-var form and code form |
| Regression | `selfValidatesOptions()` defaults `False`; every pre-existing scheme still rejects unknown options identically |

---

## Phase B — Dropbox transport spike (gate)

Timeboxed prototype under the scratchpad, **not committed**.

Dropbox splits its API across two hosts with two calling conventions:

| | RPC | Content |
|---|---|---|
| Host | `api.dropboxapi.com` | `content.dropboxapi.com` |
| Used by | metadata, list, move, copy, share | `upload`, `download`, upload sessions |
| Arguments | JSON request body | **`Dropbox-API-Arg` header, JSON-encoded** |
| Body | JSON | raw binary |

**Question:** can `RestClient` / `RestClientIo` issue a request with
JSON-encoded arguments in a header and a raw binary body, and stream both
directions, without fighting the client's serialization layer?

**Gate:** yes ⇒ phase E proceeds as planned. No ⇒ Dropbox defers and phases
C/D/F proceed unaffected. Record the finding in
`cloud-storage-qlib-modules.md` §4.3 either way.

This runs first because it is the only unknown that can reshape later phases,
and it is cheap to answer.

---

## Phase C — `AwsS3DataProvider` + `GoogleDriveDataProvider`

Both follow the OneDrive layout: a flat root `ChildMap` naming every action
provider as a direct child, plus a nested `events` container.

### C.1 `AwsS3DataProvider`

**Client:** no new client module. `qlib/AwsRestClient.qm` (v1.3) already
implements S3 SigV4 — the `aws_s3` flag on client and connection, the
S3-specific canonical-request rule (S3 URIs encoded **once**, every other AWS
service **twice**, S3 paths not normalized — `AwsRestClient.qm:945-965`), XML
body parsing with no external XML module, and STS `AssumeRole`. **Do not
reimplement any of it.**

Add a thin `AwsS3RestConnection` to `qlib/AwsRestClient.qm` pinning
`aws_service = "s3"` / `aws_s3 = True` with an S3 ping and its own
`required_options`, mirroring how `GoogleCalendarRestConnection` pins
`api_profile` (`GoogleRestClient.qm:409-500`).

**Files** (`qlib/AwsS3DataProvider/`):

```
AwsS3DataProvider.qm              # module decl; registerFactory, registerApp, registerAction
AwsS3DataProviderDefs.qc          # AppName = "AmazonS3", AmazonS3Logo, event names, limits
AwsS3DataProviderBase.qc          # *RestClientIo rest; ConstructorOptions; RetrySet; MaxIoRetries
AwsS3DataProvider.qc              # root provider + DefaultChildMap
AwsS3ListBucketsDataProvider.qc
AwsS3CreateBucketDataProvider.qc
AwsS3ListObjectsDataProvider.qc   # record-based, ContinuationToken pagination
AwsS3GetObjectDataProvider.qc
AwsS3DownloadFileDataProvider.qc
AwsS3UploadFileDataProvider.qc    # multipart above the single-shot threshold
AwsS3DeleteObjectDataProvider.qc
AwsS3CopyObjectDataProvider.qc
AwsS3EventsDataProvider.qc        # events container
AwsS3WatchObjectsDataProvider.qc  # DPAT_EVENT, replaces the TS triggers
AwsS3DataTypes.qc
amazon-s3-logo.svg
```

**Action map.** TS options carry over except where §5 of the migration design
renames them for cross-app consistency:

| Action | S3 operation | Options |
|---|---|---|
| `list-buckets` | `GET /` | `region`, `include_location` |
| `create-bucket` | `PUT /{bucket}` | `bucket_name`, `region`, `acl`, `object_lock_enabled` |
| `list-objects` | `GET /{bucket}?list-type=2` | `bucket_name`, `prefix`, `max_keys`, `continuation_token`, `delimiter`, `start_after`, `region` |
| `get-object` | `GET /{bucket}/{key}` | `bucket_name`, `object_key`, `include_content`, `version_id`, `region` |
| `download-file` | `GET /{bucket}/{key}` | `bucket_name`, `object_key`, `version_id`, `region` |
| `upload-file` | `PUT /{bucket}/{key}` or multipart | `bucket_name`, `object_key`, `file`, `storage_class`, `metadata`, `tags`, `region` |
| `create-text-file` | `PUT /{bucket}/{key}` | `bucket_name`, `object_key`, `content`, `content_type`, `storage_class`, `metadata`, `tags`, `region` |
| `delete-object` | `DELETE /{bucket}/{key}` | `bucket_name`, `object_key`, `version_id`, `bypass_governance_retention`, `region` |
| `copy-object` | `PUT` + `x-amz-copy-source` | source/target bucket + key |

The TS app's `get_file`/`get_object` and `list_files`/`list_objects` are
near-duplicate pairs. Collapse each to one action — `get-object` with
`include_content`, and `list-objects` with an optional extension filter.
Events: `new-bucket`, `new-or-updated-file`.

**New surface previously handled by the AWS SDK** — budget for it explicitly,
it is the bulk of the work in this module:

- virtual-host-style (`<bucket>.s3.<region>.amazonaws.com`) vs path-style
  addressing, and when each is required
- region redirects: `301 PermanentRedirect` / `307` with `x-amz-bucket-region`,
  and **re-signing after redirect**
- XML `ListObjectsV2` parsing with `ContinuationToken` pagination
- multipart upload (`CreateMultipartUpload` / `UploadPart` /
  `CompleteMultipartUpload`) and multipart ETag semantics
- `Content-MD5` / `x-amz-checksum-*` where required

### C.2 `GoogleDriveDataProvider`

**Client:** extend `qlib/GoogleRestClient.qm`. Add to
`GoogleRestClientBase::ApiProfiles` (currently `none`, `calendar`, `gmail`):

```qore
"drive": {
    "oauth2_scopes": ("https://www.googleapis.com/auth/drive",),
    "ping_method": DefaultGooglePingMethod,
    "ping_path": "/drive/v3/about?fields=user",
    "ping_headers": DefaultGooglePingHeaders,
},
```

plus a `GoogleDriveRestConnection` pinning `api_profile = "drive"`, mirroring
`GoogleCalendarRestConnection`.

**Files:** same shape as C.1 under `qlib/GoogleDriveDataProvider/`, app name
`GoogleDrive`, logo `google-drive-logo.svg`.

**Action map:**

| Action | Drive v3 endpoint | Notes |
|---|---|---|
| `list-files` | `GET /drive/v3/files` | record-based, real `pageToken` iteration |
| `find-file` / `find-folder` | `GET /drive/v3/files?q=` | `DPAT_FIND`; every option must exist in `SearchOptions` |
| `get-file` | `GET /drive/v3/files/{id}` | metadata |
| **`download-file`** | `GET /drive/v3/files/{id}?alt=media` | **new — closes the content-read gap** |
| **`export-file`** | `GET /drive/v3/files/{id}/export?mimeType=` | **new — Google-native docs** |
| `upload-file` | `POST /upload/drive/v3/files?uploadType=` | `multipart`, and `resumable` for large files |
| `replace-file` | `PATCH /upload/drive/v3/files/{id}` | |
| `create-file-from-text` | `POST /upload/drive/v3/files` | |
| `create-folder` | `POST /drive/v3/files` | `mimeType=application/vnd.google-apps.folder` |
| `copy-file` | `POST /drive/v3/files/{id}/copy` | |
| `move-file` | `PATCH /drive/v3/files/{id}` | `addParents` / `removeParents` |
| `delete-file` | `DELETE` or `PATCH trashed=true` | `permanently_delete` selects |
| `create-shortcut` | `POST /drive/v3/files` | shortcut mimeType |
| `add-sharing-preference` | `POST /drive/v3/files/{id}/permissions` | |

Events: `new-file`, `new-folder` via `changes.list` with a `startPageToken`.

**Settle design open question 5 here:** whether the scope set is fixed at full
`drive` or made profile-selectable so deployments can use the far narrower
`drive.file` (app-created files only). Recommend selectable now — far cheaper
than changing it after release.

### C.3 Conformance for both

From `design/data-provider-checklist.md`, the items that fail *silently* if
missed:

- **Action path resolution.** Every `registerAction()` `path` must resolve
  through the root `ChildMap`. If it does not, the action's provider is left
  `null`: `ref_data` lookups return nothing (empty dropdowns) and
  `doRequestImpl()` is never reached. Verify mechanically —
  `grep -A2 'DefaultChildMap' qlib/<M>/<M>DataProvider.qc` against
  `grep -nE '"path":' qlib/<M>/<M>DataProvider.qm`.
- Every `DPAT_API` action needs **both** `options` (via
  `DataProviderActionCatalog::getActionOptionFromFields()`) **and**
  `output_type`, or it renders with no form fields and is unusable.
- Single-key hash slices need the trailing comma — `Fields{"key",}`. Without it
  you get the value, not a hash, and an `OPTION-ERROR` at module load.
- `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>` — call
  `.getFields()`; returning the type object raises `RUNTIME-TYPE-ERROR` when the
  catalog acquires the type description.
- Fields with `required_groups` declared first, then required, then optional —
  no optional fields interleaved.
- All date/time fields use `DateType` / `DateOrNothingType` regardless of the
  API wire format; convert inside `doRequestImpl()`.
- `allowed_values` declared explicitly in the option, never described as prose
  in `desc` / `short_desc`.
- `short_desc` plain text under 80 chars; `desc` markdown.
- `groups`: `(DataProvider::AppGroup::CloudStorage,)` — enum, not a raw string.
- **Allowed-values callbacks** replacing the TS `get_allowed_values` helpers
  (`get-bucket-allowed-values`, `get-object-allowed-values`,
  `get-file-id-allowed-values`, `get-folder-id-allowed-values`). Skipping these
  is a silent UI regression and is the easiest thing in this phase to forget.
- `RestClientIo` throughout, so tier 1b pollers work in phase F.
- Grep every `<scheme>://` literal in each module against the registered scheme
  set — a typo makes every connection `(InvalidConnection)`.

### C.4 Tests

`examples/test/qlib/AwsS3DataProvider/AwsS3DataProvider.qtest` and
`examples/test/qlib/GoogleDriveDataProvider/GoogleDriveDataProvider.qtest`,
exec bit set, `%modern`, hard `%requires` (both are in-repo modules — no
`%try-module`).

Live credentials will not be in CI, so split each suite:

- **Always-run:** app and action catalog shape; every action path resolves
  through `ChildMap`; request construction; SigV4 signature vectors against
  known-good fixtures; XML/JSON response parsing from captured fixtures; type
  validation; option-schema conformance.
- **Integration:** gated on an explicit capability check, following
  `examples/test/qlib/AwsRestClient/`. Never a catch-all `try`/`catch` skip —
  that hides real regressions.

**Status.** Both suites exist and cover the always-run half: connection
construction and scheme registration, app and action catalog shape, every action
path resolving through the root `ChildMap`, an action advertising options if and
only if its request type has fields, and the file API registration naming
actions and options that exist. Writing them found three real defects in phase C
that offline "the module loads" checking had missed — see the phase F commit.
Request-construction and response-parsing cases against captured fixtures, which
`DropboxDataProvider.qtest` has for every action, are still missing for both.

---

## Phase D — `conn://` tier 2 declarative file API

### D.1 `DataProvider` additions

`qlib/DataProvider/DataProviderActionCatalog.qc` — the read-mode and
write-format constants, the `DataProviderFileApiInfo` hashdecl from design §4,
and:

```qore
static registerAppFileApi(string app, hash<DataProviderFileApiInfo> info);
static *hash<DataProviderFileApiInfo> getAppFileApi(string app);
```

guarded by the existing catalog lock and participating in
`ImplicitModuleTransaction` deregistration the way `registerScheme()` does.

### D.2 Tier 2 dispatch

In `FileLocationHandlerConnection`: `conn.getAppName()` → `getAppFileApi()` →
`getAppActionEx()` → `conn.getDataProvider(action.subtype ?? opts.subtype)` →
`getChildProviderPath(action.path)` → `doRequest()` → extract
`read_data_field`, decode per `read_data_encoding`, or re-enter
`FileLocationHandler` with the returned URL for `DPFR_URL`.

Streams are synthesized with the `StreamPipe` + background pattern used by the
FTP and SFTP handlers — **including the error-propagation discipline**:
`on_exit io_counter.done()` **outside** the `try`, so `waitForIo()` observes a
recorded error before the wait group drains
(`FileLocationHandlerSftp.qc:199-212`). `getIoPollerForLocation()` throws
`UNIMPLEMENTED` for tier 2, matching the SFTP precedent.

### D.3 Registrations

| Module | read action | path option | read mode | data field | write action | data option |
|---|---|---|---|---|---|---|
| `FileDataProvider` | `get` | `path` | data | `data` | `create` | `data` (raw) |
| `FtpClientDataProvider` | `get-file` | `path` | data | `data` | `send-file` | `data` (raw) |
| `OneDriveDataProvider` | `download-file` | item id (`path_is_id`) | **url** | `download_url` | `upload-file` | `file` |
| `AwsS3DataProvider` | `download-file` | `object_key` (container `bucket_name`) | data | `content` | `upload-file` | `file` |
| `GoogleDriveDataProvider` | `download-file` | file id (`path_is_id`) | data | `content` | `upload-file` | `file` |

Because phase C standardized the vocabulary, each registration is a few lines.

### D.4 Tests

Stub app + data provider registering a `DataProviderFileApiInfo`, exercising
`DPFR_DATA`/base64, `DPFR_DATA`/raw, `DPFR_URL` with `follow_url` both ways,
container options, and `path_is_id`. Negative: declared read action does not
exist; response lacks `read_data_field`; write attempted with no `write_action`.

---

## Phase E — `DropboxRestClient` + `DropboxDataProvider`

Conditional on the phase B gate.

**`qlib/DropboxRestClient.qm`** — client + `DropboxRestConnection`, templated on
`qlib/OneDriveRestClient.qm` (OAuth2 `authorization_code`,
`token_access_type: offline`). Must expose both the JSON-RPC path and the
content-transfer path. Scheme `dropbox`. Scopes as in the TS app:
`files.content.read`, `files.content.write`, `files.metadata.read`,
`files.metadata.write`, `sharing.read`, `sharing.write`, `account_info.read`.

**`qlib/DropboxDataProvider/`** — app name `Dropbox`, logo `dropbox-logo.svg`
(squared per §0.1), same structure and conformance rules as phase C.

**Action map:**

| Action | Endpoint | Host |
|---|---|---|
| `list-folder` | `/2/files/list_folder` (+ `/continue`) | RPC |
| `search` | `/2/files/search_v2` | RPC |
| `get-metadata` | `/2/files/get_metadata` | RPC |
| `download-file` | `/2/files/download` | **content** |
| `upload-file` | `/2/files/upload`, or `upload_session/start\|append_v2\|finish` | **content** |
| `create-text-file` | `/2/files/upload` | **content** |
| `create-folder` | `/2/files/create_folder_v2` | RPC |
| `delete-file` / `delete-folder` | `/2/files/delete_v2` | RPC |
| `copy-file` / `copy-folder` | `/2/files/copy_v2` | RPC |
| `move-file` / `move-folder` | `/2/files/move_v2` | RPC |
| `create-shared-link` | `/2/sharing/create_shared_link_with_settings` | RPC |
| `list-shared-links` | `/2/sharing/list_shared_links` | RPC |
| `revoke-shared-link` | `/2/sharing/revoke_shared_link` | RPC |
| `get-file-link` | `/2/files/get_temporary_link` | RPC |
| `list-file-revisions` | `/2/files/list_revisions` | RPC |
| `restore-file-revision` | `/2/files/restore` | RPC |

**Implement upload sessions** for files over the single-shot limit — absent from
the TS app. Events: `new-file`, `new-folder`, `file-modified` via
`/2/files/list_folder/longpoll` or cursor polling.

Register in `SchemeMap`, `FactoryMap`, `CMakeLists.txt`; add the tier 2 file API
registration; tests as in C.4.

---

## Phase F — tier 1b native handlers

**Status: done.** What landed, in order:

**Done — client injection.** `FileLocationHandlerHttp` and
`FileLocationHandlerRest` each take an optional already-authenticated client. A
bound handler treats the location as a URI path on the client's target rather
than as a URL, sends request headers per request rather than reconfiguring the
shared client, and advertises only the options that still apply
(`encoding`, `headers`, `method`) — every other option configures the
construction of a client, which a bound handler does not do. Bound handlers are
obtained through `FileLocationHandler::getHttpHandlerForClient()` /
`getRestHandlerForClient()`, since the concrete handler classes are
module-private. A bound REST handler also supports output streams (buffered and
sent on close), which an unbound one cannot.

**Done — generic REST connection.** `RestConnection::supportsFileLocationsImpl()`
returns `True` **only when the connection serves no data provider application**,
and `getFileLocationHandlerImpl()` returns a bound `FileLocationHandlerRest`.
The gate matters: tier 1b takes precedence over tier 2, so returning `True`
unconditionally would replace every application's file API with a bare `GET` on
the location path — Drive needs an opaque ID and `?alt=media`, Dropbox serves
content from another host with its arguments in a header, and S3 needs a bucket
as well as a key. A subclass that can genuinely transfer files natively
overrides both methods together.

**Done — tier 1b option validation.** Options are now validated against the
connection's own handler exactly as they are against a tier 1a redirect target;
they were previously passed through unchecked, because the `conn://` handler
declares that it validates its own options.

**Done — `DPFR_URL` pollers.** `FileLocationHandlerDataProvider` resolves the URL
with one blocking action call and returns the poller for the URL's own scheme, so
the transfer — the part with the volume — is non-blocking. It still throws
`UNIMPLEMENTED`, with a message saying why, for an application that returns the
data inline (nothing to poll) and when `follow_url` is `False` (the caller asked
for the URL, not its contents).

**Done — Dropbox native handler.** `DropboxFileLocationHandler` transfers through
the Dropbox content endpoints. Writing through an output stream drives an upload
session from the stream itself, sending each chunk as it is written, so a file
larger than memory can be written; content that ends within the first chunk is
sent in a single request instead, saving two round trips. Reads take the bytes
from the response body rather than base64-encoded into a JSON field.

**Done — S3 and Google Drive native handlers.** Both are the same shape as the
Dropbox one, and both are reached through their connection's
`supportsFileLocations()` / `getFileLocationHandler()`:

- `AwsS3RestConnection`: a `GET` on `/{bucket}/{key}` for reads, and a multipart
  upload (`CreateMultipartUpload` / `UploadPart` / `CompleteMultipartUpload`)
  driven from an input stream for writes. Each part is a separate signed request
  with a known body, so SigV4 needs no streaming-signature work — one part is
  buffered at a time, with the 5 MB minimum part size as the floor. The bucket
  comes from the `container` option or the first path segment.
- `GoogleDriveRestConnection`: `?alt=media` for reads, and a resumable upload
  (`POST ?uploadType=resumable`, then `PUT` with `Content-Range`) driven from an
  input stream for writes.

Both override `supportsFileLocationsImpl()` to `True` on their connections,
since the base class leaves an application connection to its application's file
API.  The streaming upload each one drives is also what the actions now use
above their size thresholds, closing design §5.3 — those thresholds had been
defined and never used, so every upload had been a single request whatever its
size.

**Done — event providers for all three modules.** `DPAT_EVENT` watch providers
replacing the TypeScript triggers now exist: S3 `new-bucket` /
`new-or-updated-file` (polling, since S3 has no client-subscribable change
feed), Drive `new-file` / `new-folder` over `changes.list`, and Dropbox
`new-file` / `new-folder` / `file-modified` over the `list_folder` cursor.
Each root provider carries an `events` container.

Both Drive and Dropbox report a creation and a modification identically, so
each keeps the state needed to tell them apart: Drive compares a change's
`createdTime` against the moment its cursor was established, and Dropbox keeps
a bounded index of the entries it has already seen, seeded by walking the
folder when the cursor is first established.

**Done — tests.** Tier 1b read, write, streams, and error propagation; tier 1b
takes precedence over tier 2 for a connection where both apply; options are
validated against the native handler's own set; a connection returning something
that is not a handler is rejected; bound handlers advertise the reduced option
set; `DPFR_URL` pollers and both `UNIMPLEMENTED` cases; and, for Dropbox,
single-request versus upload-session selection, chunk offsets, byte accounting,
and error propagation through `waitForIo()`.

---

## Phase G — post-migration tranches

Four tranches run against live `s3`, `googledrive`, and `dropbox` connections.
All four are complete.

**Tranche 1 — defects and cheap wins.** Drive `rename-file` (neither `move-file`
nor `replace-file` could change a name); Drive shared-drive support, added
centrally so a new action cannot silently omit `supportsAllDrives`; Dropbox
`get-account-info` and `get-space-usage`, which use the `account_info.read`
scope the connection had been requesting for nothing.

**Tranche 2 — event providers.** See phase F above.

**Tranche 5 — the rest of the plan.** The tier 1b native handlers for S3 and
Drive (phase F), the streaming uploads they are built on (§5.3), and a
repo-wide fix for `AbstractDataProvider::error()`, which throws rather than
logs and wins overload resolution: 62 call sites across 26 modules reported
failures by raising an exception from the code meant to record them, destroying
the real error on every retry-exhausted request and stopping four modules'
watch providers permanently on the first transient error.

**Tranche 3 — API surface.** S3 `presigned-url` (SigV4 query-string signing,
added to `AwsRestClient` where the credentials live), `delete-objects`,
`delete-bucket`, `move-object`, object tagging, `list-object-versions`, and
server-side encryption options.  Drive `list-shared-drives`,
`get-storage-quota`, `list-permissions`, `remove-permission`, `untrash`,
`create-shortcut`, and the revisions API.  Dropbox `save-url`, batch
delete/move/copy with `check-job-status`, `permanently-delete`, thumbnails and
previews, sharing with named people, and search continuation.

**Tranche 4 — record-based listing.** `list-objects`, `list-files`,
`list-folder`, and Dropbox `search` are record sources as well as API actions,
with iterators that fetch each page as it is reached; the `DPAT_FIND` actions
take their options from the providers' own `SearchOptions`.

### What live testing found that offline testing had not

Every one of these passed a full offline suite first:

| Defect | Consequence |
|---|---|
| Drive multipart upload was serialized into the client's data format | no upload to Drive had ever worked |
| `ex.arg` is a `hash<RestResponseInfo>`; probing an undeclared key throws | every failed Drive/Dropbox request reported `INVALID-MEMBER` instead of the API error |
| `AbstractDataProvider::error()` throws rather than logs, and wins overload resolution | a watch provider died permanently on its first transient error |
| `hash<auto>` literals narrow to their value type | S3 `list-objects` with a `prefix`, and Drive `create-folder` / `upload-file` / `create-text-file` with a folder, all raised `RUNTIME-TYPE-ERROR` |
| a missing key in a `hash<string, bool>` cannot be assigned to a `bool` | the Dropbox watches delivered nothing at all |
| `data` was applied as an override rather than a default | **every object written to S3 was stored as a quoted base64 string** |
| the request target is percent-encoded again on the way out | S3 pagination and any key needing encoding failed as `SignatureDoesNotMatch` |
| Dropbox names the `search_v2` continuation `/2/files/search/continue_v2` | the continuation answered with an HTML 404 page |
| S3 answers `CreateMultipartUpload` with XML and no `Content-Type` | the REST client refused to deserialize it, so no multipart upload could be opened |
| the REST client returns response headers at the **top level**, not under `hdr` | every header the S3 module read was `NOTHING`: an object's MIME type, size, ETag, modification time and version ID, and the ETag and version ID of every write. Nothing failed visibly, because `NOTHING` is valid for each — only the multipart upload noticed, being the one place that checks |
| Drive answers an intermediate resumable chunk with `308 Resume Incomplete`, which is `>= 300` | **no Drive upload larger than one chunk could complete**: the chunk was stored and then reported as a failure |

Two of these were only reachable because the offline suite was itself wrong: the
S3 fake overrode `doRequest()` but not `doRequestAssumingXml()`, so the
"offline" suite was silently making real requests against Amazon, and the Drive
fake answered an intermediate chunk with a `200` and no `Range` header — a
response Drive never sends. Both fakes now answer as the services do.

### Still outstanding

Nothing.  The two items that were outstanding are done:

- ~~**`AwsS3DataProvider` still uses the blocking client**~~ (design §5.5).
  Done.  The module is built on `AwsRestClientIo` throughout: 28 type
  references, `doS3Request()` on `restDoRequestAssumingXml()`,
  `cancelPollOperation()` on `close()` rather than `disconnect()`, and both
  `AwsS3RestConnectionBase::getDataProvider()` and
  `getFileLocationHandlerImpl()` handing out `getAsync()`.  The response shape
  is reconciled in `normalizeResponse()` alone, as predicted.
  `getPresignedUrl()` was hoisted rather than duplicated: the credential and
  scope members moved from each client's `private:internal` block into
  `AwsRestClientBase`, with two small abstract hooks
  (`getAwsHostHeaderValue()`, `getAwsBaseUrl()`) for the parts the two clients
  derive differently, so query-string signing now exists once.
- ~~**Qore `HTTPClient` re-encodes the request target.**~~  Resolved, and the
  question it raised for the async path is now settled empirically rather than
  by reading the code: the `RestClientIo` transport sends the request target
  **verbatim**.  There is no `getMsgPath()`-style rewrite between the caller's
  path and the HTTP/1 request line or the HTTP/2 and HTTP/3 `:path`
  pseudo-header, so a pre-encoded path reaches the wire unchanged and
  `pre_encoded_urls` needs no counterpart there.  Confirmed against Amazon: a
  signed `GET` on a key containing a space, a `+` and an `&` returns 200
  through `AwsRestClientIo`.  `RestConnection::getAsyncImpl()` now records the
  mechanism rather than merely asserting the options "do not carry over".

### Defects this work turned up

Each was found by doing the migration rather than by a test failing, and each
is fixed here:

| Defect | Consequence |
|---|---|
| `AwsRestClientIo` never signed asynchronous requests | `signRequest()` was only called from `restDoRequest()`; every async path — `restDoRequestAsync()` and anything built on `RestClientIoAsyncRequestOperation` — sent AWS requests with no `authorization` header.  The generic single-header `signer` hook cannot serve, since SigV4 sets four headers that sign each other; fixed by overriding `prepareAsyncRequestAttempt()` |
| No caller-driven poll operation existed on `RestClientIo` | `RestClientIoAsyncRequestOperation` submits itself to the async I/O controller, so a second driver calling `continuePoll()` would race it, and an error only rejected the Promise — a caller looping on `goalReached()` would never stop.  Added `startPollRequest()` / `startPollRawRequest()` and a caller-driven mode that rethrows and exposes the response through `getOutput()` |
| A retry started inside `continuePoll()` re-checked the OAuth2 token | `ensureToken()` can refresh synchronously, and a sync socket operation on an async-I/O `continuePoll` worker raises `SOCKET-SYNC-ON-IO-THREAD-ERROR`.  Added `startPollRetry()` / `startPollRawRetry()`, used for every request a poll operation issues after its first |
| `now_us().durationMicroseconds()` is always `0` | An absolute date has no duration, so ten live tests across the three suites that believed they were generating a unique object key, file name, or folder per run were reusing one fixed name.  Replaced with `clock_getmicros()` |

### Poller coverage

Every polled read is exercised **offline** as well as live.  Each module's fake
client overrides the poll entry points (`startPollRequest()` /
`startPollRetry()`, and `startPollContentRequest()` /
`startPollContentRetry()` for Dropbox) and answers from a canned response
without a network, so the offline suites drive a real poller to completion and
cover what a live test cannot reach on demand: a transient socket error
mid-transfer, the retry limit, and the service's own error document being
unpacked.  Without this the fakes could only be asked to *build* a poller, and
a change to the poll path would have passed all three offline suites — the
failure mode this branch's phase-G table is a list of.

To make that possible, `RestClientIo`'s `startPoll*` methods return
`AbstractPollOperation` rather than the concrete operation class.  That is the
whole contract a caller needs, and it is what lets a subclass answer requests
from a script instead of from the network.

The fakes model the transport honestly in two respects that each caught a bug
while being written: nothing is sent until the poller is driven (a real client
prepares the request and sends on the first `continuePoll()`), and an attempt
that fails does **not** consume a canned response, since an attempt that dies
on the socket never received one.

### Non-blocking reads

All three native file location handlers implement
`getIoPollerForLocationImpl()`; none throws `UNIMPLEMENTED` any longer, so a
`conn://` location naming an `s3`, `googledrive`, or `dropbox` connection can
be read with non-blocking I/O — which is what design §5.5 exists to guarantee.

Each poller retries a transient I/O error and reports a failure in its
service's own terms (`S3-API-ERROR`, `GOOGLE-DRIVE-ERROR`, `DROPBOX-ERROR`)
rather than as a bare HTTP status, matching the blocking read it replaces.  The
Drive poller keeps the two-request shape of the blocking read — metadata, then
content — so that a Google-native document is still reported precisely instead
of as the API's generic *"Only files with binary content can be downloaded"*;
naming `export_mime_type` makes it a single request.  S3 signs the polled
request with the same code that signs every other request the module makes, so
a polled read and a blocking read put an identical signature on the wire.

---

## Sequencing

| Phase | Depends on | Deliverable | Shippable alone |
|---|---|---|---|
| A | — | `conn://` + tier 1a | yes |
| B | — | Dropbox transport answer (gate) | n/a — spike |
| C | A (for tier-2 readiness only) | `AwsS3DataProvider`, `GoogleDriveDataProvider` | yes |
| D | A, C | Tier 2 declarative file API + registrations | yes |
| E | B (gate), C | `DropboxRestClient` + `DropboxDataProvider` | yes |
| F | C, D, E | Tier 1b streaming handlers + `DPFR_URL` pollers | yes |

A and B are independent and can run concurrently. C does not strictly require A,
but running A first means D lands against a settled framework.

---

## Out of scope on this branch

- **Retiring the TS apps.** `apps/amazon-s3`, `apps/google-drive`, and
  `apps/dropbox` live in the separate `module-v8` repository and are removed by
  a change tracked there. The cutover conflict is two implementations
  registering the same app name — which is exactly why §0.1 keeps the names
  identical rather than inventing new ones.
- **`ts-toolkit` `files: {...}` key** for the ~80 apps staying in TypeScript.
- **Directory listing through a connection** (`FilePoller`-style). `conn://`
  covers read and write of a single file only.
- **Non-blocking reads for `ftp://` and `sftp://`.**  Both tier-1a handlers
  still throw `UNIMPLEMENTED` from `getIoPollerForLocationImpl()`, and neither
  can be fixed in the handler: SFTP has no async API at all in `module-ssh2`,
  and FTP's composite `FtpClientIo::FtpClientPollOperation` does not work —
  its suite covers only the two primitive poll operations, never a composite
  GET, so the state machine shipped broken.  One defect in it is fixed here
  (the data connection is now opened before the transfer command, since a
  server will not answer `RETR` until the connection it advertised is
  accepted); at least one more remains.  Written up as
  `/tmp/qore-5386-ftp-poller-prompt.md` and
  `/tmp/qore-5386-sftp-poller-prompt.md`.
- **Migrating the other Amazon apps** (`ses`, `sns`, `sqs`, `lambda`,
  `cloudfront`, `cloudwatch`, `ec2`) onto `AwsRestClient`. If that is ever
  wanted it argues for building `AwsS3DataProvider` on shared AWS base classes
  rather than S3-specific ones — worth a moment's thought during C.1, not worth
  building for speculatively.

---

## Decisions needed before coding starts

1. **Scheme name** — `conn://` with `connection://` as alias. Must be settled
   before phase A ships; location strings become persisted configuration.
2. **S3 container placement** — bucket as first path segment
   (`conn://s3/bucket/key`) vs option (`conn://s3/key{container=bucket}`). The
   design supports both with the connection's configured bucket as default; if
   only one is wanted, the option form is less ambiguous.
3. **`AwsS3RestConnection` subclass vs. plain `AwsRestConnection`** with
   configuration by convention. Plan assumes the subclass.
4. **Google Drive scope granularity** — fixed full `drive` vs selectable
   `drive.file`. Recommend selectable (C.2).
5. **`SftpConnection` redirect** — land it in `module-ssh2` alongside phase A,
   or defer and ship phase A without SFTP coverage.
