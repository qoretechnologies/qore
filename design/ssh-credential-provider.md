# SSH Credential & Host-Key Trust Provider

- **Status:** Accepted; phase 1 in progress
- **Date:** 2026-05-18
- **Tracking issue:** qoretechnologies/qore#5336
- **Affected components:** new Qore `sshutil` binary module; `module-ssh` (libssh, server); `module-ssh2` (libssh2, client); `Ssh2Connections`; SFTP `FileLocationHandler`

## 1. Summary

Introduce a pluggable SSH key and host-key trust provider abstraction, shipped as the Qore `sshutil` binary module and shared by `module-ssh` (server) and `module-ssh2` (client).

The abstraction lets embedding applications whose logical security principals do not map to the OS user running the process manage SSH host-key trust and SSH private-key material per logical principal, per connection, or per listener. Existing filesystem and SSH-agent behavior remains available; applications are no longer forced to choose between the process user's filesystem trust domain and disabling verification.

## 2. Motivation / Problem

`module-ssh` and `module-ssh2` currently bind important SSH key material to the OS process user's filesystem:

- `module-ssh2` supports `setKnownHostsFile()`, process-global default known-host controls, host-key policies, `setKeys()`, `setKeysFromData()`, and SSH agent authentication. Host-key trust is still file-oriented; there is no callback or pluggable trust store.
- `module-ssh2` can disable the implicit `~/.ssh/known_hosts` default with `SSH2Base::setDefaultKnownHostsFile("")`, but that must mean only "no implicit filesystem trust source"; it must not implicitly disable host-key verification.
- `module-ssh` server host identity keys are configured as filesystem paths in `SshListenerConfig.host_keys` and loaded with `ssh_pki_import_privkey_file()`. This prevents application-managed and passphrase-protected host keys unless exposed through files.

This is a concrete trust-domain gap for multi-principal embedding applications, not only an API cleanup. SSH host-key trust and identity private keys need a Qore-level indirection that can be backed by filesystem defaults, memory, agent, database, secret-manager, or other application-managed storage.

## 3. Concerns (kept separate)

| | Concern | module-ssh today | module-ssh2 today |
|---|---|---|---|
| A | Client identity private keys | n/a | file paths, in-memory key blobs, SSH agent, default `~/.ssh/id_rsa` discovery |
| B | Server host identity private keys | filesystem paths only, passphrase hard-coded to `nullptr` in the listener load path | n/a |
| C | Server host-key trust store | n/a | OpenSSH `known_hosts` file, REJECT/TOFU policy, explicit session-only TOFU, or explicit verification disable |
| D | Shared key-material vocabulary | native `SshKey` class backed by libssh | no matching key-introspection class; mostly paths/blobs passed to libssh2 |

Concern mapping: A → `AbstractSshClientIdentityProvider`; B → `AbstractSshServerHostKeyProvider`; C → `AbstractSshHostKeyStore`; D → shared typed hashes. These stay separate: client identity resolution, server host-key resolution, and remote host-key trust have different cardinality, policy, error handling, and audit needs.

## 4. Hard constraints

`module-ssh`'s C++ `SshKey` cannot be moved to Qore core or reused directly by `module-ssh2`:

- `module-ssh` links libssh; `module-ssh2` links libssh2.
- Qore core must not gain a dependency on an SSH library for provider plumbing.
- SSH key formats are not the same abstraction as Qore core's OpenSSL certificate/private-key classes.

The shared layer is therefore a Qore-level contract plus typed key-material and public-key metadata vocabulary. Native key parsing, import/export, and protocol verification stay in the binary modules.

## 5. Design constraints / non-goals (binding)

- `SshProviderContext` splits generic SSH fields from opaque application context. Generic fields: purpose, connection name, listener name, `original_host`, normalized host, port, username, policy names, and the deadline fields (§7.4). `opaque_context` is pass-through only: generic modules must never inspect, branch on, persist, or log it.
- No generic `metadata` map. If a value is needed for generic-module behavior it is a typed generic field; otherwise it belongs in `opaque_context`.
- Client identity has the same trust-domain requirement as host-key trust: when a client identity provider is configured, generic modules must not silently authenticate with OS-process-user credentials (SSH agent, `~/.ssh` default key discovery). Such fallbacks require explicit opt-in. Pre-existing defaults such as `use_agent = True` do **not** count as opt-in once a provider is configured.
- No application-specific provider implementations, schemas, configuration, or naming in `qore`, `module-ssh`, or `module-ssh2`. Application-backed stores/providers live in the embedding application.
- The contract is defined purely in SSH terms (host, port, key material, algorithm, policy), never in application terms (users, roles, RBAC, vaults, tenancy).
- This proposal does not add a server-side remote-host trust store. `module-ssh`'s `SshServerAuthProvider::VirtualSshAuthProvider` (client authorized-keys policy) is complementary and out of scope.

