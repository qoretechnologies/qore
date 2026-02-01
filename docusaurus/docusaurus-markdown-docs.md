# Goal: Migrate Qore/Qorus documentation generation from C++ transpilation (Qdx) to a native Qore-based "Docs-as-Code" pipeline targeting Docusaurus.

## Work Done
1.  **Exploration:**
    *   Identified that `astparser` module can natively parse Qore code, exposing namespaces, classes, methods, and Doxygen-style comments.
    *   Confirmed `Qdx` approach was suboptimal and `Docusaurus` + `Markdown` is the desired target.

2.  **Implementation (`generate_docs.q`):**
    *   Created a standalone CLI tool in Qore.
    *   **Features:**
        *   Parses Qore source files (`.q`, `.qm`, `.qc`).
        *   Generates Docusaurus-compatible Markdown.
        *   Handles Frontmatter (`id`, `title`, `sidebar_label`).
        *   Extracts full method signatures (return types, params with types/names).
        *   **Doc Parsing:** Robustly strips Doxygen markers (`#!`, `/**`) and converts tags (`@param`, `@code`, `\c`, `@ref`) to Markdown.
        *   **AST Handling:** Safely navigates complex AST structures (handling missing names, nulls, recursion).
        *   **Error Handling:** Catches and logs errors per-file/node without crashing the entire build.

3.  **Refinement:**
    *   Fixed crashes due to strict type checking (e.g., `regex` capture groups returning `nothing` vs `string`).
    *   Fixed "empty signature" bugs by implementing recursive parameter collection (`collectParams`).
    *   Fixed formatting issues where Doxygen code blocks leaked into summaries.
    *   Verified output against `RestClient.qm` (Qore core) and `SoapClient` (Module).

4.  **Integration:**
    *   Configured `qorus-docs` sidebars to include the new "Reference" section.
    *   Successfully generated docs for `qore` core, `module-*`, and `Qorus`.

## Outstanding / Next Steps
1.  **Build System Integration:**
    *   Add `generate_docs.q` to the official build (CMake/Makefiles) for `qore` and modules.
    *   Ensure it runs in CI to keep docs up-to-date.

2.  **Polish:**
    *   **Type Linking:** Auto-link types (e.g., `hash<auto>` or custom classes) in method signatures to their definition pages.
    *   **Inherited Methods:** Currently lists inherited classes; could optionally expand to list inherited methods explicitly if desired (though standard Docusaurus practice often links to parent).
    *   **Complex Types:** Better formatting for complex types like `hash<string, list<int>>` in signatures (currently does basic formatting).

3.  **Review:**
    *   Manual review of generated pages for formatting oddities in complex Doxygen blocks.
    *   Verify cross-module linking works as expected with the file structure.
