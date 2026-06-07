# Coding-Support Diagnostics — Phased Implementation Plan

Branch: `feature/coding_support` (off `develop`)
Target release: Qore 2.3 (see `project_qore_2_3_target`)

## Goal

Improve Qore's diagnostics to better support both human developers and AI-driven
coding tools:

1. Warn when a `switch` over an enum value does not cover all enum members.
2. Add "did you mean …?" near-match suggestions to identifier-resolution errors
   (bareword, undefined class, function, method, constant, namespace, hashdecl
   member, named argument).
3. Provide **machine-readable (structured) parse diagnostics**, settable per
   `Program` via a new API in `lib/QC_Program.qpp` (plus a standalone-`qore`
   emitter), so tools/agents can consume errors deterministically instead of
   scraping prose.
4. Supporting foundations: source **column tracking**, **stable error codes**,
   and **code-frame snippets**.

## Current-state facts (verified)

- `SwitchStatement::parseInitImpl` (`lib/SwitchStatement.cpp:188`) already:
  folds every simple case value to a concrete `QoreValue` (`:229-243`), runs a
  duplicate-case pass (`:246-268`), and tracks whether a `default:` exists
  (`deflt`). The operand type is parsed into `parse_context.typeInfo` at `:197`.
- Enums are first-class: `QoreEnumDecl` (`include/qore/QoreEnumDecl.h`) exposes
  `getMemberCount()`, `findMemberByValue()`, and `QoreEnumMemberIterator`.
  `QoreTypeInfo::getReturnEnum(ti)` extracts the `QoreEnumDecl*` from a type.
- Bareword resolution + the existing hint hook live in
  `lib/QoreNamespace.cpp`: `parseResolveBarewordIntern` (`:1663`) and
  `bareword_foreign_hint` (`:1640`). Undefined-class errors:
  `parseFindScopedClassIntern` (`:2031`). All candidate maps (locals, class
  constants, statics, globals, namespace/root constants) are in scope at the
  error sites.
- **All** parse errors/warnings funnel through one chokepoint:
  `qore_program_private::makeParseException` and `::makeParseWarning`
  (`include/qore/intern/qore_program_private.h:2044`, `:2748`). Both build a
  `ParseException(loc, code/name, desc)` and raise into an `ExceptionSink`.
- Warning codes are bitmask `#define QP_WARN_*` in `include/qore/QoreProgram.h`
  (`:46-72`), highest currently used bit `1<<19`
  (`QP_WARN_AMBIGUOUS_OVERLOAD`). Names are literal strings at call sites; no
  central registry. Qore-visible mirror constants `WARN_*` are in
  `lib/QC_Program.qpp` (`:957+`).
- **Columns are computed but discarded**: flex/bison fill
  `yylloc->first_column/last_column` (`scanner.lpp` YY_USER_ACTION,
  `parser.hpp` YYLTYPE), but `get_loc` (`scanner.lpp:72`) passes only
  `first_line/last_line` to `getLocation()`. `QoreProgramLocation` /
  `QoreProgramLineLocation` (`QoreLibIntern.h:358`) store only `int16_t
  start_line/end_line` (+ `file`, `source`, `lang`, `offset`).
- **No edit-distance utility exists** anywhere in `lib/` or `include/`.
- Tests assert parse errors via `assertThrows("PARSE-EXCEPTION", \p.parse(), ...)`
  (e.g. `examples/test/qore/parser/hashdecl-parse-error.qtest`). Warnings are
  delivered to a separate `warn_sink` `ExceptionSink` with a `warn_mask`.

## Design principles

- **Never break existing code.** Exhaustiveness is a *gated warning*, not an
  error. New behavior is opt-in or default-off where it could surprise.
- **Parse-time only.** None of this touches IR lowering or the AOT wire format —
  safe on `feature/5164_jit`'s sibling work, but this branch is off `develop`.
- **One suggestion engine, many call sites.** A single helper, reused, so UX is
  consistent and improvements land everywhere at once.
- **Suggestions must respect scope/visibility** — never propose a name the user
  can't legally use; an AI consumer will trust it.
- **Per-`Program` opt-in for structured capture** — zero overhead on the happy
  path and when disabled.
- Per `feedback_unreleased_functionality_docs`: no per-commit release notes on
  the feature branch; doc/release-note summary happens at merge. `@since` tags
  go under 2.3.

---

## Phase 0 — Test scaffolding & baselines

**Goal:** a repeatable way to assert on errors, warnings, suggestions, and
(later) structured output before writing features.

- Add `examples/test/qore/diagnostics/` with a helper that parses a snippet
  into a child `Program` and returns the raised error/warning descriptions
  (using `warn_sink` + `warn_mask`). This is the harness all later phases use.