## 6. Module

A new generic binary module **`sshutil`** (namespace `Qore::SshUtil`) defines the typed SSH provider contract. It provides namespace-scoped abstract classes, typed hashes, string-backed enums, descriptor-only default filesystem implementations, and a registration registry. The module is delivered with Qore and targets `%requires qore >= 2.3`.

The contract is intentionally a binary module rather than a qlib module because downstream QPP/C++ code must be able to reference the provider/store class types directly. This allows `module-ssh` and `module-ssh2` to expose typed APIs instead of accepting generic `object` values and duck-typing method names.

## 7. Architecture

### 7.1 `AbstractSshClientIdentityProvider` (concern A)

- Returns ordered client authentication candidates for a connection.
- Candidate types: SSH agent, in-memory private-key material, and (default implementation only) filesystem paths.
- Each candidate carries diagnostics/audit metadata but no private key material.
- Malformed provider responses fail closed before authentication starts. With multiple valid ordered candidates, server-side rejection of one may advance to the next per normal SSH auth semantics.
- Provider-side secret retrieval / key import errors are **fatal (fail closed)** unless the candidate is explicitly marked `optional: True`, in which case the error is non-fatal and the next candidate is tried.

### 7.2 `AbstractSshServerHostKeyProvider` (concern B)

- Returns one or more server host identity keys for a listener; supports passphrase-protected and application-managed private keys.
- Default implementation preserves current `SshListenerConfig.host_keys` filesystem behavior.
- Returned key order is authoritative and preserved for server host-key algorithm preference where the SSH library allows it.
- Unsupported key algorithms may be skipped only while at least one usable key remains and diagnostics expose no secret material; if none remains, listener startup fails closed.
- Consulted at listener startup and on explicit listener reload/reconfiguration; **not** consulted per accepted connection.
- **Reload atomicity (stage-then-swap):** reload fully resolves and imports the entire new key set into a staging set before any swap. The listener atomically adopts the new set only on full success. On any failure a running listener retains the previous set and reload returns an error; at initial startup, failure means the listener does not start. No partially-applied key set is ever observable.

### 7.3 `AbstractSshHostKeyStore` (concern C)

