# Migrating Amazon S3, Google Drive, and Dropbox to native qlib modules

**Status:** Design proposal. Not implemented.
**Issue:** [#5386](https://github.com/qoretechnologies/qore/issues/5386).
**Branch:** `feature/5386_file_app_improvements`.

**Goal:** move the `amazon-s3`, `google-drive`, and `dropbox` apps out of the
v8/TypeScript app catalog (`module-v8/ts/src/apps/`) and into the Qore repo as
native qlib REST client + data provider modules, redesigning them in the process.

**Companion:** [`file-location-handler-connection-scheme.md`](file-location-handler-connection-scheme.md) —
the `conn://` named-connection file location scheme. These two proposals are
mutually reinforcing and should be sequenced together; see §7.

**Precondition:** none of the three apps has been released, so there is no
backward-compatibility burden. Action names, option names, response shapes, and
OAuth2 scopes are all free to change.

---

## 1. Why migrate

### 1.1 The infrastructure already exists in qlib

The single strongest argument: for two of the three, the hard part is already
written and tested in this repo.

**Amazon S3.** `qlib/AwsRestClient.qm` (v1.3) already implements AWS SigV4 with
explicit S3 support:
- an `aws_s3` option flag on both `AwsRestClient` and `AwsRestConnection`
- the S3-specific canonical-request rule — S3 URIs are URI-encoded **once**,
  every other AWS service **twice**, and S3 paths are *not* normalized
  (`AwsRestClient.qm:945-965`)
- XML response body handling with no external XML module dependency
  (`AwsRestClientBase::parseAssumeRoleResponse()`, `AwsRestClient.qm:297-320`)
- STS `AssumeRole` credential chaining
- an existing test suite at `examples/test/qlib/AwsRestClient/`

An `AwsS3DataProvider` module is therefore a *data provider layer over an already
working client*, not a from-scratch S3 implementation.

**Google Drive.** `qlib/GoogleRestClient.qm` has an `api_profile` mechanism —
`GoogleRestClientBase::ApiProfiles` currently holds `none`, `calendar`, and
`gmail`, each supplying OAuth2 scopes and ping configuration — plus the
`GoogleCalendarRestConnection` and `GmailRestConnection` subclasses that pin a
profile. Adding a `drive` profile and a `GoogleDriveRestConnection` is a
mechanical extension of an established pattern, and
`qlib/GoogleCalendarDataProvider/` is a complete structural template.

**Dropbox** is the only one needing a genuinely new client module; see §4.3.

### 1.2 The TS implementations have real functional gaps

These are not hypothetical — they were found while designing `conn://`:

- **Google Drive cannot read file content at all.** `get-file-by-id.action.ts`
  returns metadata only (`id`, `name`, `mime_type`, `web_view_link`,
  `web_content_link`, ...). There is no action that returns bytes. Any
  file-location integration is impossible without adding one.
- **OneDrive-style URL indirection.** Where content *is* returned, it is
  base64-encoded into a JSON response field (`content` in S3 `get_file` and
  Dropbox `download_file`). Every byte is base64-inflated, buffered whole in
  memory in the JS runtime, then buffered again in Qore.
- **No streaming anywhere.** S3 multipart upload, Dropbox upload sessions
  (`/2/files/upload_session/*`), and Drive resumable upload are all absent. The
  action API cannot express them. Dropbox's own `upload_file` is capped by the
  single-shot endpoint; S3 uploads are capped by memory.
- **Inconsistent vocabulary.** The three apps spell the same two operations six
  different ways: `get_file` / `get_object` / `download_file` for read, and
  `upload_file` / `create_text_object` / `create-file-from-text` for write, with
  path options named `object_key`, `path`, `folder`+`file_name`, and bare IDs.

### 1.3 Strategic fit

Native modules get, for free, things the TS bridge cannot provide: async
non-blocking I/O via `RestClientIo`, connection pinging and monitoring through
`PollingConnectionMonitor`, `AbstractConnection` OAuth2 refresh, record-based
data providers with real iterators, and direct participation in `conn://`
tier 1b (§7).

### 1.4 What migration does *not* buy

Be honest about this: `@aws-sdk/*` stays in `module-v8/ts/package.json`
regardless. Seven other Amazon apps (`cloudfront`, `cloudwatch`, `ec2`,
`lambda`, `ses`, `sns`, `sqs`) use it. Removing `amazon-s3` drops
`@aws-sdk/client-s3` only.

---

## 2. Scope

| App | TS LOC | Actions | Triggers | Target qlib modules |
|---|---|---|---|---|
| `amazon-s3` | 1,936 | 8 | 2 | `AwsS3DataProvider/` (client: existing `AwsRestClient`) |
| `google-drive` | 3,219 | 14 | 2 | `GoogleDriveDataProvider/` (client: `GoogleRestClient` + `drive` profile) |
| `dropbox` | 2,551 | 18 | 3 | `DropboxRestClient.qm` + `DropboxDataProvider/` |

---

## 3. Common structure

Follow `qlib/OneDriveDataProvider/` — the newest and cleanest cloud-storage
module in the repo — for all three:

```
qlib/<X>RestClient.qm                 # client + connection classes (new only for Dropbox)
qlib/<X>DataProvider/
    <X>DataProvider.qm                # module decl; registers factory, app, and actions
    <X>DataProviderFactory.qc
    <X>DataProviderBase.qc            # shared REST plumbing, ConstructorOptions, retry set
    <X>DataProvider.qc                # root provider
    <X><Action>DataProvider.qc        # one per action
    <X><Name>DataType.qc              # request/response types
    <X>WatchChangesDataProvider.qc    # DPAT_EVENT provider (replaces TS triggers)
    <x>-logo.svg
```

Wiring, in each case mirroring OneDrive:

- **Connection → provider.** `getType()`, `hasDataProvider()` → `True`,
  `getAppName()` → the app name, and `getDataProvider()` doing
  `load_module("<X>DataProvider")` + `create_object("<X>DataProvider", getAsync())`
  to avoid a circular module dependency (`OneDriveRestClient.qm:384-390`).
- **Scheme registration** in `ConnectionSchemeCache::SchemeMap`.
- **App + action registration** in the module `init` block via
  `DataProviderActionCatalog::registerApp()` / `registerAction()`, with
  `"groups": (DataProvider::AppGroup::CloudStorage,)`.
- **Build registration**: `qore_user_module("qlib/<X>DataProvider" "<x>-logo.svg")`
  in `CMakeLists.txt`. There is no `Makefile.am` in this repo any more — CMake only.
- **Tests** in `examples/test/qlib/<X>DataProvider/`.
- **Async-first**: base classes hold a `*RestClientIo::RestClientIo`, as
  `OneDriveDataProviderBase` does, so every provider supports non-blocking I/O
  from day one.

---

## 4. Per-module design

### 4.1 `AwsS3DataProvider`

- **Client:** existing `AwsRestClient` / `AwsRestConnection` with
  `aws_service = "s3"`, `aws_s3 = True`. No new client module.
- **Connection:** either reuse `AwsRestConnection` directly with a pinned
  service, or add a thin `AwsS3RestConnection` that pins `aws_service`/`aws_s3`
  and supplies an S3 ping — the latter matches how `GoogleCalendarRestConnection`
  pins `api_profile`, and gives the app its own scheme. **Recommend the thin
  subclass**, scheme `s3`.
- **New surface to get right** (this is where the real work is, since the SDK
  handled it before):
  - virtual-host-style (`<bucket>.s3.<region>.amazonaws.com`) vs path-style
    addressing, and when each is required
  - region redirects: `301 PermanentRedirect` / `307` with
    `x-amz-bucket-region`, and re-signing after redirect
  - XML list parsing with `ContinuationToken` pagination for `ListObjectsV2`
  - multipart upload (`CreateMultipartUpload` / `UploadPart` / `Complete`) and
    multipart ETag semantics
  - `Content-MD5` / `x-amz-checksum-*` where required
- **Actions:** `list-buckets`, `create-bucket`, `list-objects` (record-based,
  paginated), `get-object`, `download-file`, `upload-file`, `delete-object`,
  `copy-object`. Event provider replaces the `new-bucket` and
  `new-or-updated-file` triggers.

### 4.2 `GoogleDriveDataProvider`

- **Client:** add a `drive` entry to `GoogleRestClientBase::ApiProfiles`:
  ```qore
  "drive": {
      "oauth2_scopes": ("https://www.googleapis.com/auth/drive",),
      "ping_method": DefaultGooglePingMethod,
      "ping_path": "/drive/v3/about?fields=user",
      "ping_headers": DefaultGooglePingHeaders,
  },
  ```
  plus a `GoogleDriveRestConnection` pinning `api_profile = "drive"`, exactly as
  `GoogleCalendarRestConnection` does (`GoogleRestClient.qm:409-500`). Scheme
  `gdrive`.
- **Fixes the content-read gap:** implement `download-file` against
  `GET /drive/v3/files/{fileId}?alt=media`, and export of Google-native
  documents via `/export?mimeType=...` — neither is possible today.
- **Also implement:** resumable upload (`uploadType=resumable`) for large files,
  and record-based `list-files` with proper `pageToken` iteration rather than the
  capped list the TS action returns.
- **Actions:** `list-files`, `find-file`, `find-folder`, `get-file`,
  `download-file`, `upload-file`, `replace-file`, `create-folder`,
  `create-file-from-text`, `copy-file`, `move-file`, `delete-file`,
  `create-shortcut`, `add-sharing-preference`. Event provider replaces the
  `new-file` / `new-folder` triggers (Drive `changes.list` with a `startPageToken`).

### 4.3 `DropboxRestClient` + `DropboxDataProvider`

The only one requiring a new client module. Template: `OneDriveRestClient.qm`
(OAuth2 `authorization_code` with `token_access_type: offline`).

**The Dropbox-specific wrinkle** that must be designed for explicitly: Dropbox
splits its API across two hosts with two different calling conventions.

> **Phase B gate: PASSED.** A spike against a local server implementing the
> Dropbox content-endpoint contract confirmed that both `RestClient` and
> `RestClientIo` can carry it with no client-layer changes: JSON arguments in a
> `Dropbox-API-Arg` request header, a raw binary body in both directions
> (`data: "bin"`, `Content-Type: application/octet-stream`), operation metadata
> read back from a `Dropbox-API-Result` response header, and a 2 MB body
> transferred intact. The dual-host split is handled with two client objects —
> one per base URL — which also sidesteps the fact that serialization is
> per-client rather than per-request.
>
> The spike did surface one genuine pre-existing bug, fixed as part of this
> work: the response deserialization guard in
> `NullRestSchemaValidator::parseResponseImpl()` assumed every body arrives as a
> string, which holds for the synchronous `HTTPClient::send()` path but not for
> the asynchronous `HttpClientIo` path, so **any** binary response over
> `RestClientIo` raised `DESERIALIZATION-ERROR` — despite every non-text
> deserializer already accepting binary input. This blocked S3 `download-file`
> as much as Dropbox. Regression test in
> `examples/test/qlib/RestClientIo/RestClientIo.qtest::testBinaryBody()`.

| | RPC endpoints | Content endpoints |
|---|---|---|
| Host | `api.dropboxapi.com` | `content.dropboxapi.com` |
| Used by | metadata, list, move, copy, share | `upload`, `download`, upload sessions |
| Arguments | JSON request body | **`Dropbox-API-Arg` header, JSON-encoded** |
| Body | JSON | raw binary file content |

So `DropboxRestClient` needs to expose both a JSON-RPC call path and a
content-transfer path that puts the arguments in a header and streams a binary
body. This needs verification against `RestClient` / `RestClientIo` early — it is
the main technical risk in the whole migration, and it should be prototyped
before committing to the phase.

- **Scheme:** `dropbox`. OAuth2 scopes as in the current TS app
  (`files.content.read/write`, `files.metadata.read/write`,
  `sharing.read/write`, `account_info.read`).
- **Implement upload sessions** (`/2/files/upload_session/start|append_v2|finish`)
  for files over the single-shot limit — absent from the TS app.
- **Actions:** `list-folder`, `search`, `get-metadata`, `download-file`,
  `upload-file`, `create-text-file`, `create-folder`, `delete-file`,
  `delete-folder`, `copy-file`, `copy-folder`, `move-file`, `move-folder`,
  `create-shared-link`, `list-shared-links`, `revoke-shared-link`,
  `get-file-link`, `list-file-revisions`, `restore-file-revision`. Event provider
  replaces the `new-file` / `new-folder` / `file-modified` triggers
  (`/2/files/list_folder/longpoll` or cursor polling).

---

## 5. Redesign decisions

Since all three can be rewritten, apply these uniformly:

1. **One action vocabulary across all three.** Adopt the OneDrive/qlib spelling
   already in the repo: `list-items` / `get-item` / `download-file` /
   `upload-file` / `create-folder` / `delete-item` / `copy-item` / `move-item` /
   `create-sharing-link`. This is a prerequisite for a coherent file-API
   contract (§7) and for users who switch backends.
2. **Binary in, binary out.** No base64 round-tripping in the transport layer.
   `DataProvider::FileDataType` (`{name, content, mime_type}`) stays the *action*
   payload for UI compatibility, but the underlying providers expose
   stream-based read/write.
3. **Streaming as a first-class path**, not an afterthought: S3 multipart,
   Dropbox upload sessions, Drive resumable upload.
4. **Record-based listing** with real server-side pagination and
   `AbstractDataProviderRecordIterator`, replacing the capped list actions.
5. **`RestClientIo` throughout**, so non-blocking pollers work.
6. **Allowed-values callbacks.** The TS apps' `get_allowed_values` helpers
   (bucket lists, folder pickers, file pickers) must be reimplemented as data
   provider allowed-values callbacks or the UI experience regresses. This is easy
   to overlook and is called out here deliberately.
7. **Triggers become `DPAT_EVENT` watch providers**, following
   `OneDriveWatchChangesDataProvider`.

---

## 6. Effort and risk

| Module | New Qore LOC (est., excl. tests) | Main risk |
|---|---|---|
| `AwsS3DataProvider` | ~2,500–3,000 | S3 addressing modes, region redirects, multipart — new surface previously handled by the SDK |
| `GoogleDriveDataProvider` | ~3,000–3,500 | Google-native doc export semantics; resumable upload |
| `DropboxRestClient` + `DropboxDataProvider` | ~3,000–3,500 | **dual-host + `Dropbox-API-Arg` header convention** — prototype before committing |

Plus test suites, roughly 500–800 LOC each.

Additional risks:
- **No SDK safety net for S3.** The SigV4 core is tested, but S3 request-shaping
  edge cases were previously the SDK's problem and become ours.
- **UI regression** if allowed-values callbacks are skipped (§5.6).
- **Scope changes require re-consent** for Google Drive and Dropbox — a non-issue
  given nothing is released, but it must stay that way. If any of the three ships
  in the TS catalog before this lands, the calculus changes.

---

## 7. Interaction with `conn://`

This is the reason to do both together.

The `conn://` proposal defines three resolution tiers. Tier 2 — the declarative
`DataProviderFileApiInfo` contract registered per app — exists **specifically
because** S3, Dropbox, Google Drive, and OneDrive each spell file operations
differently and are reachable only through generic action dispatch. Migrating
three of those four changes that picture:

- **Native modules can implement tier 1b directly.** An authenticated
  `RestClientIo`-backed handler object gives real streaming reads and writes and
  non-blocking pollers — which request/response action dispatch structurally
  cannot provide. This is strictly better for the three migrated apps.
- **Phase 3 of the `conn://` plan largely dissolves.** That phase was "add a
  `files: {...}` key to `ts-toolkit`'s `TQoreAppWithActions`, have the v8 module
  forward it, and register file APIs for S3 and Dropbox." If those apps become
  native, the TS-side plumbing is no longer on the critical path. It remains
  worth doing eventually, for the ~80 other TS apps and any future one, but it
  drops out of the minimum viable path.
- **Tier 2 still earns its place** — for `FileDataProvider`,
  `FtpClientDataProvider`, `OneDriveDataProvider`, and every TS app that stays in
  TS. The migrated modules should register tier-2 metadata *as well as*
  implementing tier 1b, so the generic action path and the UI stay uniform.
- **§5.1's unified action vocabulary is what makes tier 2 cheap.** If all
  cloud-storage apps spell read `download-file` and write `upload-file` with a
  `path` option, the per-app `DataProviderFileApiInfo` registration collapses to
  a few lines and a sane default becomes possible.

Recommended combined sequencing:

| Phase | Content |
|---|---|
| A | `conn://` phase 1 (scheme, tier 1a redirect, filesystem/FTP/SFTP/HTTP, path confinement, filter hook, tests) — independent, ships value alone |
| B | Prototype the Dropbox dual-host/`Dropbox-API-Arg` path against `RestClient`/`RestClientIo`. **Gate:** if this is ugly, Dropbox slips and S3/Drive proceed |
| C | `AwsS3DataProvider` + `GoogleDriveDataProvider` (the two with existing client infrastructure), with the §5 unified vocabulary |
| D | `conn://` phase 2 (tier 2 declarative file API) + registrations for `FileDataProvider`, `FtpClientDataProvider`, `OneDriveDataProvider`, and the two new modules |
| E | `DropboxRestClient` + `DropboxDataProvider` |
| F | `conn://` tier 1b native handlers for the three migrated modules + REST connections; `DPFR_URL` poller support |
| G | *(optional, deferred)* `ts-toolkit` `files: {...}` key for the remaining TS apps |

---

## 8. Open questions

1. **Retiring the TS apps is out of scope for this repo.** The new data
   providers and apps land here; `apps/amazon-s3`, `apps/google-drive`, and
   `apps/dropbox` live in the separate `module-v8` repository and are retired by
   a change tracked there. What must be resolved at cutover is the conflict:
   two implementations registering the same app name in the action catalog. The
   recommendation is that the `module-v8` removal lands as close as possible to
   each native module shipping; until then, the app names must differ, which is
   user-visible.
2. **`AwsS3RestConnection` subclass vs. plain `AwsRestConnection`.** The subclass
   gives S3 its own scheme, ping, and required-options string; the alternative is
   configuration-by-convention. Recommendation above is the subclass, but it is
   a judgment call.
3. **Does the whole Amazon app family follow?** S3 is the one with a file story,
   so it leads. Whether `ses`/`sns`/`sqs`/`lambda` should also migrate onto
   `AwsRestClient` is a separate, larger question — but if the answer is
   eventually yes, that argues for building `AwsS3DataProvider` on shared AWS
   base classes rather than S3-specific ones.
4. **Where does the shared cloud-storage vocabulary live?** §5.1 implies a common
   contract across OneDrive, S3, Drive, and Dropbox. That could be documentation
   only, or a shared abstract base module. Documentation-only to start;
   revisit after the second module lands.
5. **Google Drive scope granularity.** `https://www.googleapis.com/auth/drive`
   is full access. `drive.file` (app-created files only) is far narrower and may
   suit some deployments. Consider making the scope set profile-selectable rather
   than fixed.
