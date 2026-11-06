# October 2026 c-ares refresh

c-ares 1.34.9 (a security release of October 19, 2026) replaces 1.34.8 as the minimum version and
as the source fallback.

## Versions and sources

|!Library|!Previous|!Updated|!Source
|c-ares|1.34.8|1.34.9|https://github.com/c-ares/c-ares/releases/tag/v1.34.9

The archive's SHA-256 value matches the digest GitHub publishes for the release asset.

## Integration decisions

- Both c-ares system discovery paths and the source fallback require 1.34.9, so an older installed
  resolver cannot bypass the update.  1.34.9 fixes several use-after-free defects that Qore's usage
  pattern reaches (lookups cancelled from a callback or when a channel is shut down, a connection
  used after the socket-state callback returns), a query cache that could return an answer obtained
  with different EDNS options, and a cached answer freed while the application reads it.
- `cmake/PatchCaresLostQuery.cmake` is removed.  It fixed a defect of 1.34.7 and 1.34.8 that lost a DNS
  query when a completion callback started a new query with the id just freed (c-ares issue #1280).
  1.34.9 contains the fix: `ares_detach_query()` removes the id mapping only if it still refers to the
  query, and the send path compares the query itself.  The patch fails the build when the source it
  patches has changed, so it could not have been kept.
- The lost-query recovery in `lib/QoreSocket.cpp` (`qore_cares_channel_lost_query()`: an unfinished
  lookup with active queries and no timeout starts again on a new channel) stays as a safety net, and
  `examples/test/qore/classes/Socket/cares-lost-query.qtest` still tests it with a debug hook.
- 1.34.9 requires a UDP reply to come from the server's port as well as its address.  Qore's custom
  socket functions (`QoreCaresSockets::arecvfrom()`) pass the address that `recvfrom()` fills in, port
  included, so they comply.
- The query cache is enabled by default since c-ares 1.31, per channel.  Qore creates a channel per
  lookup, so the cache has no effect; a shared channel would use it.
