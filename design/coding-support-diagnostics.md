# Coding-Support Diagnostics — Design

A suite of parse-time diagnostic features for human developers and AI-driven
coding tools: near-match identifier suggestions, an enum-exhaustiveness `switch`
warning, machine-readable parse diagnostics, source-column tracking, stable error
codes, and code-frame snippets.

## Goals

1. Warn when a `switch` over an enum value does not cover all enum members.
2. Add "did you mean …?" near-match suggestions to identifier-resolution errors.
3. Provide **machine-readable (structured) parse diagnostics** so tools/agents can
   consume errors deterministically instead of scraping prose.
4. Supporting foundations: source **column tracking**, **stable error codes**, and
   **code-frame snippets**.

## Design principles

- **Never break existing code.** Exhaustiveness is a *gated warning*, not an
  error. New behavior is opt-in or default-off where it could surprise.
- **Parse-time only.** None of this touches IR lowering or the AOT wire format.
- **One suggestion engine, many call sites.** A single helper, reused, so the UX
  is consistent and improvements land everywhere at once.
- **Suggestions must respect scope/visibility** — never propose a name the user
  can't legally use; an AI consumer will trust it.
- **Per-`Program` opt-in for structured capture** — zero overhead on the happy
  path and when disabled.

## Always-on behaviors

These appear in ordinary error/warning output with no flag:

- **"did you mean …?" near-match suggestions** on identifier-resolution errors —
  unresolved bareword, undefined class, undefined type, undefined function,
  undefined hashdecl member, undefined constant/namespace, unknown method/member,
  unknown named argument. Driven by a bounded Damerau-Levenshtein helper
  (`q_edit_distance`) plus `QoreSuggestionList`. Threshold is `min(2, max(1,
  len/3))`. Suggestions only ever name identifiers that actually exist in scope
  and respect visibility; a pure capitalization difference is flagged specially
  ("case mismatch").
- **`non-exhaustive-switch` warning** (`QP_WARN_NONEXHAUSTIVE_SWITCH`, bit
  `1<<20`, in the default + module warning masks `QP_WARN_ALL` /
  `QP_WARN_MODULES`) — fires when a `switch` over an enum-typed value omits
  members and has no `default:`; lists the unhandled members; a `default:`
  silences it. Only simple `case <const>:` nodes participate (relational case
  blocks are skipped, mirroring the duplicate-case check). For `*enum` operands,
  `NOTHING` coverage is not required.

## Structured capture

Machine-readable capture of all parse errors/warnings is available three ways, all
opt-in and default-off with zero overhead otherwise:

| Channel | How to enable | Output |
|---|---|---|
| Per-`Program` API | `Program::setParseDiagnosticsCollected(True)`, then `Program::getParseDiagnostics()` | `list<hash<ParseDiagnosticInfo>>` |
| CLI JSON | `qore --diag-format=json` / `QORE_DIAG_FORMAT=json` | JSON array on stdout, parse-only, exit 2 on error |
| CLI code frames | `qore --code-frames` / `QORE_CODE_FRAMES=1` | Rust-style `file:line:col` + source line + caret, parse-only |

Capture is wired at the single chokepoint (`makeParseException` /
`makeParseWarning`), so **every** parse error/warning — the always-on
suggestions, the enum warning, and Qore's existing overload/type errors — flows
through it. The per-`Program` API collects after-parse (no callback variant, to
avoid parse-lock reentrancy); the collection list is cleared at the start of each
parse action. Collection is a dedicated tooling toggle, not a parse-option /
capability restriction.

The CLI JSON shape mirrors `ParseDiagnosticInfo` exactly, so the channels agree.

### `ParseDiagnosticInfo`

Declared as a hashdecl in `lib/QC_Program.qpp` (so it is documented):

```
hashdecl ParseDiagnosticInfo {
    string severity;          # "error" | "warning"
    string code;              # stable id, e.g. "UNDEFINED-CLASS"
    *int warningCode;         # QP_WARN_* bit for warnings, NOTHING for errors
    string file;
    *string source;
    int startLine;
    int endLine = -1;
    int startColumn = -1;     # -1 when unknown
    int endColumn = -1;
    string message;
    *string hint;
    list<string> suggestions = ();
}
```

Internally each diagnostic is built as a `QoreDiagnostic` record (severity, stable
string `code`, warning bit, location, message, optional hint, suggestions) at the
chokepoint when collection is enabled, then converted to the hashdecl.

## Source columns

Columns are tracked end-to-end. `QoreProgramLineLocation` carries
`start_column`/`end_column` (-1 = unknown), interned with a full lexicographic
ordering that includes columns. They are threaded from the scanner's `get_loc`
(passing flex/bison's `first_column`/`last_column`) through all grammar
`getLocation(@…)` call sites, and exposed additively via the `SourceLocationInfo`
hashdecl (`column`/`endcolumn`). **Parse-error and AST-node locations carry
accurate columns.** Code frames render the source line with a `^~~~` caret span
using these columns, falling back to line-only when columns are -1.

## Overload diagnostics

When a call fails to match any variant, Qore's existing overload diagnostics carry
the needed detail and are surfaced through the structured API as
`PARSE-TYPE-ERROR`:

- single-candidate mismatches produce a precise per-argument message ("argument
  'x' to f(int x) expects int, but call supplies bool");
- multi-candidate mismatches render the actual call with argument types plus every
  candidate signature.

Both are raised via `parseException`, so they flow through the chokepoint and are
captured by the collector / JSON emitter / code frames. A diverging-argument
pointer for the *multi-candidate* case (not pinpointed today) is a possible future
refinement in the resolution code.

## Key files

- Edit-distance + suggestion engine: `lib/support.cpp`
  (`include/qore/intern/QoreLibIntern.h`).
- Suggestion call sites: `lib/QoreNamespace.cpp`, `lib/QoreTypeInfo.cpp`,
  `lib/FunctionCallNode.cpp`, `include/qore/intern/QoreNamespaceIntern.h`.
- Enum warning: `lib/SwitchStatement.cpp` (`QP_WARN_*` in
  `include/qore/QoreProgram.h`, string table in `lib/QoreProgram.cpp`).
- Columns: `include/qore/intern/QoreLibIntern.h`, `lib/QoreLib.cpp`,
  `lib/scanner.lpp`, `lib/parser.ypp`.
- Structured API + JSON/frames: `lib/QC_Program.qpp`, `lib/QoreProgram.cpp`,
  `command-line.cpp`.
- Tests: `examples/test/qore/diagnostics/`.

## Known limitation

Reflection-of-*declaration* source locations (function variants, classes,
hashdecls, constants, members) build their `QoreProgramLocation` from bare line
ints inside `UserSignature`/variant/member constructors, so they report
`column == -1`. Threading columns through those constructor chains is a separate,
bounded effort that the diagnostic features do not depend on — the collector, JSON
emitter, and code frames consume parse-error/AST-node locations, which do carry
columns.
