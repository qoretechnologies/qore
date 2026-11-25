# TZif zoneinfo files

Copyright (C) 2026 Qore Technologies, s.r.o.

`QoreZoneInfo` (`lib/QoreTimeZoneManager.cpp`) reads zoneinfo files in the Time Zone Information Format of
[RFC 9636](https://www.rfc-editor.org/rfc/rfc9636), which obsoletes RFC 8536 and adds version 4. The object is
immutable after construction; lookups need no lock.

## Reading

The whole file is read, up to `QoreZoneInfo::MaxFileSize` (1 MiB; real zoneinfo files are less than 8 KiB), and
parsed from memory (`QoreZoneInfo::init()`):

- A path (`findLoadRegionFromPath()`; `findLoadRegionOrPath()` passes names starting with `/` or `.` from
  `TimeZone::setRegion()` and `Program::setTimeZoneRegion()` to it) is made absolute with the current directory once
  and checked against `PO_NO_FILESYSTEM` and the file-system policy of the code that runs (`checkPathPolicy()`,
  errors name the path as given) before anything else. `loadPath()` then, without the lock: opens exactly that path
  with `O_NONBLOCK`, determines the canonical path of the open file (`F_GETPATH` on macOS, `/proc/self/fd/N` on
  Linux, `realpath()` otherwise), verifies that it names the open file (`stat()` device and inode) and checks the
  policy again for that path before anything is reported about the file; then the file must be regular and is
  read. For code that a sandbox governs, the path is first resolved with `realpath()` and checked, the file is
  opened with `O_NOFOLLOW`, and a failed open or an undeterminable canonical path gives the policy error.
- Zones (`getZone()`): the data read from a file is identified by the device and inode of the file and the
  SHA-256 of the data (`ZoneIdentity`); `identity_map` holds one `QoreZoneInfo` per identity, never returned
  itself. Each name gets a `QoreAliasZoneInfo` named by the name as given (a path) or the region name, which shares
  the data of the identity's zone; `path_aliases` and `region_aliases` map names to them and an entry is used for
  a path only when its identity is unchanged (a changed file gets a new zone; the old one is kept in
  `retired_zones`, as dates refer to it). New zones are inserted under the write lock; when another thread
  inserted the same data first, its zone is used.
- Tenants and quotas (`QoreZoneCharger`, `checkCharge()`, `prepareCharge()`, `commitCharge()`). The rule:
  1. A Program created with no Program running is a host Program (`qore_program_private::tz_host`); its code is
     never charged, whatever its parse options.
  2. A Program created by the host Program or by code that is not restricted is a tenant root with a quota of its
     own (`qore_program_private::tz_quota`). A Program created by restricted code of a non-host Program shares that
     code's tenant quota (`qore_tz_caller_tenant_quota()`), and so does a `SandboxManager` that such code creates
     (`QoreSandboxMemoryAccount::tz_tenant_quota`, set in `qore_sandbox_manager_record_creator()`), so restricted
     code can never obtain a fresh quota.
  3. A new zone (the parsed data if new, plus the name's zone) loaded by restricted code is charged with its memory
     to the tenant quota of the Program, to each member of the governing sandbox
     (`QoreSandboxMemoryAccount::tz_path_zones`/`tz_path_bytes`) and the tenant quota that member carries, and to
     the isolation domain (`domain_path_quota`): 256 zones / 32 MiB each. There is no process-wide pool.
  The restrictions are those of the code that runs (`CallerMode`), or of the Program for the `%set-time-zone`
  directive (`ProgramMode`: the Program's own parse options and sandbox managers, as the directive is processed
  while other code parses the Program). Memory is checked against and charged to the thread's sandbox charge chain
  (`qore_sb_check_alloc()`, `QoreSandboxCharge::add()`, released at shutdown); in `ProgramMode` it is charged to
  the accounts of the Program's sandboxes directly. Everything that can fail is done before the maps refer to a new
  zone; `commitCharge()` cannot fail. Exceptions are built after the lock is released. Each new name entry counts
  against the limits, so the entries a tenant creates are bounded. When a limit is reached, another name of a
  region whose file is loaded already gets the file's zone named by its path in the zoneinfo directory
  (`region_identity_aliases`, one per file). On Windows, where the file is not identified by its handle, code that
  a sandbox governs cannot load a path; trusted code's paths are made absolute, read without the lock and cached by
  that path (at most 16384).
- Date literals: the JIT rebuilds the zone of a literal by its name with `findLoadedZone()`, which finds a zone that
  was loaded when the code was parsed (a region, a path as given or the local zone) without file access.
- The `%set-time-zone` parse directive (`qore_program_private::findTimeZoneDirectiveZone()`) is processed while
  the Program is parsed, when the code that runs is the caller of the parse: a path is refused when the parsed
  Program has `PO_NO_FILESYSTEM` or any sandbox manager; a region goes to the region loader.
- Deserialized dates and TimeZone objects (`QoreTimeZoneManager::findDataRegion()`): the name is data and never
  opens a path: the region name of the local zone is the local zone (no file access, also if the name is a path), a
  canonical name goes to the region loader, and anything else (a path, a NUL character) raises the uniform unknown
  region error.
- The local zone at startup: a relative `TZ` value or an absolute path in the zoneinfo directory followed by a
  canonical region name is loaded as a region; any other absolute path is read at exactly that path with
  `O_NONBLOCK`. Nothing is read relative to the current directory.
- A region (`QoreTimeZoneManager::findLoadRegion()`) is the runtime's own read of its time zone database, not
  subject to the file-system policy of the code that names it, so that sandboxed code can deserialize a date in a
  region that is not cached yet (qore#7442). It takes names from data as well (`find_create_timezone()` for DBI
  drivers and Arrow/Parquet imports, AOT/JIT date literals, regions of deserialized dates), so a name is never
  loaded as a path there. The region name of the local zone returns it. A canonical name (`isRegionName()`: at
  most 255 bytes of components of `[A-Za-z0-9_+-]` separated by single `/`) is looked up in `region_aliases` under
  the read lock and otherwise loaded by `loadRegion()` without the lock: `open_region_file()` opens the
  `realpath()` of the zoneinfo directory with `O_DIRECTORY` and walks the region's components with `openat()` and
  `O_NOFOLLOW` (`O_DIRECTORY` for directories, `O_NONBLOCK` for the file), taking one component at a time from a
  string of at most `PATH_MAX` bytes, so a component replaced during the walk cannot lead outside; a component
  that is a symbolic link is read with `readlinkat()` and followed only inside of the directory (`..` cannot go
  above it, an absolute target must be in it, at most 40 links); the file must be regular. The zone of the file is
  named by its path in the zoneinfo directory (on macOS the true name from `F_GETPATH`, as the file system does not
  distinguish case, so every spelling of a region reaches its file; spellings share its data and are charged). Every
  failure, also a file that is not a zoneinfo file, raises the same `TZINFO-ERROR`
  `unknown time zone region "<name>"` (`raiseUnknownRegion()`), so the error does not show whether a path exists.
  `checkRegion()` gives it for a region string with a NUL character, a `..` or empty component or a backslash. The
  zoneinfo directory must not be writable by untrusted code (hard links and files put there cannot be told apart).
- `parse()` checks the version 1 header, computes the size of each data block from the header counts in 64-bit
  arithmetic and checks it against the file size before anything in the block is read. A version 1 file is read
  from its 32-bit block. In a version 2+ file the version 1 header and block are only skipped; the 64-bit block
  after the second header and the footer are read.
- `parseDataBlock()` reads one block with the RFC 9636 section 3.2 checks: strictly ascending transition times,
  type indices less than `typecnt`, `utoff` not -2^31, DST and indicator bytes 0 or 1, designation indices less
  than `charcnt` with a NUL at or after the index.
- The footer is `NL TZ-string NL`; `QorePosixTzRule::parse()` parses a nonempty TZ string.

Every check raises `TZINFO-ERROR` with the file name and the problem; the zone is then not loaded or cached.
Parsing is bounded by the file size limit, so it has no cancellation checks.

## Lookup

`QoreZoneInfo::getUTCOffsetImpl()`:

- no transitions: the footer rule if there is one, else time type 0
- before the first transition: time type 0
- on or after the last transition with a footer rule: the rule
- otherwise the type of the last transition at or before the time (binary search); after the last transition of a
  file without a footer rule, local time is unspecified, and the type of the last transition is used, as in the
  reference implementation (tzcode)

`QorePosixTzRule::getUTCOffset()` evaluates the rule for the time reduced into 1970 - 2370 (the Gregorian calendar
repeats every 400 years), so no year or time calculation can overflow for any 64-bit time. It computes the
transitions of the year before, the year of and the year after the time, and one more year back, because a version
3 rule time can be up to 167 hours before or after the rule date. A year whose DST end is a full year (or more) after
its start has DST all year (RFC 9636 section 3.3.1); a year whose end is before its start (southern hemisphere) has
DST at its start and end.

The standard UTC offset of the zone (`TimeZone::UTCOffset()`, and the first guess when local time is converted to
UTC) is the standard offset of the footer rule; files without a footer rule keep the earlier heuristic (the last
non-DST type in the type array).

## RFC 9636 mapping

| Behaviour | RFC 9636 | Test (`examples/test/qore/vars/tzif-zone-files.qtest`) |
|---|---|---|
| version 2+ files: the version 1 header and data block are only skipped | 4: readers SHOULD ignore the version 1 header and data block except for skipping them | `version2Structure` (invalid v1 counts, v1 data that differs) |
| 64-bit transition times are read | 3, 3.2 (TIME_SIZE 8 in the version 2+ block) | `wideTransitions`, `zicFixtures` (Qore/Wide 1850, 2050), `rfcExamples` (B.2 1896) |
| header and data block lengths are checked against the file size | 4: readers SHOULD check that the headers and data blocks fit the file size | `corruptFiles` (truncated header, v1 and v2 blocks, huge counts) |
| `typecnt` and `charcnt` are not zero | 3.1: MUST NOT be zero | `corruptFiles` |
| `isutcnt` and `isstdcnt` are zero or `typecnt` | 3.1: MUST either be zero or equal to `typecnt` | `corruptFiles` |
| transition times strictly ascending | 3.2: sorted in strictly ascending order | `corruptFiles` (equal, descending) |
| type indices in range | 3.2: MUST be in the range [0, typecnt - 1] | `corruptFiles` |
| `utoff` is not -2^31; DST indicator 0 or 1 | 3.2: MUST NOT be -2^31; MUST be 0 or 1 | `corruptFiles` |
| designation index in range and NUL-terminated | 3.2: MUST be in the range [0, charcnt - 1]; a NUL MUST exist at or after the index | `corruptFiles` |
| standard/wall and UT/local indicators 0 or 1 | 3.2: MUST be 0 or 1 | `corruptFiles` |
| time type 0 before the first transition | 3.2: specified by time type 0 | `zicFixtures` (1800), `rfcExamples` (B.2 1890, B.4) |
| footer rule on or after the last transition | 3.2: specified by the TZ string if present and nonempty | `zicFixtures` (slim after 1995, fat after 2037), `rfcExamples` (B.2 2019, B.4, B.5) |
| no transitions: the footer rule, else time type 0 | 3.2 | `footerRules`, `version2Structure` (time type 0) |
| empty footer: information not available | 3.3: if the string is empty, the information is not available | `rfcExamples` (B.3), `version2Structure` |
| footer framing `NL TZ-string NL`, no NUL | 3.3: MUST NOT contain NUL octets | `corruptFiles` (missing, unterminated, NUL) |
| POSIX TZ strings: names, offsets, `Jn`, `n`, `Mm.w.d`, rule times, default DST offset | 3.3 (POSIX.1-2017 section 8.3) | `footerRules`, `invalidFooters` |
| signed rule hours from -167 to 167 | 3.3.2 (version 3 extension) | `footerRules` (RFC example, hour 26), `rfcExamples` (B.4) |
| DST all year | 3.3.1 | `footerRules` (both RFC examples) |
| versions 3, 4 and later are read like version 2 | 3 (readers of version N can read later versions), 3.1 | `version2Structure`, `rfcExamples` (B.4, B.5) |
| version 4 leap-second tables (truncated, expiring) are read | 3.1 | `rfcExamples` (B.5) |
| data after the footer (version 2+) or after the version 1 block (version 1) is ignored | 3, 4: version 1 readers MUST ignore data after the version 1 block | `version1Files`, `version2Structure` |
| designations with other bytes than ASCII alphanumerics, '-' and '+' become numeric (`-10`, `+0530`) | 4: SHOULD act as if the designation contained a numeric string | `version2Structure` |

## Decisions where RFC 9636 leaves the choice to the reader

- A footer that is not consistent with the last transition violates section 3.3 (MUST); zic 2022g writes one for
  America/Ojinaga of tzdata 2026e with `-b slim`. The type of the last transition then applies up to the first
  transition of the rule after it, as in tzcode; a rule without transitions applies from the last transition on.
- A TZ string that begins with `:` (SHOULD NOT) has an implementation-defined meaning and raises `TZINFO-ERROR`.
- A TZ string with a DST name and no rule uses `M3.2.0,M11.1.0`, the default rule of tzcode and glibc (POSIX leaves
  it implementation-defined).
- The version 3 rule hour extension is also accepted in version 2 files, as tzcode does.
- The leap-second records are read but not applied: transition times are compared with Qore's epoch seconds, and
  broken-down time has no leap seconds, as before.
- The footer consistency rule and the version 1 block contents are not validated.

## Verification

Besides the test, every zdump transition boundary from 1800 to 2400 of all zones of tzdata 2026e compiled with
`zic -b slim` and `zic -b fat`, and of the macOS system zoneinfo, gives the same offset, DST flag and abbreviation in
Qore as in zdump.
