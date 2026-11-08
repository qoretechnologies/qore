# Address resolution hints

Copyright (C) 2026 Qore Technologies, s.r.o.

Qore's socket and global address-resolution APIs accept the platform `AI_*` constants.
`QoreCaresAddrInfoResolver` translates these to the corresponding `ARES_AI_*` constants before calling
`ares_getaddrinfo()`. Their numeric values are unrelated and can differ between platforms. The original
flags remain available when formatting results, including the optional canonical name.

Numeric-only host requests are checked with `ares_inet_pton()` before any lookup: a resolvable hostname
such as `localhost` is still invalid when `AI_NUMERICHOST` is requested. The c-ares API defines the matching
flag but does not enforce this restriction itself. Numeric service requests use `ARES_AI_NUMERICSERV`.
Qore also sets this flag for every nonempty decimal service string, so binding port zero and connecting
to a numeric port do not invoke a potentially blocking system NSS service-name lookup.

## Localhost names and the hosts file

c-ares answers a localhost name (`localhost` or a name ending in `.localhost`, RFC 6761 section 6.3) with the
hosts file's addresses for the name, and then adds the loopback address of every requested address family that the
file did not give the name (`file_lookup()` and `ares_addrinfo_localhost()` in c-ares 1.34). The system resolver
(glibc, musl) answers with the file's addresses only. With a hosts file that maps `localhost` to `127.0.0.1` only, as
in a podman container, c-ares answered `::1, 127.0.0.1`, sorted `::1` first, and a server bound to `localhost`
(`Socket::bindINET()`, `HttpServer::addListener()`) listened on `::1`, while a client of the system resolver
connected to `127.0.0.1` and was refused (the server holds the port there without listening; see the
`SocketBindNameReservation` tests).

`QoreCaresAddrInfoResolver::start()` therefore reads the addresses that the hosts file gives a localhost name
(`qore_hosts_file_addresses()`), from the file that c-ares reads (`/etc/hosts`, or the file set with the debug hook
`dbg_cares_set_hosts_file()`, which is also given to the channel as `ARES_OPT_HOSTS_FILE`; on Windows, where
c-ares finds the file in the registry, no file is read). `keepHostsFileAddresses()` then drops the addresses that
c-ares added, if any of the answer's addresses is in the file. The added loopback address remains the answer of a
lookup in an address family that the file does not give the name, and of a lookup of a name that the file does not
have, both as RFC 6761 recommends; the system resolver gives the same answers for explicit families.
`examples/test/qore/classes/Socket/LocalhostHostsFile.qtest` covers both cases, hosts files with both and with one
entry, a dual-stack server, and, on Linux with `getent`, the system resolver's answer.

The resolver retains its asynchronous callback and error-handling paths. Numeric validation failures use
the same `QOREADDRINFO-GETINFO-ERROR` exception as other resolution failures, and asynchronous callers
receive that exception through their future.

`examples/test/qore/classes/Socket/resolve-addrinfo.qtest` covers numeric restrictions through synchronous
and asynchronous APIs, valid and invalid numeric port boundaries, passive wildcard binding, canonical
numeric addresses, and rejection of synchronous resolution on the I/O worker.

The c-ares [API documentation](https://c-ares.org/docs/ares_getaddrinfo.html) describes its hint constants.

## The status of a failed lookup, and its checks

A `QOREADDRINFO-GETINFO-ERROR` exception for an answer of the resolver has the resolver's status in its argument
(`arg.status`): `ENOTFOUND`, `ENODATA`, `EBADNAME`, `ENONAME` and `ESERVICE` are definite answers about the name,
which `DataProviderRetryUtil` does not retry; the others, such as `ESERVFAIL`, `EREFUSED` and `ETIMEOUT`, can be
temporary. c-ares can give a definite status that hides a temporary failure:

- for a lookup of both address families (`AF_UNSPEC`), it gives the status of the family whose query finished last,
  so a `SERVFAIL` for `A` and a `NODATA` for `AAAA`, finishing last, gave `ENODATA`;
- it goes on to the next name after a `SERVFAIL` or `REFUSED` answer for a single-label name, which it looks up with
  the search domains (c-ares issue #852: systemd-resolved fails or refuses single-label names by policy), so a search
  domain's `NXDOMAIN` hid the failure of the name itself.

`QoreCaresAddrInfoResolver` therefore checks a definite failure, but only if a query of the lookup failed: it counts
the failed queries of its channel (a `SERVFAIL`, `NOTIMP` or `REFUSED` answer, a timeout or a connection error) with
`ares_set_server_state_callback()`. A lookup during which no query failed is definite as c-ares gives it, with no
other query: a check would send again a query whose answer the query cache does not keep, such as an `NXDOMAIN`
without an SOA record. c-ares 1.34.9 reports the failed queries when its public call returns, after the callbacks of
the lookups, so `advance()` decides what follows a result after c-ares has returned; it also starts the checks then,
as a lookup takes the environment lock. The checks are:

- **Confirm**: a lookup of both families is checked by a lookup of each family. The failure is definite only if both
  answers are; otherwise the lookup fails with the status of the family that was not.
- **Probe**: a single-label name is asked for itself, without the search domains (`ares_query_dnsrec()`), for the
  record types that the lookup asked for (`A`, `AAAA` or both; c-ares asks for `AAAA` only for an IPv6 lookup, also
  with `AI_V4MAPPED`). A `SERVFAIL` or `REFUSED` answer makes the lookup fail with it. A lookup of both families that
  is confirmed with a definite failure is probed if a query failed during the confirmation, as the lookups of the
  confirmation hide it too. The probe names the host as given, not an alias that `HOSTALIASES` gives it.
- **Repeat**: a check that finds an address (a family with addresses, or a probe with records) starts the lookup
  again once, and its result is the answer, so that the addresses are in the order that c-ares sorts them (RFC 6724).
  If the lookup started again fails with a definite status after the check saw a temporary failure, the lookup fails
  with that failure: a definite failure is never reported after a temporary one was seen.

A lookup started again after c-ares lost its query (`restart()`) drops a running check. The unit test
`QoreCaresCheckTest` (`lib/ql_debug.cpp`, run by `asyncio_unit_tests.qtest`) drives lookups against a DNS server that
answers each query in a known order, including lookups destroyed or started again during each check;
`cares-error-status.qtest` covers the statuses through the public APIs with `examples/test/DnsTestServer.qm`.
