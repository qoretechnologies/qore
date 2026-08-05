# Documentation Tables

Qore documentation does not use Markdown tables. It uses a pipe-delimited format of its own that is
converted to raw HTML `<table>` markup before doxygen ever sees it. This document defines the
format, describes the two implementations that must agree on it, and lists the diagnostics that keep
malformed tables out of the shipped documentation.

## Why a custom format

Doxygen's own Markdown table support is not usable here:

- Every `.dox`/`.dox.tmpl` page in this repository indents its body by four spaces. Under Markdown
  rules a four-space-indented block is a *code block*, so a Markdown table written inside a Qore doc
  page renders as literal text — pipes, dashes and all.
- The Qore format predates doxygen's Markdown support and is used by hundreds of tables across
  `lib/*.qpp`, `qlib/*.qm`, `doxygen/lang/*.dox.tmpl` and the module documentation.
- The generated HTML carries the Qore header styling (`td.qore`), which Markdown tables cannot
  express.

The practical consequence is that **a Markdown table in Qore documentation is not a table** — it is
literal garbage in the generated HTML, and it fails silently. Everything below exists to make that
failure loud.

## The format

A **table row** is a line whose first non-whitespace character is `|`. Cells are separated by `|`. A
**header cell** is written with `!` immediately after its leading `|`.

```
    |!Type|!Description
    |int|a 64-bit signed integer
    |string|a UTF-8 encoded string
```

Rules:

| Rule | Detail |
|---|---|
| Header row | The first row of a table; every cell should start with `!`. `qpp` finds a table by searching for `\|!`, so a table whose header row does not start with `\|!` is **not converted at all**. |
| No trailing delimiter | A row must not end with `\|`; a trailing delimiter emits a spurious empty final cell. |
| No alignment row | There is no Markdown `\|---\|---\|` separator row. Writing one produces a junk table row. |
| Column count | Every row must have the same number of cells as the header row. |
| Line continuation | A row may be continued on the following line by ending the line with `\`. The continuation is joined before the row is split into cells. |
| Table end | The table ends at the first line that does not start with `\|`. |

### Cell text and escaping

A cell may contain any doxygen markup. Three constructs stop `|` from being treated as a cell
delimiter:

| Construct | Example | Notes |
|---|---|---|
| Backslash escape | `a \| b` written as `a \\\| b` | The preferred form. The backslash stays in the emitted `<td>` and doxygen renders `\|` as a literal `\|`. |
| `@code` block | `@code{.py} a \| b@endcode` | Text between `@code` and `@endcode` is not scanned for delimiters. |
| Double quotes | `@ref bitwise_or_operator "\|"` | Text in double quotes is not scanned for delimiters. |

Single quotes are **not** protection characters. An earlier version of the `qpp` splitter treated
`'` as a quote character, which meant an apostrophe in ordinary prose (`Oracle's`, `the branch's
value`) silently swallowed the following cell delimiters and merged two cells into one. That rule
was removed; use `\|` when a cell needs a literal pipe.

### Generated markup

Each row becomes a `<tr>`; each cell becomes a `<td>`, with header cells emitted as
`<td class="qore"><b>…</b></td>`. A one-time `@htmlonly <style>` block defines `td.qore`.

## The two implementations

The format is implemented twice, and the implementations must stay in sync.

| Implementation | Location | Applies to |
|---|---|---|
| `qpp` | `get_table_cells()`, `doRow()`, `process_comment()` in `lib/qpp.cpp` | doc comments in `.qpp` sources, and whole `.dox.tmpl` files via `qpp --table=<file>` (driven by the `QORE_WRAP_DOX` CMake macro) |
| `Qdx` | `Qdx::DocumentTableHelper` in `qlib/Qdx.qm` | doc comments in `.qm`/`.qc`/`.ql` sources via `qdx` → `Qdx::AstProcessor`, and `.dox` files via `qdx --dox` |

`Qdx::DocumentTableHelper::getCells()` and `get_table_cells()` in `lib/qpp.cpp` implement identical
splitting rules; a change to one must be mirrored in the other, and
`examples/test/qlib/Qdx/DocumentTable.qtest` asserts the shared behavior.

### Known behavioral differences

Two differences remain, both deliberate:

1. **Table recognition.** `qpp` only recognizes a table whose header row starts with `|!`; `Qdx`
   starts a table at any line beginning with `|`. Making `Qdx` strict would turn currently-rendered
   tables in out-of-tree modules into literal text, so `Qdx` instead **warns** when a table has no
   header row. Warning-free sources render identically under both implementations.
2. **Cell trimming.** `Qdx` trims leading and trailing whitespace from cell text; `qpp` does not.
   This only affects whitespace inside `<td>`, which HTML collapses — except for header detection:
   `| !Header` is a header cell under `Qdx` but not under `qpp`. `Qdx` warns about that spelling.

## Diagnostics

Both implementations validate every table they see and report the following:

| Diagnostic | Meaning |
|---|---|
| line starts with `\|` but is not part of a table | The header row does not start with `\|!`, so the whole table is emitted literally (`qpp` only). |
| table has no header row | Same defect seen from the `Qdx` side, where the table is still converted (`Qdx` only). |
| Markdown column-alignment row | A `\|---\|---\|` row was found inside a table. |
| row ends with `\|` | A trailing delimiter emits an empty final cell. |
| row has N cells but the header row has M | Usually an unescaped `\|` in cell text; escape it as `\\\|`. |
| header cell has whitespace before `!` | `\| !Header` is not a header cell for `qpp` (`Qdx` only). |

Validation runs before any conversion, so `.dox.tmpl` diagnostics carry exact source line numbers.
Doc comments are processed as isolated buffers, so their diagnostics name the file and quote the
offending row instead.

### Strict mode

Diagnostics are warnings by default, so that out-of-tree modules do not stop building. This
repository turns them into errors:

- `qpp --table-strict` (`-T`) — exits non-zero if any issue was found.
- `qdx --strict-tables` (`-S`) — exits non-zero if any issue was found.
- The `QORE_DOX_TABLE_STRICT` CMake option (default `ON` in this repository) makes
  `QORE_WRAP_QPP_VALUE` and `QORE_WRAP_DOX` pass `--table-strict`, so a malformed table fails the
  build.

To check the whole tree by hand:

```
for f in $(find doxygen modules -name '*.dox.tmpl'); do
    build/qpp --table-strict --table="$f" --output=/dev/null || echo "FAIL: $f"
done
for f in lib/*.qpp modules/*/src/*.qpp; do
    build/qpp --table-strict -d /dev/null -o /dev/null "$f" >/dev/null || echo "FAIL: $f"
done
```

## Writing a table: checklist

- Start with `|!` and mark every header cell with `!`.
- No spaces between `|` and `!`.
- No trailing `|` at the end of a row.
- No `|---|---|` alignment row.
- Escape a literal pipe in cell text as `\|`.
- Keep the cell count constant across rows.
- Continue a long row with a trailing `\` rather than wrapping it onto an unmarked line.
