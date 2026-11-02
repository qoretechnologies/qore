# Language Release Notes

The release notes of the Qore language and its bundled modules are in
`doxygen/lang/900_release_notes.dox.tmpl` (page `release_notes`). A major release has hundreds of entries, so the
page only stays readable if every entry is in its place. This document defines that structure, the editorial policy
for what goes into it, and the checker that enforces the structure.

## Structure

Each release is one `@section`, newest first. Its content is, in this order and with nothing else:

1. `@par Release Summary`: one paragraph, or a `@parblock ... @endparblock` for a major release (themes and
   headline-level highlights only, each one line, linking to the subsection entry or reference page).
2. The subsections that the release needs, in this order, with these ids (`<section>` is the section id, such as
   `qore_3_0_0`):

| Subsection id | Title | Content |
|---|---|---|
| `<section>_compatibility` | Changes That Can Affect Backwards-Compatibility | changes to released behavior that can break existing code |
| `<section>_new_features` | New Features in Qore | new language, runtime, API and tool features |
| `<section>_deprecated` | Deprecated Features | features deprecated in this release |
| `<section>_known_limitations` | Known Limitations | limitations of the release |
| `<section>_updated_modules` | Updated Modules | changes to modules released before, one bullet per module |
| `<section>_new_builtin_modules` | New Builtin Modules | new binary modules built with Qore |
| `<section>_new_user_modules` | New User Modules | new modules in `qlib/` |
| `<section>_bug_fixes` | Bug Fixes in Qore | fixes to released behavior |

Every entry of a subsection is one of:

- a bullet (`- ...`), with continuation lines and nested bullets indented further; this is the form of the
  updated-module, new-module and bug-fix entries;
- a `@par Title` block, used for the compatibility and new-feature entries: a single paragraph after the title, or
  `@parblock ... @endparblock` for more than one paragraph, a list or code. Doxygen ends a `@par` at the first blank
  line, so a second paragraph without `@parblock` is detached from its title. An `@anchor` may precede the `@par`.

An entry is a summary: usually one sentence that says what changed and links (`@ref`) to the reference
documentation, which holds the full description; a list item or paragraph has at most four lines. A major release
has hundreds of entries, so a minor or esoteric change gets one line, related changes are grouped in one entry, and
an updated module gets at most a few one-line highlights and a link to the module's own release notes, which list
all of its changes. Long explanations, examples beyond a short one and design rationale belong in the
reference documentation (`doxygen/lang/*.dox.tmpl`, the API documentation in `lib/*.qpp` and `qlib/`), not here.

A new entry goes into the subsection that matches it, never directly after the `@section` line or after the release
summary: text there is outside every subsection and breaks the structure and the table of contents of the page.

## Editorial policy

The notes of a release describe what changed relative to the previous *release*, from the point of view of a user of
that release:

- A fix or change to functionality that no release has shipped (added during the current development cycle) is not
  an entry. The functionality's own entry, if any, may be edited to describe its final state, and its reference
  documentation describes its behavior.
- An entry does not describe development history ("no longer", "previously") of unreleased functionality.
- A change to a released module also goes into that module's own release notes (the `@page` of its release history
  in its `.qm` file or `modules/<module>/docs/release-notes.dox.tmpl`).

## Checker

`doxygen/check-release-notes.py` checks the structure of `900_release_notes.dox.tmpl`:

- every release section begins with `@par Release Summary`, and nothing follows the summary before the first
  subsection (all sections);
- from Qore 3.0.0 on, the subsections are the standard ones above, in the standard order, each at most once, and
  every paragraph in them is a bullet or a `@par` block (sections of older releases keep their historical
  subsection ids);
- from Qore 3.0.0 on, no list item (without its nested items) or paragraph, including those of the release summary,
  is longer than four lines;
- every `@parblock`, `@code`, `@verbatim`, `@htmlonly`, `@dot` and `@msc` in a subsection or summary is closed in it;
- no line contains a backslash before a double quote (`\\"`): doxygen reads it as an escaped quote, which leaves the
  quotes of the paragraph unbalanced, so that the markup after it (such as `</tt>`) is not processed and the rest of
  the paragraph is rendered in a monospace font.

It runs in the `check-module-release-notes` CI job, after its tests in `examples/test/cmake/test_release_notes.py`,
and in the build as the `check-release-notes` target, which `docs-lang`, `docs-lang-final` and
`docs-lang-final-fast` depend on when CMake finds a Python 3 interpreter. Run it directly with
`python3 -I doxygen/check-release-notes.py`, and check the rendered page with
`cmake --build build --target docs-lang-final-fast` (`build/docs/lang/html/release_notes.html`).
