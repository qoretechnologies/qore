# Qore code highlighting in the doxygen documentation

Issue: [#5573](https://github.com/qoretechnologies/qore/issues/5573)

## Problem

Qore documentation marks code with `@code{.py}` so that doxygen handles `#` comments, and doxygen then highlights it
with its Python lexer, which does not know Qore: types, parse directives, regex literals and operators, date/time
literals, `$1`, hashdecls, `sub`, `inherits` etc. are highlighted wrongly or not at all.

Doxygen has no plugin interface for code highlighting (`@code{.ext}` and `EXTENSION_MAPPING` only select a built-in
lexer), and its HTML (`<div class="fragment"><div class="line">...`) does not record the language of a fragment.

## Design

The highlighting is replaced when the HTML is post-processed with `qdx --post`, which every Qore documentation build
already runs.

1. **Highlight query** — `modules/astparser/grammars/tree-sitter-qore/queries/highlights.scm` uses the common
   tree-sitter capture names, so editors using tree-sitter can use it too; it is installed in
   `share/qore/treesitter/tree-sitter-qore/queries`.  As in tree-sitter-highlight, the first pattern wins for a node
   and a capture of an inner node overrides the outer one, so context-specific patterns (type names inside types,
   streaming keywords as member names) precede the general keyword patterns.
2. **astparser** — `cmake/EmbedHighlightQuery.cmake` compiles the query into the module.  `AstHighlighter` compiles
   it once (thread-safe, freed at module deletion), evaluates the predicates `#eq?`, `#any-of?`, `#match?` and their
   `not-` forms, which tree-sitter leaves to the caller, and paints captures by byte (outer nodes first, the lowest
   pattern index last), giving non-overlapping ranges.  `AstTree::getHighlights()` and `AstTree::getErrorRanges()`
   return character offsets in the UTF-8 source.  The conditional-directive preprocessing writes one space per
   character (not per byte) for the lines that it blanks, so character offsets match the original text.
3. **Qdx** — `QoreCodeFragmentHighlighter` extracts the text of each fragment (decoding entities and recording
   `<a>` links, dropping doxygen's `<span>`s), parses it, and emits the lines again with doxygen's CSS classes
   (`keyword`, `keywordtype`, `keywordflow`, `comment`, `stringliteral`, `charliteral`, `preprocessor`), so the
   theme and its dark mode apply.  Links are re-emitted around the same text, with highlighting inside them;
   tooltip `<div class="ttc">` blocks that doxygen adds after the lines are kept.
   `QorePostProcessingInputStreamLineIterator` buffers the lines of a fragment, after the per-line post-processing.

## Deciding which fragments are Qore

The HTML does not say, so a fragment is left with doxygen's highlighting when:

- more than 20% of its non-whitespace characters are in parse errors (code in other languages, program output);
  if the code has errors, it is also parsed with a `;` appended, since documentation often shows a bare expression
- it is a source listing (`class="lineno"`)
- it contains HTML that doxygen does not generate for code fragments, or entities of non-ASCII characters
- the page documents Python or Java sources (doxygen lists them at the end of the page, so `qdx` checks the file
  before processing it)
- highlighting is disabled: `qdx --post --no-highlight`, `QORE_DOC_NO_CODE_HIGHLIGHT=1`, or the
  `QORE_DOCS_NO_CODE_HIGHLIGHT` cmake variable of the documentation macro in `cmake/QoreMacros.cmake` (for
  documentation of other languages, such as the python module)

Measured on the docs of this repository: of 3337 fragments in the language reference, 3077 are highlighted, 174
are source listings and 86 are left (shell commands, C++, REPL sessions, code mixed with output); every
highlighted fragment keeps its text, lines and links.  `qdx --post` on the 2141 files of the language reference
takes 7.3 s instead of 5.7 s.