- Establish a baseline: capture current messages for a handful of
  undefined-identifier and bad-switch snippets so we can diff UX changes.

**Risk:** none. **Wire format:** none.

---

## Phase 1 — Edit-distance helper + "did you mean" for barewords & undefined classes

**Goal:** the highest value-to-risk feature; ship the suggestion engine and wire
it into the two most common identifier errors.

- New `lib/qore-edit-distance.cpp` + `include/qore/intern/qore_edit_distance.h`:
  - `int q_edit_distance(const char* a, const char* b, int max)` —
    Damerau-Levenshtein with early-exit at `max` (bounded, O(n·m) worst case but
    capped; only runs on the error path).
  - `class QoreSuggestionSet` — collects candidate names, ranks by distance,
    returns the best 1–3 within threshold. Threshold:
    `min(2, max(1, len/3))`; treat pure case-difference as a distinct, stronger
    hint ("did you mean 'myVar'? (case mismatch)").
- Extend `bareword_foreign_hint` usage: at the bareword error site
  (`QoreNamespace.cpp:1744`), gather candidates from the maps already consulted
  (locals, class constants, statics, globals, namespace + root constants) into a
  `QoreSuggestionSet`, append "; did you mean '…'?" to the message.
- Same treatment at `parseFindScopedClassIntern` (`:2031`): gather class names
  from the searched namespaces; for a scoped name, also suggest a different
  namespace path if the leaf matches a class elsewhere ("did you mean
  'Foo::Bar'?").
- Keep the foreign-keyword hints; suggestions are additive.

**Tests:** positive (typo → correct suggestion), negative (garbage name → no
suggestion), case-mismatch, scoped vs unscoped, visibility (private member not
suggested).

**Risk:** low — error-path only. **Wire format:** none.

---

## Phase 2 — Extend suggestions to remaining identifier sites

**Goal:** consistent "did you mean" across the identifier surface.

Wire `QoreSuggestionSet` into:
- Undefined function / unresolved call (function map).
- Unknown method / nonexistent member access (class method + member maps;
  respect access).
- Undefined constant and undefined namespace (`NamespaceMapIterator`).
- Unknown **hashdecl member** access and unknown **named argument** (member
  list of the `TypedHashDecl` / signature param names).

Each is the same pattern: collect in-scope candidates at the existing error
site, rank, append.

**Tests:** one targeted snippet per site. **Risk:** low. **Wire format:** none.

---

## Phase 3 — Enum-exhaustiveness `switch` warning

**Goal:** warn when a `switch` over an enum type omits members and has no
`default:`.

- New warning bit in `include/qore/QoreProgram.h`:
  `#define QP_WARN_NONEXHAUSTIVE_SWITCH (1<<20)`. Add to `QP_WARN_ALL` /
  default masks **only as a non-strict default** (decision point: include in
  `QP_WARN_DEFAULT` or gate behind `QP_WARN_STRICT`? — recommend default-on, it
  is high-signal and silenced by `default:`).
- Mirror Qore-visible constant `WARN_NONEXHAUSTIVE_SWITCH` in
  `lib/QC_Program.qpp` warning-constants group, with docs.
- In `SwitchStatement::parseInitImpl`: capture the operand `typeInfo` before the
  case loop overwrites it (`:197` → save into a local). After the existing
  case-folding loop, if `getReturnEnum(opType)` is non-null **and** `!deflt`:
  iterate `QoreEnumMemberIterator`, mark members hit by a simple case value
  (`findMemberByValue` against folded case values), and if any are unmatched,
  `makeParseWarning(... QP_WARN_NONEXHAUSTIVE_SWITCH, "NONEXHAUSTIVE-SWITCH",
  "switch on enum '%s' does not handle: %s; add the missing case(s) or a
  'default:'")`.
- Only simple `case <const>:` nodes participate (skip relational case blocks,
  mirroring the duplicate-check restriction at `:252`).
- For `*enum` operands, don't require `NOTHING` coverage in v1 (note as future
  refinement).

**Tests:** new `examples/test/qore/diagnostics/switch-enum-exhaustive.qtest`:
all-covered (no warn), missing-member (warn lists it), `default:` present (no
warn), relational case (no warn), non-enum operand (no warn).

**Risk:** low-medium — must not regress existing switch tests; run the full
`qore/` suite. **Wire format:** none.

---

## Phase 4 — Source column tracking (foundation)

**Goal:** thread the columns flex/bison already compute through to
`QoreProgramLocation`, enabling precise diagnostics and code frames. This is the
one structurally invasive phase.

- Extend `QoreProgramLineLocation` (`QoreLibIntern.h:358`) with `int16_t
  start_column / end_column` (default -1 = unknown). Keep size impact minimal
  (int16_t matches existing line fields).
