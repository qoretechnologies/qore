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