- Verifies and stores remote server host keys for `(host, port)` plus optional provider context.
- **The store owns all host-key matching semantics.** The binary module passes `(host, port)` plus the observed key and does not match itself. A conforming store (including the default) reproduces core OpenSSH `known_hosts` matching: `[host]:port` qualification, host wildcards, hashed `|1|...` HMAC-SHA1 hostnames, and plain-key `@revoked` entries.
- **Host normalization (baseline, OpenSSH-compatible):** match after (1) stripping at most one trailing root dot (`host.` ≡ `host`) and (2) lowercasing ASCII `A–Z` only. **No implicit IDNA/punycode conversion** — non-ASCII hostnames are compared verbatim as bytes (this is OpenSSH's own behavior). `original_host` is preserved separately for diagnostics/audit. IPv6 literals are normalized as address literals and bracketed only when producing/parsing `[host]:port` entries. Resolved IP addresses are **not** passed in v1; stores must not perform implicit DNS/reverse-DNS matching.
- Plain-key `@revoked` is part of the baseline fail-closed contract: a matching revoked plain key yields `Revoked` and the connection is refused.
- **`@cert-authority` is out of scope.** libssh2 1.11.1 exposes no certificate/X.509/host-certificate API and `libssh2_knownhost_*` has no `@cert-authority` support, so it cannot be honored at the module boundary regardless of store capability. Stores **must reject** `@cert-authority` entries (fail closed). Revisit only if libssh2 gains host-certificate support.
- Any OpenSSH `known_hosts` feature/marker present but unsupported by the active store fails closed for the affected entry; never silently ignored.
- Supports persistent REJECT and TOFU semantics.
- `acceptHostKeyIfAbsent(context, observed_key)` is an atomic TOFU operation returning a structured decision so concurrent first connections cannot race.
- `approveHostKey(context, expected_observed_key, record)` (or equivalent compare-and-set) is an explicit manual-approval operation **separate from `connect()`**. It must atomically verify the approved observed key is still the key being approved; it must not trust stale exception payloads without rechecking key identity.

### 7.4 Deadline model (corrected)

Provider/store callbacks run on the connect/listener-setup path and are bounded by the overall operation timeout. Two fields are carried in `SshProviderContext`, **both authoritative for their domain**:

- **`deadline_ns`** — absolute monotonic timestamp, integer nanoseconds in the domain of Qore `clock_getnanos()`. Authoritative for the binary module's own in-process enforcement and interruptible I/O. Not a wall-clock `Date`.
- **`remaining_ns`** — remaining duration, recomputed at each provider/store call site. Authoritative input for any provider/store that delegates across a process boundary (database, secret manager, subprocess, remote service), where the local monotonic epoch is meaningless. Not diagnostic-only.

Providers/stores must honor interruptible I/O and whichever value applies to them; exceeding the budget fails closed. Timeout handling is deterministic and must not use polling loops.

### 7.5 Shared typed hashes (concern D)

- `SshPublicKeyInfo`: algorithm, public key blob, OpenSSH public key text when available, SHA256 fingerprint, optional SHA1/MD5 fingerprints, optional comment, typed non-secret source fields.
- `SshClientIdentityCandidate`: non-secret descriptor (agent / key reference / filesystem path / in-memory provider-backed key), optional `optional` flag, public-key info, typed non-secret audit fields; **no** private key data or passphrases.
- `SshPrivateKeyMaterial`: short-lived material returned **only** through a point-of-need provider method during connection setup or listener key import. May contain private key data, optional public key data, `passphrase_ref` or `secret_ref`, format, algorithm hint, typed non-secret source field. Never a long-lived configuration object.
- `SshHostKeyRecord`: host, port, normalized host pattern if applicable, key info, typed trust fields, timestamps, policy fields.
- `SshHostKeyDecision` enum: `Matched`, `Added`, `AcceptedSession`, `Unknown`, `Mismatch`, `Revoked`, `StoreError`, `InvalidResponse`, `ProviderError`, `Timeout`. `Added` = durable persistence succeeded; `AcceptedSession` = explicitly selected session-only TOFU accepted without durable persistence.
- `SshHostKeyDecisionInfo`: decision enum, observed public key info, optional matched record/reference, optional store/provider diagnostic text, typed non-secret audit fields.
- `SshProviderContext`: generic SSH fields (§5) plus `opaque_context`.

All fields above are typed, generic, non-secret. Application data travels only in `opaque_context`.

For identity keys the binary module derives canonical `SshPublicKeyInfo` from the supplied key material so provider- and module-computed fingerprints cannot disagree. For remote host keys, fingerprints come from the SSH transport (`module-ssh2` already exposes them via `getHostKey()`).

## 8. Binary module integration

### `module-ssh2`

- Add `SSH2Base::setClientIdentityProvider()` and `SSH2Base::setHostKeyStore()` (independent setters are the canonical contract). An optional aggregate convenience type `SshProviderSet` (referencing providers by object or registry name) may be accepted at the connection layer and decomposes to the independent setters.
- Keep `setKeys()`, `setKeysFromData()`, `setUseAgent()`, `setKnownHostsFile()`, `setVerifyHostKey()`, and default setters for compatibility.
- If a host-key store is configured, verify through the store instead of the known_hosts file path; otherwise preserve existing OpenSSH `known_hosts` behavior.
- **Host-key verification is a precondition for client authentication:** it must complete with a success decision (`Matched`, `Added`, or explicit `AcceptedSession`) before the client identity provider is consulted for any secret and before any auth attempt or credential transmission. Any non-success decision aborts before secret retrieval.
- If verification is enabled but no store/known_hosts file is configured: fail closed under persistent TOFU / REJECT; session-only trust requires the explicit `TrustOnFirstUseSession` policy.
- `setVerifyHostKey(False)` remains the explicit insecure bypass. `setDefaultKnownHostsFile("")` only disables implicit filesystem trust-source selection.
- Client public-key auth ordering is controlled by `SshClientAuthOrder` enum: `ExplicitFirst` (default) = explicit `setKeysFromData()`, then explicit `setKeys()` paths, then provider candidates, then explicitly-enabled fallback; `ProviderFirst` = provider candidates before explicit caller keys.
- When a client identity provider is configured, OS-process-user fallback (agent, `~/.ssh/id_rsa`/default key discovery) is suppressed unless explicitly opted in via `SshClientIdentityFallbackPolicy`: `Disabled`, `Agent`, `DefaultKeys`, `AgentAndDefaultKeys`. Default with a provider configured = `Disabled`; with no provider configured the legacy behavior (effectively `AgentAndDefaultKeys`) is preserved for back-compat. Legacy booleans such as `use_agent` do not implicitly override provider isolation. If the provider yields no usable candidate and no explicit caller keys were set, authentication fails closed.

### `module-ssh`

- Add listener support for `AbstractSshServerHostKeyProvider` in addition to `SshListenerConfig.host_keys`; path-based host keys remain the default.
- Import provider-returned key material through libssh without temporary files; support passphrases for encrypted host keys.

### Connection & file-location layers

- `Ssh2Connections` exposes a readable `host_key_policy` option with stable string values `reject` / `tofu` / `tofu-session`; the binary API keeps the type-safe `Ssh2HostKeyPolicy` enum (string-backed enum in qlib where the option framework permits).
- `known_hosts = ""` is an explicit per-connection "no known_hosts file" setting, distinct from leaving it unset (inherit process default).
- `verify_host_key` boolean option for parity with `setVerifyHostKey()`; disabling carries an explicit insecure-semantics doc note.
- Provider/store support (or documented equivalent) added to `Ssh2Connections` construction and to SFTP `FileLocationHandler` paths.
- Serialized connection definitions store a stable **registry name** for a provider/store; objects and `opaque_context` are resolved/injected at runtime via the module's registration registry (mirroring `ConnectionSchemeCache` / `DataProvider::registerFactory`). No object or `opaque_context` serialization.

## 9. Security requirements

- Host-key verification precedes client authentication (see §8); non-success ⇒ no secret retrieval, no auth attempt.
- Fail closed whenever verification is enabled but cannot be performed, except explicit session-only TOFU.
- A missing configured known_hosts file is acceptable only as the first-use case for a persistent TOFU store/file; failure to persist an accepted persistent-TOFU key fails the connection.
- `setDefaultKnownHostsFile("")` must not silently create a session-only trust mode.
- Client identity provider and host-key store are consulted afresh on every connection attempt including reconnects; decisions/secrets must not be cached across connections. Caching a first decision and skipping on reconnect is non-conforming.
- With a client identity provider configured, process-user fallback is used only with explicit `SshClientIdentityFallbackPolicy` opt-in; otherwise fail closed.
- Provider methods may be called from C++ during connect/listener setup and must be thread-safe and exception-safe.
- **Concurrency model:** provider/store objects must be reentrant or internally synchronized; the same object may be invoked concurrently by multiple connections/listeners/threads. v1 has no single-threaded-ownership mode. Default filesystem implementations are internally synchronized. Generic modules document their callback concurrency model rather than relying on hidden global serialization.
- Provider/store callbacks bounded by the deadline model (§7.4); exceeding it fails closed; deterministic, no polling.
- Binary modules must not invoke provider/store callbacks while holding long-lived internal connection/listener locks: snapshot state, call out, then reacquire.
- Secret material is requested at point of need, never persisted or logged, never retained beyond connection setup / listener key import. Qore values are GC'd and not guaranteed zeroized; the requirement is non-retention and non-disclosure, not in-memory wiping. Long-lived secret-bearing typed hashes are non-conforming; use a point-of-need pull method.
- The default filesystem host-key store performs safe cross-process updates (advisory file lock + atomic temp-file rename), consistent with private `~/.ssh` creation at mode 0700. `acceptHostKeyIfAbsent()` atomicity holds across processes sharing the file.
- Default filesystem implementations enforce functional-domain / sandbox semantics (`QDOM_FILESYSTEM`).
- Exceptions include a structured non-secret `SshHostKeyDecisionInfo` payload in `arg`. Decision → exception mapping (normative, tested):
  - `Matched` / `Added` / `AcceptedSession` → success (no exception)
  - `Unknown` → `SSH2-HOSTKEY-UNKNOWN`
  - `Mismatch` → `SSH2-HOSTKEY-MISMATCH`
  - `Revoked` → `SSH2-HOSTKEY-REVOKED`
  - `StoreError` → `SSH2-HOSTKEY-STORE-ERROR`
  - `InvalidResponse` → `SSH2-HOSTKEY-INVALID-RESPONSE`
  - `ProviderError` → `SSH2-HOSTKEY-PROVIDER-ERROR`
  - `Timeout` → `SSH2-HOSTKEY-TIMEOUT`
  - `Added` reachable only when persistent TOFU durably stores the key; `AcceptedSession` only under explicit session-only TOFU; under REJECT an absent key yields `Unknown`. `module-ssh` server-side uses parallel `SSH-HOSTKEY-*` / listener-start errors.
- Opaque provider context and secret references are never written to logs, exception text, connection info, or serialized connection definitions by generic modules.

## 10. Resolved design decisions

| # | Question | Resolution | Rationale |
|---|---|---|---|
| 1 | Module / namespace name | `sshutil` binary module, namespace `Qore::SshUtil` | Binary modules use lower-case names by convention; the scope covers credentials, server host identity, and host-key trust, so `sshutil` is broader and more accurate than a credential-only name |
| 2 | Independent setters vs aggregate | Independent setters canonical; optional `SshProviderSet` aggregate at the connection layer that decomposes to them | Keeps the "sibling interfaces, not a god object" principle; aggregate is convenience only |
| 3 | Deadline field name/type | `deadline_ns` (absolute monotonic int ns, `clock_getnanos()` domain) **and** `remaining_ns` (recomputed per call); both authoritative | Verified Qore fn is `clock_getnanos()`; delegating providers need a duration, in-process enforcement needs the absolute deadline |
| 4 | Opt-in/override option shapes | `SshClientAuthOrder::{ExplicitFirst(default), ProviderFirst}`; fallback via the client identity fallback enum | Single typed enums; explicit-first preserves least surprise |
| 5 | Client identity fallback enum | `SshClientIdentityFallbackPolicy::{Disabled, Agent, DefaultKeys, AgentAndDefaultKeys}` | `Disabled` avoids confusion with Qore `NOTHING`; default `Disabled` when a provider is set, legacy behavior otherwise |
| 6 | Session-only success decision name | `AcceptedSession` | Clear tie to "session-only TOFU"; rejects `AcceptedEphemeral` |
| 7 | Exception names (invalid resp / provider fail) | `SSH2-HOSTKEY-INVALID-RESPONSE`, `SSH2-HOSTKEY-PROVIDER-ERROR` (+ parallel `SSH-HOSTKEY-*` server-side) | Consistent with existing `SSH2-HOSTKEY-*` family; mapping is normative/tested |
| 8 | Host normalization (root dot, IDNA) | Strip ≤1 trailing dot; lowercase ASCII only; **no IDNA/punycode** (verbatim byte compare for non-ASCII) | Exactly OpenSSH's own behavior; least surprising; keeps default store interoperable |
| 9 | Provider key-retrieval failure fatal vs optional | Fatal by default; non-fatal only if candidate sets `optional: True` | Fail-closed default with an explicit, auditable escape hatch |
| 10 | libssh2 host-certificate support | **Not supported** (libssh2 1.11.1 has no cert/X.509 API; no `@cert-authority`). `@cert-authority` out of scope; stores reject it (fail closed) | Determined empirically from installed headers; not a design choice |
| 11 | Listener reload/rotation atomicity | Stage-then-swap; never a partial key set; previous set retained on reload failure | Deterministic, no observable partial state |
| 12 | Provider/store concurrency model | v1: reentrant / internally synchronized; no single-threaded-ownership mode | Safe default; default impls internally synchronized |
| 13 | Default filesystem store format | v1: OpenSSH `known_hosts` only; alternative serialization deferred | Interoperable, matches current behavior, no new on-disk format; locks §7.3 normalization to OpenSSH |
| 14 | Resolved peer IPs as store context | Not in v1; logical host only | Avoids implicit DNS/reverse-DNS trust expansion |
| 15 | Serialized provider/store reference model | Stable registry name + runtime resolution via registration registry | Mirrors `ConnectionSchemeCache`/`registerFactory`; no object/secret serialization |
| 16 | Min Qore version / compile guards | Qore `%requires qore >= 2.3`; `module-ssh` and `module-ssh2` hard-depend on `sshutil`; guards `HAVE_LIBSSH2_KNOWNHOST_API`, `HAVE_LIBSSH2_PUBLICKEY_FROMMEMORY`; `module-ssh` in-memory host-key import needs libssh `ssh_pki_import_privkey_base64` | QPP/C++ module APIs need binary-module class types; `sshutil` is shipped with Qore 2.3 |

## 11. Remaining (implementation-time, not design-blocking)

- Exact numeric libssh / libssh2 minimum versions, confirmed against the CI build matrix when wiring the binary modules (the relevant guards are fixed; only the numeric floors need build verification).
- `module-ssh` in-memory host-key import: confirm the precise libssh API/version available on all CI targets (Alpine cmake 4 matrix).

These do not block freezing the phase-1 binary-module contract.

## 12. Phasing

1. Define the `sshutil` binary module contract: typed hashes, the three abstract classes, default filesystem/agent implementations, registration registry, focused unit tests.
2. Keep the explicit `TrustOnFirstUseSession` policy as the temporary compatibility path for embedders with no trust store yet.
3. Expose readable `host_key_policy`, explicit empty `known_hosts`, and `verify_host_key` in connection/data-provider construction paths.
4. Wire `module-ssh2` to host-key stores and client identity providers.
5. Wire `module-ssh` to server host-key providers.
6. Add connection/file-location integration paths and documentation.
7. External application-backed implementations live outside the generic SSH modules.

## 13. Required test coverage

- disabled implicit known_hosts + persistent TOFU fails closed; + explicit session TOFU works;
- host-key verification runs before client identity retrieval; non-success aborts before any secret retrieval / auth attempt;
- client identity provider with no usable candidate and no explicit keys fails closed, no process-user fallback;
- `use_agent = True` default does not re-enable process-user fallback once a provider is configured; explicit fallback policy values re-enable only the selected source;
- malformed provider responses fail closed before auth starts;
- ordered identity candidates advance after server-side auth rejection per SSH semantics;
- invalid/exception provider-store responses fail closed; `InvalidResponse`/`ProviderError`/`StoreError`/`Timeout` map correctly;
- provider/store re-invoked on reconnect; nothing cached across connections;
- provider/store invoked concurrently per the documented model;
- concurrent persistent TOFU across processes does not race;
- persistent TOFU → `Added`; explicit session-only → `AcceptedSession`; `AcceptedSession` never writes a durable record;
- manual approval uses compare-and-set; rejects if the observed key changed since review;
- OpenSSH matching for `[host]:port`, wildcards, hashed names, plain-key `@revoked`;
- host normalization: lowercase ASCII, ≤1 trailing dot, IPv6 `[host]:port`, `original_host` preserved, non-ASCII verbatim (no IDNA);
- `@cert-authority` entries rejected (fail closed); other unsupported known_hosts features fail closed;
- host matching uses the logical caller host; no implicit DNS/reverse-DNS matching;
- decision→exception mapping holds for every `SshHostKeyDecision`;
- server host-key provider preserves key order, skips unsupported algorithms only as specified, fails listener startup when no usable key remains;
- listener reload re-consults the provider and handles failure atomically (no partial key set);
- generic modules serialize provider/store references only via the registry-name mechanism;
- no generic-module logging/serialization of opaque context, private key data, passphrases, or secret references.

## 14. Reference: current API surface

- `module-ssh2` (libssh2, ns `Qore::SSH2`): `SSH2Base::setKnownHostsFile/getKnownHostsFile/setVerifyHostKey/getVerifyHostKey/setHostKeyPolicy/getHostKeyPolicy`, static default equivalents, env-var defaults, `getHostKey()`, `addKnownHost()`, `setKeys()`, `setKeysFromData()`, `setUseAgent()`. Canonical policy surface `enum<Ssh2HostKeyPolicy>` (`TrustOnFirstUse`, `TrustOnFirstUseSession`, `Reject`); legacy `SSH2_HOSTKEY_*` ints retained. Classes: `SSH2Client`, `SFTPClient`, `SSH2Channel`, `SSH2Listener`, `SSH2Base`. libssh2 1.11.1 installed; no host-certificate API.
- `Ssh2Connections` (shipped, transitional): exposes `known_hosts` (empty string = explicit "no file"), `host_key_policy` (`reject`/`tofu`/`tofu-session`), `verify_host_key` (default `True`), `use_agent`, `keepalive_interval`.
- `module-ssh` (libssh, ns `Qore::Ssh`): `SshKey` (import/export, fingerprinting, type inspection, public-key blob, equality, generation). `SshListenerConfig.host_keys` is a list of filesystem paths. `SshServerAuthProvider::VirtualSshAuthProvider` covers client auth policy, not server host-key identity storage.
- Qore qlib conventions: `ConnectionProvider`, `DataProvider`; scheme registration via `ConnectionSchemeCache`; SFTP `FileLocationHandler` exposes key-file options but no host-key trust-store options.
