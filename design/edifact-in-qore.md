# EDIFACT in Qore

This document describes the architecture and practical usage of the EDIFACT support shipped in the Qore standard library (the `EdifactUtil` module).

Related documentation:

- Module docs: `qlib/EdifactUtil/EdifactUtil.qm`
- CLI example: `examples/edifact-schema-load.qr`
- Tests/fixtures: `examples/test/qlib/EdifactUtil/`

## Architecture Overview

The EDIFACT support is layered so that parsing/serialization can work without a directory schema, while schema-aware workflows can be enabled when directory flat files are available.

Core components:

- **EdifactBase / EdifactDelimiters**
  - `EdifactDelimiters` handles UNA parsing and delimiter configuration.
  - `EdifactBase` provides common parsing and serialization utilities, segment handling, and release character escaping.
- **Readers / Writers / Iterators**
  - `EdifactReader` (and abstract iterator classes) parse EDIFACT streams into message data structures.
  - `EdifactWriter` builds EDIFACT interchanges and messages from structured data.
- **Schema support (optional)**
  - `EdifactSchema` is a typed representation of a UN/EDIFACT directory release.
  - `EdifactDirectoryLoader` loads flat-file directories (EDMD/EDSD/EDCD/EDED/EDCL) into a schema.
  - `EdifactDirectoryCache` persists schemas as Qore value strings for fast reloads.
  - `EdifactSchemaRegistry` is the central access point that loads, caches, and registers schemas by release.
- **DataProvider integration**
  - `EdifactReadDataProvider`, `EdifactWriteDataProvider`, and `EdifactSchemaDataProvider` expose EDIFACT functionality via the DataProvider framework and `qdp`.

## Directory Data and Cache

The EDIFACT module does not ship directory data. Provide flat-file directory releases (for example `D21A`) yourself and load them on demand. The module caches the parsed schema to speed up future loads.

Cache control:

- Env var `QORE_EDIFACT_DIRCACHE` sets the cache root (default: `~/.qore/edifact`).
- Cache options are passed to `EdifactSchemaRegistry::loadRelease()`:
  - `skip_cache_read`: ignore cache and load from directory.
  - `cache_only`: load only from cache (throws on cache miss).
  - `cache_error`: how to handle cache parse/type errors (`"ignore"`, `"warn"`, `"error"`; default `"warn"`).
  - `cache_warnings`: optional list reference (use `\cache_warnings`) that receives warning strings when `cache_error` is `"warn"`.

When cache errors are ignored or warned, they are treated as cache misses so the loader can rebuild a fresh cache from directory flat files.

## Practical Usage

### 1) Parse EDIFACT data

```qore
%requires EdifactUtil

string edi = "UNA:+.? 'UNB+UNOA:1+SENDER+RECEIVER+240101:1200+1'";
EdifactReader r(edi);
list<hash<auto>> messages = r.readAll();
```

### 2) Generate EDIFACT data

```qore
%requires EdifactUtil

EdifactWriter w((
    "sender_id": "SENDER",
    "recipient_id": "RECEIVER",
    "control_reference": "REF001",
));

w.addMessage((
    "message_reference": "1",
    "message_type": "ORDERS",
    "message_version": "D",
    "message_release": "01B",
    "segments": (
        ("BGM", ("220", "PO-123")),
        ("DTM", (("137", "20240101", "102"),)),
    ),
));

string out = w.getText();
```

### 3) Load a schema from a directory release

```qore
%requires EdifactUtil

string release_dir = "/path/to/edifact/D21A";
list<string> cache_warnings = ();

EdifactSchema schema = EdifactSchemaRegistry::loadRelease("D21A", (
    "dir": release_dir,
    "skip_cache_read": True,
    "loader_opts": ("strict": True),
    "cache_error": "warn",
    "cache_warnings": \cache_warnings,
));

if (cache_warnings.size())
    printf("cache warnings: %N\n", cache_warnings);
```

### 4) Use the DataProvider layer (`qdp`)

```qore
qdp '@Edifact/read{path=/tmp/in.edi}' search {}
qdp '@Edifact/write{path=/tmp/out.edi}' create ("messages": $data)
qdp '@Edifact/schema{release=D21A,dir=/path/to/edifact/D21A,skip_cache_read=true}' search {}
```

### 5) CLI helper

The example CLI `examples/edifact-schema-load.qr` exposes schema loading and cache options for local testing.

## Tests and Fixtures

Tests live under `examples/test/qlib/EdifactUtil/`. Directory fixtures are stored under `examples/test/qlib/EdifactUtil/fixtures/` and include both positive and negative cases.

## Notes and Best Practices

- Use `loader_opts.strict` in CI or validation pipelines to catch malformed EDMD content early.
- For production, prefer cache loading for speed, and log `cache_warnings` so cache corruption is observable.
- Schema-aware workflows are optional; basic parsing/writing does not require directory releases.
