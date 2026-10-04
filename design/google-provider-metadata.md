# Google provider metadata without live discovery

GoogleDataProvider 2.2 resolves Calendar v3 and Gmail v1 from complete upstream Discovery documents shipped beside
its module. `GoogleDiscoverySchemas.qc` verifies exact SHA-256 checksums, API identities, versions, and revisions
when loading the module. The module resources are read during initialization so the module-resource sandbox rule
applies; subsequent schema resolution needs neither filesystem access nor network access. CMake packages the same
resources with source and AOT modules, including Google's Apache 2.0 license and the provenance notice.

The snapshots retain every schema and resource method. They use the existing request/input/output type resolver,
including recursive references. A missing, truncated, mismatched, or stale document fails module loading with
`GOOGLE-SCHEMA-UNAVAILABLE`; the loader does not substitute generic or partial types. An intact older pinned revision
is intentionally valid for that package: expiry is determined by an explicit package update, not wall-clock age.
Repairing a failed package permits a subsequent module load. Loaded successful schemas remain immutable for the
process lifetime; package upgrades require restarting workers.

`ProviderIndex::createDataProviderIndex()` and `runIndexWorker()` enter `DataProviderStaticMetadataHelper` before
loading providers or running setup callbacks. Its scope includes deferred catalog output-type resolution as well
as serialization. During that scope GoogleDataProvider rejects APIs without a pinned document before consulting
runtime caches or making HTTP requests. Thus a complete indexing pass makes zero Google discovery requests under
both immediate refusal and dropped egress. No negative network cache or timer is needed. Runtime APIs without a
shipped schema still use the existing live discovery path; failures are not cached, so later calls retry. HTTP
schema retrieval happens outside the shared type-cache mutex to keep runtime I/O from blocking offline indexing.
Cache publication takes that mutex and retains the first successful document, so concurrent discovery cannot replace
a revision whose types have already been resolved.
Runtime action execution and authentication continue to use the provided REST connection.

The action catalog retains callbacks that failed output-type resolution. Before serializing such an action,
ProviderIndex retries its callback in static metadata context. A failure is recorded through the existing
`serialize-action-metadata` qualification path and the action is omitted from the index. The index report is not
`complete`; a generic type is never substituted. Later builds retry retained callbacks, enabling recovery in the
same process. Output types themselves remain excluded from the serialized index.

Regression coverage is in `examples/test/qlib/GoogleDataProvider/`. `OfflineSchemas.qtest` resolves every method and
schema, nested response fields, empty responses, invalid paths, repeated unavailable APIs, and runtime request and
authentication behavior. `OfflineIndex.qtest` covers failure reporting, repeated builds, recovery, and byte-for-byte
native/worker agreement for the Google apps. `test_offline_schemas.py` tests source and AOT package corruption and
repair, and runs schema, index, and existing worker lifecycle tests inside isolated Linux network namespaces with
REJECT and DROP rules. System-call tracing verifies that those tests make no IPv4/IPv6 network calls.

Run after rebuilding the module targets:

```sh
cmake --build build --target GoogleDataProvider-qmod ProviderIndex-qmod -j4
python3 -B -W error examples/test/qlib/GoogleDataProvider/test_offline_schemas.py -v
```
