# Implementation Plan — issue #5386, `feature/5386_file_app_improvements`

**Issue:** [#5386](https://github.com/qoretechnologies/qore/issues/5386)
**Branch:** `feature/5386_file_app_improvements`

**Design documents:**
- [`file-location-handler-connection-scheme.md`](file-location-handler-connection-scheme.md) — the `conn://` scheme
- [`cloud-storage-qlib-modules.md`](cloud-storage-qlib-modules.md) — native S3 / Google Drive / Dropbox modules

**Normative references** (every phase below conforms to these; deviations are
called out explicitly):
- `design/qore-module-structure.md`
- `design/data-provider-development-guide.md`
- `design/data-provider-checklist.md`
- `design/data-provider-semantic-string-types.md`

---

## 0. Ground rules for the whole branch

### 0.1 Identity is preserved

App names and icons carry over from the v8/TS apps **unchanged** — the native
modules are the same apps, reimplemented:

| TS app | App name (`AppName` const) | Logo file | Scheme |
|---|---|---|---|
| `apps/amazon-s3` | `AmazonS3` | `amazon-s3-logo.svg` | `s3` |
| `apps/google-drive` | `GoogleDrive` | `google-drive-logo.svg` | `gdrive` |
| `apps/dropbox` | `Dropbox` | `dropbox-logo.svg` | `dropbox` |

Logos are extracted by base64-decoding the `*_APP_LOGO` constants in each TS
app's `constants.ts` and writing the SVG to the module directory.

**One correction is required during extraction:** the checklist mandates square
icons, and the current Dropbox SVG is `width="1200" height="800"` with
`viewBox="-35.3175 -50 306.085 300"`. Normalize each extracted logo to square
dimensions rather than copying the TS asset verbatim.

Per `design/data-provider-checklist.md` §App Icon Convention, the logo is a
separate file loaded at module level — never an inlined string constant:

```qore
public const AmazonS3Logo = File::readTextFile(get_script_dir() + "/amazon-s3-logo.svg");
```

following `qlib/OneDriveDataProvider/OneDriveDataProviderDefs.qc:31`.

### 0.2 Registration is a three-place operation

Every new module must be registered in all three, or it fails silently:

1. `qlib/ConnectionProvider/ConnectionSchemeCache.qc` → `SchemeMap` — scheme → REST client module.
   Missing ⇒ every connection becomes `(InvalidConnection)` and disappears from
   the action picker.
2. `qlib/DataProvider/DataProvider.qc` → `FactoryMap` — scheme → data provider module.
   Missing ⇒ the module loads but the app never appears in the apps list.
3. `CMakeLists.txt` → one `qore_user_module()` call.

### 0.3 `qore_user_module()` second argument — reference-doc correction

`design/qore-module-structure.md` §1 states that the second argument is a
semicolon-separated dependency list, and the `QORE_USER_MODULE` header comment
in `cmake/QoreMacros.cmake:2478-2482` says the same. **Both are stale.** The
macro body (`cmake/QoreMacros.cmake:2498-2500`) reads:

```cmake
# any args after the module path are extra resource files (logo SVGs,
# asyncapi YAMLs, etc.) installed alongside the module
set (_extra_files ${ARGN})
```

and inter-module build-order dependencies are derived automatically from each
module's own `%requires` directives in
`QORE_FINALIZE_USER_MODULE_DEPENDENCIES`. The correct form for these modules,
matching `CMakeLists.txt:3564` and `:3572`, is therefore:

```cmake
qore_user_module("qlib/AwsS3DataProvider" "amazon-s3-logo.svg")
```

**Action item (phase A):** fix the stale text in
`design/qore-module-structure.md` and the stale macro header comment in
`cmake/QoreMacros.cmake` in the same pass. This is a one-line-each
documentation fix and belongs on this branch since the plan depends on it being
right.

### 0.4 Per-phase definition of done

No phase is complete until all of:

- `cmake --build build --target <Module>-qmod` succeeds for every touched qlib
  module. **A stale `.qmod` silently shadows edited `.qm`/`.qc` sources — the
  edited code simply never runs, with no error.**
- `./run_tests.sh -d qlib/<Module>` passes with no warnings and no errors, run
  under `qore --enable-debug`.
- The relevant boxes in `design/data-provider-checklist.md` are ticked.
- Docs build: `cmake --build build --target docs` (doc tables use the Qore pipe
  format, never Markdown — `QORE_DOX_TABLE_STRICT` fails the build otherwise).
- No valgrind run is required: there are no C++ changes anywhere in this branch.

---

## Phase A — `conn://` core + tier 1a redirect

Self-contained and shippable alone. Nothing later depends on being able to reach
a SaaS file store.

### A.1 Core framework change

`qlib/FileLocationHandler/FileLocationHandler.qc`:

- Add to `AbstractFileLocationHandler`:
  ```qore
  #! Returns True if the handler validates its own options
  bool selfValidatesOptions() { return selfValidatesOptionsImpl(); }
  private bool selfValidatesOptionsImpl() { return False; }
  ```
- In `FileLocationHandler::getInfo()` (`FileLocationHandler.qc:498-506`), skip
  the unknown-option check when `handler.selfValidatesOptions()` is `True`.
  Defaults and required-option processing are unaffected.
- Register the new schemes in `FileLocationHandler::init()`:
  ```qore
  FileLocationHandler::registerHandler("conn", new FileLocationHandlerConnection());
  FileLocationHandler::registerHandler("connection", new FileLocationHandlerConnection());
  ```

Default `False` means no existing handler changes behavior — assert this in
tests rather than assuming it.

### A.2 New handler

`qlib/FileLocationHandler/FileLocationHandlerConnection.qc`:

- Parse `<connection-name>/<path>`; name is everything before the first `/`.
- Resolve lazily — **no `%requires ConnectionProvider`** (see design §6):
  ```qore
  load_module("ConnectionProvider");
  object conn = call_function("ConnectionProvider::get_connection", name);
  ```
  held in an `object`, exactly as `FileLocationHandlerSftp::getSftpClient()`
  does with `ssh2` (`FileLocationHandlerSftp.qc:245-252`).
- Tier 1a: obtain `hash<FileLocationRedirect>{scheme, location, opts}` and
  re-enter dispatch for `redirect.scheme`.
- Confine the path: resolve the join and reject any result outside the
  connection root, **before** any I/O.
- Every error message and log line uses `getSafeUrl()`, never `getUrl()`.
- Declare handler options: `encoding`, `subtype`, `container`, `path_is_id`,
  `follow_url`, `conn_rtopts`; return `True` from `selfValidatesOptionsImpl()`
  and re-validate against the resolved delegate, naming the actual target scheme
  in the error.

`qlib/FileLocationHandler/FileLocationHandler.qm`: add the new class to the
mainpage class list and a release-notes subsection. Module version → 3.1.

### A.3 Connection-side additions

`qlib/ConnectionProvider/AbstractConnection.qc`:

```qore
const string CF_FILE_LOCATIONS = "file-locations";

bool supportsFileLocations()             { return supportsFileLocationsImpl(); }
object getFileLocationHandler()          { return getFileLocationHandlerImpl(); }
*hash<FileLocationRedirect> getFileLocationRedirect(string path, bool write) {
    return getFileLocationRedirectImpl(path, write);
}

private bool supportsFileLocationsImpl() { return False; }
private object getFileLocationHandlerImpl() {
    throw "FILE-LOCATION-UNSUPPORTED", ...;
}
private *hash<FileLocationRedirect> getFileLocationRedirectImpl(string path, bool write) {}
```

`getFileLocationHandler()` returns `object`, not
`FileLocationHandler::AbstractFileLocationHandler` — deliberately, to keep the
dependency edge from forming. Comment this at the declaration so it is not
"tidied up" later.

Implement `getFileLocationRedirectImpl()` in:
- `qlib/ConnectionProvider/FilesystemConnection.qc` → `file://<root>/<path>`
- `qlib/ConnectionProvider/FtpConnection.qc` → `ftp(s)://...`
- `qlib/ConnectionProvider/HttpConnection.qc` → `http(s)://...`
- `SftpConnection` in `module-ssh2` → `sftp://...` **(separate repo; if it
  cannot land in step, `conn://` over SFTP falls back to tier 3 with a clear
  error, and the qore-side tests cover filesystem/FTP/HTTP only)**

Add the filter hook to `FileLocationHandler`:
```qore
FileLocationHandler::setConnectionFilter(*code<bool(string conn_name, bool write)> filter);
```
with `QORE_FILE_LOCATION_CONNECTIONS` as the no-code default (unset = all
allowed).

`ConnectionProvider.qm` version → 3.2 + release-notes entry.

### A.4 Tests — `examples/test/qlib/FileLocationHandler/`

Extend the existing `FileLocationHandler.qtest` or add
`FileLocationHandlerConnection.qtest` (exec bit set, `%modern`,
`%prepend-module-path` for local modules, `TmpDir`/`TmpFile` from `FsUtil`):

- Parsing: name/path split; empty path; `{}` options; path containing `{`; name
  with `.`/`-`; `connection://` alias.
- Tier 1a round trip over a stub connection-provider module registering a
  `FilesystemConnection` on a `TmpDir`: text and binary read/write, stream
  reader, output stream, `append`, encoding conversion.
- **Path confinement (negative):** `conn://c/../../etc/passwd` throws and
  touches nothing.
- **Credential-leak assertions:** force each failure path on a connection whose
  URL contains a password; assert it appears in neither `ex.desc`, `ex.arg`, nor
  the log.
- Negative: unknown connection; `QORE_CONNECTION_PROVIDERS` unset; connection
  with no file support; unknown option (error names the *resolved target*
  scheme).
- Filter hook: allowed/denied, read and write independently.
- Regression: `selfValidatesOptions()` defaults `False` and every pre-existing
  scheme still rejects unknown options identically.

### A.5 Reference-doc fixes

Per §0.3: correct the second-argument description in
`design/qore-module-structure.md` §1 and the `QORE_USER_MODULE` header comment
in `cmake/QoreMacros.cmake`.

---

## Phase B — Dropbox transport spike (gate)

**Timeboxed prototype, not production code.** Throwaway script under the
scratchpad, not committed.

Dropbox splits its API across two hosts with two conventions:

| | RPC | Content |
|---|---|---|
| Host | `api.dropboxapi.com` | `content.dropboxapi.com` |
| Arguments | JSON request body | `Dropbox-API-Arg` header, JSON-encoded |
| Body | JSON | raw binary |

**Question to answer:** can `RestClient` / `RestClientIo` cleanly issue a request
with JSON-encoded arguments in a header and a raw binary body — and stream both
directions — without fighting the client's serialization layer?

**Gate:** if yes, phase E proceeds as planned. If it needs client-layer changes,
Dropbox is deferred and phases C/D/F proceed without it; record the finding in
`cloud-storage-qlib-modules.md` §4.3 either way.

Doing this before C is deliberate: it is the only unknown that can change the
shape of later phases, and it is cheap to answer.

---

## Phase C — `AwsS3DataProvider` + `GoogleDriveDataProvider`

The two modules whose client infrastructure already exists. Both follow the
`qlib/OneDriveDataProvider/` layout and
`design/data-provider-development-guide.md` §File Structure.

### C.1 `AwsS3DataProvider`

**Client:** no new client module. Add a thin `AwsS3RestConnection` to
`qlib/AwsRestClient.qm` pinning `aws_service = "s3"` and `aws_s3 = True`, with
an S3 ping and its own `required_options` — mirroring how
`GoogleCalendarRestConnection` pins `api_profile`
(`GoogleRestClient.qm:409-500`). Scheme `s3`.

**Files:**
```
qlib/AwsS3DataProvider/
    AwsS3DataProvider.qm            # module decl; registerFactory, registerApp, registerAction
    AwsS3DataProviderDefs.qc        # AppName = "AmazonS3", AmazonS3Logo
    AwsS3DataProviderBase.qc        # *RestClientIo rest; ConstructorOptions; RetrySet; MaxIoRetries
    AwsS3DataProvider.qc            # root provider + ChildMap
    AwsS3ListBucketsDataProvider.qc
    AwsS3CreateBucketDataProvider.qc
    AwsS3ListObjectsDataProvider.qc # record-based, ContinuationToken pagination
    AwsS3GetObjectDataProvider.qc
    AwsS3DownloadFileDataProvider.qc
    AwsS3UploadFileDataProvider.qc  # multipart for large objects
    AwsS3DeleteObjectDataProvider.qc
    AwsS3CopyObjectDataProvider.qc
    AwsS3WatchObjectsDataProvider.qc # DPAT_EVENT — replaces the TS triggers
    AwsS3DataTypes.qc
    amazon-s3-logo.svg
```

**New surface previously handled by the AWS SDK** — budget for it explicitly:
- virtual-host-style (`<bucket>.s3.<region>.amazonaws.com`) vs path-style
  addressing, and when each is required
- region redirects: `301 PermanentRedirect` / `307` with `x-amz-bucket-region`,
  and **re-signing after redirect**
- XML `ListObjectsV2` parsing with `ContinuationToken` pagination
- multipart upload (`CreateMultipartUpload` / `UploadPart` / `Complete`) and
  multipart ETag semantics
- `Content-MD5` / `x-amz-checksum-*` where required

`AwsRestClient` already handles the SigV4 core, including the S3-specific rule
that S3 URIs are encoded **once** and paths are **not** normalized, unlike every
other AWS service (`AwsRestClient.qm:945-965`) — do not reimplement this.

### C.2 `GoogleDriveDataProvider`

**Client:** extend `qlib/GoogleRestClient.qm`:

```qore
"drive": {
    "oauth2_scopes": ("https://www.googleapis.com/auth/drive",),
    "ping_method": DefaultGooglePingMethod,
    "ping_path": "/drive/v3/about?fields=user",
    "ping_headers": DefaultGooglePingHeaders,
},
```

added to `GoogleRestClientBase::ApiProfiles`, plus a `GoogleDriveRestConnection`
pinning `api_profile = "drive"`. Scheme `gdrive`.

**Files:** same shape as C.1, under `qlib/GoogleDriveDataProvider/`, app name
`GoogleDrive`, logo `google-drive-logo.svg`.

**This phase closes the content-read gap.** The TS `get-file-by-id` returns
metadata only. Implement:
- `download-file` → `GET /drive/v3/files/{fileId}?alt=media`
- export of Google-native documents → `/export?mimeType=...`
- resumable upload (`uploadType=resumable`) for large files
- record-based `list-files` with real `pageToken` iteration, replacing the
  capped TS list action

**Open question 5 from the design doc** should be settled here: whether the
scope set is fixed at full `drive` or made profile-selectable so deployments can
use the much narrower `drive.file`. Recommend making it selectable now — it is
far cheaper than changing it after release.

### C.3 Conformance for both

Per `design/data-provider-checklist.md`:

- **Action path resolution:** pick one layout (flat or nested `ChildMap`) and
  keep it consistent. Verify mechanically — every action's first path segment
  must appear in the root's `ChildMap`, or the provider is silently `null`,
  dropdowns come back empty, and `doRequestImpl()` is never reached.
- `short_desc` plain text under 80 chars; `desc` markdown.
- `groups`: `(DataProvider::AppGroup::CloudStorage,)`.
- Every `DPAT_API` action needs both `options` (via
  `DataProviderActionCatalog::getActionOptionFromFields()`) **and**
  `output_type`, or the action renders with no form fields and is unusable.
- Single-key hash slices need the trailing comma — `Fields{"key",}`. Without it
  you get the value, not a hash, and an `OPTION-ERROR` at module load.
- `getRecordTypeImpl()` returns `*hash<string, AbstractDataField>` — call
  `.getFields()`; returning the type object causes `RUNTIME-TYPE-ERROR`.
- All date/time fields use `DateType`/`DateOrNothingType` regardless of wire
  format; convert inside `doRequestImpl()`.
- `allowed_values` declared explicitly in the option definition, never described
  as prose in `desc`.
- **Allowed-values callbacks** replacing the TS `get_allowed_values` helpers
  (bucket lists, folder pickers, file pickers). Skipping these is a silent UI
  regression and is the single easiest thing to forget in this phase.
- `RestClientIo` throughout, so tier 1b pollers work in phase F.

### C.4 Registration and docs (both modules)

- `ConnectionSchemeCache::SchemeMap`: `"s3": "AwsRestClient"`,
  `"gdrive": "GoogleRestClient"`.
- `DataProvider.qc` `FactoryMap`: `"s3": "AwsS3DataProvider"`,
  `"gdrive": "GoogleDriveDataProvider"`.
- `CMakeLists.txt`: `qore_user_module("qlib/AwsS3DataProvider" "amazon-s3-logo.svg")`
  and `qore_user_module("qlib/GoogleDriveDataProvider" "google-drive-logo.svg")`.
- Mainpage `@section awss3dataproviderintro` / `@section googledrivedataproviderintro`
  as the first section; all `@code` blocks use `@code{.py}`.
- Update `doxygen/lang/120_modules.dox.tmpl` and
  `doxygen/lang/900_release_notes.dox.tmpl`.
- Grep every `<scheme>://` literal in each module and confirm it matches a
  registered scheme — a typo makes every connection `(InvalidConnection)`.

### C.5 Tests

`examples/test/qlib/AwsS3DataProvider/AwsS3DataProvider.qtest` and
`examples/test/qlib/GoogleDriveDataProvider/GoogleDriveDataProvider.qtest`,
exec bit set, `%modern`, hard `%requires` (both are in-repo modules — no
`%try-module`).

Live credentials will not be present in CI, so structure each suite as:
offline tests that always run (registration, app/action catalog shape, request
construction, SigV4 signature vectors against known-good fixtures, XML/JSON
response parsing from captured fixtures, type validation) plus integration tests
gated on an explicit env var. Follow the existing
`examples/test/qlib/AwsRestClient/` precedent. Per
`feedback_explicit_capability_over_catch`: gate on an explicit capability check,
never a catch-all skip that would hide a real regression.

---

## Phase D — `conn://` tier 2 declarative file API

### D.1 DataProvider additions

`qlib/DataProvider/DataProviderActionCatalog.qc`:

- `DPFR_DATA` / `DPFR_URL` read-mode constants
- `DPFW_FILE` / `DPFW_BASE64` / `DPFW_RAW` write-format constants
- `public hashdecl DataProviderFileApiInfo` (fields as specified in the design
  doc §4 tier 2)
- `registerAppFileApi(string app, hash<DataProviderFileApiInfo> info)` and
  `*hash<DataProviderFileApiInfo> getAppFileApi(string app)`, guarded by the
  existing catalog lock and participating in `ImplicitModuleTransaction`
  deregistration like `registerScheme()` does

### D.2 Tier 2 dispatch

In `FileLocationHandlerConnection`: app lookup → `getAppActionEx()` →
`conn.getDataProvider(subtype)` → `getChildProviderPath(action.path)` →
`doRequest()` → extract and decode, or follow the URL for `DPFR_URL`.

Streams are synthesized with the `StreamPipe` + background pattern already used
by the FTP/SFTP handlers, **including the `OutputStreamWrapper` + `WaitGroup`
error-propagation discipline**: `on_exit io_counter.done()` outside the `try`, so
`waitForIo()` observes a recorded error before the wait group drains
(`FileLocationHandlerSftp.qc:199-212`). `getIoPollerForLocation()` throws
`UNIMPLEMENTED` for tier 2, matching the SFTP precedent.

### D.3 Registrations

`FileDataProvider` (`get`/`create`, `path`, `data`), `FtpClientDataProvider`
(`get-file`/`send-file`), `OneDriveDataProvider` (`download-file` with
`DPFR_URL` on `download_url`, `upload-file`), plus the two phase-C modules.

Because phase C standardized the vocabulary (design §5.1), these registrations
should each be a few lines.

### D.4 Tests

Stub app + data provider registering a `DataProviderFileApiInfo`, exercising
`DPFR_DATA`/base64, `DPFR_DATA`/raw, `DPFR_URL` with `follow_url` both ways,
container options, and `path_is_id`. Negative: declared read action does not
exist; response lacks `read_data_field`; write attempted with no `write_action`.

---

## Phase E — `DropboxRestClient` + `DropboxDataProvider`

Conditional on the phase B gate.

- `qlib/DropboxRestClient.qm` — client + `DropboxRestConnection`, templated on
  `qlib/OneDriveRestClient.qm` (OAuth2 `authorization_code`,
  `token_access_type: offline`). Must expose both the JSON-RPC path and the
  content-transfer path (arguments in `Dropbox-API-Arg`, raw binary body).
  Scheme `dropbox`. Scopes as in the TS app: `files.content.read/write`,
  `files.metadata.read/write`, `sharing.read/write`, `account_info.read`.
- `qlib/DropboxDataProvider/` — app name `Dropbox`, logo `dropbox-logo.svg`
  (squared per §0.1), same structure and conformance rules as phase C.
- **Implement upload sessions** (`/2/files/upload_session/start|append_v2|finish`)
  for files over the single-shot limit — absent from the TS app.
- Event provider replacing the `new-file` / `new-folder` / `file-modified`
  triggers (`/2/files/list_folder/longpoll` or cursor polling).
- Register in `SchemeMap`, `FactoryMap`, `CMakeLists.txt`; tier 2 file API
  registration; tests as in C.5.

---

## Phase F — tier 1b native handlers

- Refactor `FileLocationHandlerHttp` and `FileLocationHandlerRest` to accept an
  injected, already-authenticated client object, so a connection's handler is a
  short subclass rather than a reimplementation.
- Implement `supportsFileLocationsImpl()` / `getFileLocationHandlerImpl()` on
  `AwsS3RestConnection`, `GoogleDriveRestConnection`, `DropboxRestConnection`,
  and the generic REST connection — giving real streaming reads and writes
  (S3 multipart, Drive resumable, Dropbox upload sessions) instead of
  request/response action dispatch.
- Add poller support for `DPFR_URL` reads: resolve the URL with a blocking
  action call, then return the HTTP handler's poller for it.
- Tests: assert that a tier 1b connection streams without buffering the whole
  object, and that tier 1b takes precedence over tier 2 when both are available.

---

## Sequencing summary

| Phase | Depends on | Deliverable | Shippable alone |
|---|---|---|---|
| A | — | `conn://` + tier 1a; filesystem/FTP/HTTP(/SFTP) connections usable as file locations | yes |
| B | — | Dropbox transport answer (gate) | n/a — spike |
| C | A (for tier-2 readiness only) | `AwsS3DataProvider`, `GoogleDriveDataProvider` | yes |
| D | A, C | Tier 2 declarative file API + registrations | yes |
| E | B (gate), C | `DropboxRestClient` + `DropboxDataProvider` | yes |
| F | C, D, E | Tier 1b streaming handlers + `DPFR_URL` pollers | yes |

A and B are independent and can run concurrently. C does not strictly require A,
but running A first means the tier-2 work in D lands against a settled framework.

---

## Out of scope on this branch

- **Retiring the TS apps.** `apps/amazon-s3`, `apps/google-drive`, and
  `apps/dropbox` live in the separate `module-v8` repository and are removed by
  a change tracked there. The conflict to resolve at cutover is two
  implementations registering the same app name (`AmazonS3`, `GoogleDrive`,
  `Dropbox`) in the action catalog — which is precisely why §0.1 keeps the names
  identical rather than inventing new ones.
- **`ts-toolkit` `files: {...}` key** for the ~80 apps staying in TypeScript —
  optional follow-on, `conn://` design phase 5.
- **Directory listing through a connection** (`FilePoller`-style). `conn://`
  covers read and write of a single file only.
- **Migrating the other Amazon apps** (`ses`, `sns`, `sqs`, `lambda`,
  `cloudfront`, `cloudwatch`, `ec2`) onto `AwsRestClient`. If that is ever
  wanted, it argues for building `AwsS3DataProvider` on shared AWS base classes
  rather than S3-specific ones — worth a moment's thought during C.1 design, not
  worth building for speculatively.

---

## Open decisions needed before coding starts

1. **Scheme name** — `conn://` with `connection://` as alias, per the design.
   Must be settled before phase A ships, since location strings become persisted
   configuration.
2. **S3 container placement** — bucket as first path segment
   (`conn://s3/bucket/key`) vs option (`conn://s3/key{container=bucket}`). The
   design supports both with the connection's configured bucket as default; if
   only one is wanted, the option form is less ambiguous.
3. **`AwsS3RestConnection` subclass vs. plain `AwsRestConnection`** with
   configuration by convention. Plan assumes the subclass.
4. **Google Drive scope granularity** — fixed full `drive` vs selectable
   `drive.file`. Recommend selectable (see C.2).