- Add a `getLocation(start_line, end_line, start_col, end_col)` overload on
  `qore_program_private`; update `get_loc` (`scanner.lpp:72`) to pass
  `yylloc->first_column/last_column`. Old overload remains (columns = -1) so
  programmatic location creators don't all need updating at once.
- Audit location-cloning paths so columns propagate (or default safely to -1).
- Reflection: expose columns on `QoreExternalProgramLocation`
  (`QoreReflection.h:193`) and any `getSourceLocation()` Qore API, additively.

**Tests:** a parser test asserting reported columns for a known snippet (once
surfaced via Phase 6 structured output or reflection).

**Risk:** medium — touches a widely-used struct; columns must degrade to -1
everywhere they aren't set, and existing line-only behavior must be unchanged.
Run full suite + valgrind (C++ change). **Wire format:** none (parse-time
struct; not serialized in AOT).

**Outcome (implemented):** columns are carried by `QoreProgramLineLocation`
(`start_column`/`end_column`, -1 = unknown), interned distinctly (the fragile
`operator<` was rewritten as a proper lexicographic ordering including columns),
and threaded from the scanner's `get_loc` plus all 337 `getLocation(@…)` grammar
call sites (mechanically transformed: the column expr is the line expr with
`first_line→first_col`/`last_line→last_col`). **Parse-error and AST-node
locations now carry accurate columns** (verified: an undefined bareword reports
its exact column span). Exposed additively via the `SourceLocationInfo` hashdecl
(`column`/`endcolumn`).

**Known follow-on:** reflection-of-*declaration* source locations (function
variants, classes, hashdecls, constants, members) build their `QoreProgramLocation`
from bare line ints inside `UserSignature`/variant/member constructors, so they
report `column == -1`. Threading columns through those constructor chains is a
separate, bounded effort and is NOT required by the diagnostic features (Phases
6/8 consume parse-error/AST-node locations, which do carry columns).

---

## Phase 5 — Stable error codes + internal structured record

**Goal:** give diagnostics a stable, documentable identity and an internal
structured form, without yet exposing an API.

- Define a `QoreDiagnostic` struct (internal, `qore_program_private.h` or new
  header): `severity` (error/warning), `code` (stable string id, e.g.
  `"UNDEFINED-CLASS"`, `"NONEXHAUSTIVE-SWITCH"`), `warn_bit` (int, -1 for
  errors), location (file/source/lines/columns), `message`, `*hint`,
  `suggestions` (vector<string>).
- Add optional `code`/`hint`/`suggestions` parameters to new
  `parse_error_ex(...)` / `makeParseException` / `makeParseWarning` overloads.
  Existing call sites keep working (code defaults to generic
  `"PARSE-EXCEPTION"`); incrementally tag the **high-value** sites first:
  the Phase 1–3 identifier + switch errors. Do **not** attempt to tag every
  call site at once — phase the rest opportunistically.
- At the chokepoint, when capture is enabled (Phase 6), build a
  `QoreDiagnostic` alongside the existing `ParseException`.

**Tests:** assert the `code` field on tagged diagnostics (via Phase 6 API).

**Risk:** low — additive overloads. **Wire format:** none.

---

## Phase 6 — Per-`Program` structured diagnostics API  ← AI-facing core

**Goal:** make structured diagnostics settable and retrievable per `Program`, as
the user requires, via `lib/QC_Program.qpp`.

- New hashdecl in `QC_Program.qpp` (declared in qpp so it is documented, like
  `StatInfo` in `ql_file.qpp`):
  ```
  hashdecl ParseDiagnosticInfo {
      string severity;          # "error" | "warning"
      string code;              # stable id, e.g. "UNDEFINED-CLASS"
      *int warningCode;         # QP_WARN_* bit for warnings, NOTHING for errors
      string file;
      *string source;
      int startLine;
      int endLine = -1;
      int startColumn = -1;     # -1 when unknown (pre-Phase-4 paths)
      int endColumn = -1;
      string message;
      *string hint;
      list<string> suggestions = ();
  }
  ```
- New `Program` methods in `QC_Program.qpp`:
  - `nothing Program::setParseDiagnosticsCollected(bool collect = True)` —
    enables structured capture for subsequent parse actions on this Program.
  - `list<hash<ParseDiagnosticInfo>> Program::getParseDiagnostics()` — returns
    all diagnostics (errors + warnings) collected during the last parse action,
    in source order. Cleared at the start of each parse action.
  - `bool Program::parseDiagnosticsCollected()` — query the flag.
