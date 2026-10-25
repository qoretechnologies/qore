# TZif zoneinfo files

Copyright (C) 2026 Qore Technologies, s.r.o.

`QoreZoneInfo` (`lib/QoreTimeZoneManager.cpp`) reads zoneinfo files in the Time Zone Information Format of
[RFC 9636](https://www.rfc-editor.org/rfc/rfc9636), which obsoletes RFC 8536 and adds version 4. The object is
immutable after construction; lookups need no lock.

## Reading

The whole file is read with `QoreFile::readBinary()`, up to `QoreZoneInfo::MaxFileSize` (1 MiB; real zoneinfo
files are less than 8 KiB), and parsed from memory:

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
