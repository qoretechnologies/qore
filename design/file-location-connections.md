# Named-Connection File Locations (`conn://`)

Any place in the product that accepts a `FileLocationHandler` location string can name a **connection
plus a path** rather than a URL:

```
conn://<connection-name>/<path>[{<options>}]
```

This exists because a growing class of file stores — S3, Dropbox, Google Drive, OneDrive, SharePoint,
Google Cloud Storage — has **no URL-expressible file scheme at all**. Before this, a location string
could only reach systems whose access fits in a URL, which excluded every SaaS file service the product
integrates with.

Usage, options and the security controls are documented on
`FileLocationHandlerConnection` in `qlib/FileLocationHandler/`. This document records the resolution
architecture and the constraints that shaped it.

## The path belongs to the location; everything else belongs to the connection

The path is always relative to the connection's own base location. The connection owns the host, the
credentials and the root; the location string owns only the leaf. A leading slash does not make the
path absolute, and `..` segments that would escape the connection's base are rejected.

That asymmetry is the whole value of the scheme. It is what lets a location string be derived from
partner data, or stored in configuration, without carrying credentials or being able to redirect at the
host level.

## Resolution tiers

`FileLocationHandlerConnection` resolves the connection once, then dispatches through the first tier
that applies.

| Tier | Mechanism | Covers |
|---|---|---|
| **1a** | the connection returns a `ConnectionProvider::FileLocationRedirect` naming an existing scheme, and dispatch re-enters that scheme's handler | local filesystem, FTP, SFTP, plain HTTP |
| **1b** | the connection supplies a native handler object via `AbstractConnection::getFileLocationHandler()` | connections whose state is not URL-expressible |
| **2** | the app's declarative file API, dispatched through its data provider actions | every SaaS file service |
| **3** | none — `LOCATION-ERROR` naming the connection, its type and its app | anything else |

Tier 1a is deliberately first and deliberately cheap: redirecting into an existing handler reuses that
handler's blocking, streaming and polling implementations unchanged, rather than reimplementing them
behind a connection.

Tier 3 errors must use `getSafeUrl()`, never `getUrl()` — the failure path is exactly where a
credential-bearing URL would otherwise leak into a log.

## Tier 2: the declarative file API

An application declares how its **actions** map onto file semantics, and the handler drives them. No
per-app handler code is involved.

`DataProviderFileApiInfo` (in `qlib/DataProvider/DataProviderActionCatalog.qc`) names the read and
write actions, the options carrying the path and container, the response field holding the data, and
how to interpret it. `DataProviderActionCatalog::registerAppFileApi()` records it;
`getAppFileApi()` looks it up.

Two axes carry the variation that matters:

- **Read mode** — `DPFR_DATA` means the response field holds the data; `DPFR_URL` means it holds a URL
  to fetch, and the handler re-enters `FileLocationHandler` with it when `follow_url` is set. OneDrive
  and SharePoint are `DPFR_URL`; S3, Dropbox, Google Drive and Google Cloud Storage are `DPFR_DATA`.
- **Write format** — `DPFW_FILE` (a `FileDataType` hash), `DPFW_BASE64`, or `DPFW_RAW`.

Dispatch resolves the app from the connection, looks up the action in the catalog, walks to the child
provider named by the action's path, and issues one `doRequest()` with the path and container options
filled in.

Six modules register a file API today: `AwsS3DataProvider`, `DropboxDataProvider`,
`GoogleDriveDataProvider`, `OneDriveDataProvider`, `SharePointDataProvider` and
`GoogleCloudStorageDataProvider`. The local filesystem and FTP are reached through tier 1a instead,
since both already have a `FileLocationHandler` scheme of their own.

### The shared action vocabulary is a contract, not a convention

Tier 2 only works because the cloud-storage providers agree on action names. They converged on
`download-file` / `upload-file`, alongside `list-items`, `get-item`, `create-folder`, `delete-item`,
`copy-item`, `move-item` and `create-sharing-link`.

`GoogleCloudStorageDataProvider` uses `download-object`, which is correct — it is an object store, not
a file store — and is exactly why the mapping is *declared* per app rather than assumed. A new
cloud-storage provider should adopt the shared spelling unless its domain genuinely differs, and must
register its file API either way.

Related policies these modules apply uniformly, and a new one should too: binary in and binary out with
no base64 round-tripping in the transport layer; streaming as a first-class path (S3 multipart, Dropbox
upload sessions, Drive resumable upload); record-based listing with real server-side pagination rather
than capped list actions; `RestClientIo` throughout so non-blocking pollers work; allowed-values
callbacks for bucket, folder and file pickers; and change triggers as `DPAT_EVENT` watch providers.

## `FileLocationHandler` must not depend on `ConnectionProvider`

This is the constraint most likely to be violated by a well-meaning change.

`FileLocationHandler` is a leaf utility whose only dependency is `Util`, and it sits *below*
`ConnectionProvider` in practice — `RestClient` requires both, and `ConnectionProvider::HttpConnection`
resolves its `ssl_cert_path` / `ssl_key_path` options *through* `FileLocationHandler`. A hard
`%requires` edge would drag `DataProvider`, `AsyncSocketIo`, `reflection`, `ProviderIndexUtil` and
`Logger` into every consumer of a file location, and would turn any future
`ConnectionProvider` → `FileLocationHandler` edge into a genuine cycle.

So the handler is registered unconditionally for `conn` and `connection`, and resolves connections by
`load_module("ConnectionProvider")` plus a dynamic call on first use — the same idiom
`FileLocationHandlerSftp` uses for `ssh2`. A missing module becomes a clear `LOCATION-ERROR` instead of
a link-time cost for everyone.

The tier-2 hashdecl and registration API live in `DataProvider`, which `ConnectionProvider` already
re-exports, so app modules register without taking on a new dependency.

## Option validation had to become opt-out

`FileLocationHandler::getInfo()` validates the `{...}` options in a location against the option set
declared by the handler *for that scheme*. For `conn://` the pass-through options legitimately belong
to whatever target the connection resolves to — which is not known until after resolution.

`AbstractFileLocationHandler::selfValidatesOptions()` (default `False`) lets a handler take over:
`getInfo()` skips the unknown-option check, and the connection and data-provider handlers validate once,
precisely, against the resolved target. The error improves as a result, because it can name the actual
target scheme.

The alternative — having the connection handler declare the union of every registered handler's options
— was rejected: it makes the accepted option set depend on module load order, and produces error
messages that name nothing useful.

## Security

A named connection can reach any configured system, including internal ones, and location strings are
frequently derived from user or partner data. That is a real widening of what a location string can do.

`FileLocationHandler::setConnectionFilter()` and the `QORE_FILE_LOCATION_CONNECTIONS` environment
variable restrict which connections are reachable. A deployment that accepts externally-influenced
location strings should set one of them.