- C++ side: `qore_program_private` gains `bool collect_diagnostics` and
  `std::vector<QoreDiagnostic>`. The chokepoint (`makeParseException` /
  `makeParseWarning`), when the flag is set, appends a record. A converter turns
  `QoreDiagnostic` → `hash<ParseDiagnosticInfo>`.
- This is **purely additive and opt-in**: default-off, no behavior change,
  errors still throw/print as today. Tools call `setParseDiagnosticsCollected()`,
  run a parse inside try/catch, then `getParseDiagnostics()` to get the full
  machine-readable list (including suggestions from Phases 1–2 and the enum
  warning from Phase 3).

**Decisions (confirmed 2026-06-07):**
- API shape: **collection-after-parse** — `setParseDiagnosticsCollected()` +
  `getParseDiagnostics()`. No callback variant (parse-lock reentrancy risk).
- **No** parse-option bit — collection is a dedicated tooling toggle, not a
  capability restriction.
- Enum-exhaustiveness warning (Phase 3) ships **on in the default warning mask**
  (`QP_WARN_DEFAULT`), since it is high-signal and always silenced by a
  `default:` case.

**Tests:** `examples/test/qore/diagnostics/structured-api.qtest` — enable
collection, parse code with an undefined class + a non-exhaustive switch, assert
the returned list has the right `code`, `severity`, location, and `suggestions`.

**Risk:** low — opt-in, additive. **Wire format:** none.

---

## Phase 7 — Standalone `qore` JSON diagnostic emitter

**Goal:** let AI tools that drive the `qore` binary directly (not via embedded
`Program`) get structured output.

- Add a CLI flag (e.g. `--diag-format=json`) and/or env var
  (`QORE_DIAG_FORMAT=json`) to `qore-main.cpp` that, on parse, enables
  collection and emits the `QoreDiagnostic` list as JSON to stderr instead of
  (or alongside) the human-formatted error.
- JSON shape mirrors `ParseDiagnosticInfo` exactly, so the two channels agree.

**Tests:** a shell/qtest that runs `qore --diag-format=json` on a bad script and
asserts parseable JSON with expected fields.

**Risk:** low. **Wire format:** none.

---

## Phase 8 — Code-frame snippets (human polish)

**Goal:** Rust-style rendered errors with the source line and a caret under the
offending span (depends on Phase 4 columns).

- Optional rendering mode for the human error formatter: show the source line +
  `^~~~` underline using start/end columns when available; fall back to
  line-only when columns are -1.
- Gate behind a flag/env var so default output is unchanged unless requested.

**Tests:** snapshot a rendered frame for a known error.

**Risk:** low — presentation only, behind a flag. **Wire format:** none.

---

## Phase 9 — Closest-overload suggestions on call-resolution errors

**Goal:** when a call fails to match any variant, list candidate signatures and
flag the diverging argument.

- At the overload-resolution error site, enumerate the function/method's
  variants, render their signatures, and indicate which argument position
  diverged (type mismatch / arity). Emit as `hint` + structured `suggestions`.

**Tests:** snippet calling a function with a wrong-typed arg asserts the
candidate signature appears.

**Risk:** low-medium (resolution code is intricate). **Wire format:** none.

---

## Sequencing & dependencies

```
Phase 0 (harness)
   ├─ Phase 1 (edit-distance + bareword/class)         ── independent, ship first
   │     └─ Phase 2 (other identifier sites)
   ├─ Phase 3 (enum-exhaustive switch)                 ── independent
   ├─ Phase 4 (columns) ── foundation
   │     ├─ Phase 8 (code frames)
   │     └─ feeds column data into ▼
   └─ Phase 5 (codes + internal record)
         └─ Phase 6 (per-Program structured API)  ◄ AI core
               └─ Phase 7 (CLI JSON emitter)
   Phase 9 (overload suggestions) — independent, any time after Phase 5
```

Recommended ship order: **1 → 3 → 2 → 4 → 5 → 6 → 7 → 8 → 9**, each as a focused
commit with its own `.qtest`. Phases 1 and 3 deliver immediate value with no
foundation work; 4–7 build the AI-facing structured pipeline.

## Cross-cutting requirements

- Per global CLAUDE.md: run tests (`--enable-debug`) before each commit; run
  **valgrind** on affected tests after the C++ changes in Phases 1, 3, 4, 5
  (use `qore -b` to disable signals); copyright 2026; brackets on all control
  statements; type-safe, exception-safe code.
- After Phase 4/6, update language docs (`doxygen/`) and `QC_Program.qpp` doc
  blocks; add `@since` 2.3 tags. No per-commit release notes on the branch.
- Keep `modules/astparser` (tree-sitter grammar) in mind only if any *syntax*
  changes — none are planned here (all changes are semantic/diagnostic), so no
  grammar regeneration is required.
