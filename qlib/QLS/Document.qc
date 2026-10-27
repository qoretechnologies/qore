# -*- mode: qore; indent-tabs-mode: nil -*-

/*  Document.qc Copyright 2017 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

public namespace QLS {

#! Represents one open document (source file).
public class Document {
    private {
        #! The document's URI.
        string uri;

        #! The document's name (without path).
        string docName;

        #! The document's language identifier.
        string languageId;

        #! The version number of this document (it will strictly increase after each change, including undo/redo).
        int version;

        #! The content of the opened text document.
        *string text;

        #! AST parser.
        astparser::AstParser parser;

        #! AST tree containing all the syntax info.
        *astparser::AstTree tree;

        #! Whether the document has been parsed successfully by the program.
        bool parseOk;

        #! Parse error description (in case a parse error occured).
        *string parseError;

        #! Cached split lines for efficient line access.
        *list<string> cachedLines;

        #! Conditional parsing defines (e.g., for %ifdef directives).
        list<string> defines = ();

        #! Cached parse directive options (invalidated on content change).
        *int cachedParseOptions;

        #! Cached symbol index (invalidated on content change).
        *FileIndex cachedIndex;

        #! Cached set of identifiers in the text (invalidated on content change).
        *hash<string, bool> cachedIdentifiers;
    }

    constructor(string n_uri, string n_text, string n_languageId, int n_version) {
        uri = n_uri;
        docName = basename(uri);
        text = n_text;
        languageId = n_languageId;
        version = n_version;

        parseText();
    }

    constructor(string n_uri) {
        uri = n_uri;
        docName = basename(uri);
        languageId = "qore";
        version = 0;

        parseFile();
    }

    /** Do not use "file://" prefix directly. Always
        use this function because of:
        https://github.com/qorelanguage/qore-vscode/issues/7
        https://blogs.msdn.microsoft.com/ie/2006/12/06/file-uris-in-windows/
     */
    static string getFileUriPrefix() {
        return Qore::Platform.Windows ? "file:///" : "file://";
    }

    /** Create a URI encoded string from a raw path.
        @param raw_path a string with a file path

        The final path is:
            - prefixed with Document::getFileUriPrefix() if required
            - encoded with Qore::encode_url()
     */
    static string getFileUri(string raw_path) {
        # if the input is already a file URI, return it unchanged to avoid
        # double-encoding the scheme and slashes
        if (raw_path =~ /^file:\/\//) {
            return raw_path;
        }
        return Document::getFileUriPrefix() + Qore::encode_url(raw_path);
    }

    #! Returns the file path for a file URI created with getFileUri()
    /** @param uri a file URI

        @return the decoded file path
    */
    static string getFilePath(string uri) {
        # trim "file://" see Document::getFileUriPrefix()
        return Qore::decode_url(uri.substr(Document::getFileUriPrefix().size()));
    }

    private:internal parseInit() {
        delete cachedIndex;
        delete cachedIdentifiers;
        delete parseError;
        delete tree;
        parseOk = True;
        parser = new astparser::AstParser();
        if (defines) {
            parser.setConditionalParsing(True);
            parser.setDefines(defines);
        }
    }

    private:internal handleParseErrors() {
        # NOTE: do NOT set tree = NOTHING here. Tree-sitter's error recovery produces
        # partial trees with ERROR nodes that still contain valid structure (classes,
        # functions, variables). Keeping the tree enables CST features (scope resolution,
        # symbol search, go-to-definition, etc.) even when the source has parse errors,
        # which is the normal state while actively editing code.
        # When the parser returned NOTHING (total failure), tree is already NOTHING.
        parseOk = False;
        parseError = sprintf("Error parsing text document '%s'\n\n", uri);
        if (parser.getErrorCount()) {
            parseError += "Errors:\n\n";
            *list<auto> diagnostics = parser.getDiagnostics();
            int i = 1;
            foreach hash<auto> diag in (diagnostics) {
                parseError += sprintf("%d) %s:%d:%d: %s\n\n", i++, docName,
                    diag.range.start.line,
                    diag.range.start.character,
                    diag.message);
            }
        }
    }

    #! Parse document from a file.
    private:internal parseFile() {
        parseInit();
        tree = parser.parseFile(Document::getFilePath(uri));
        if (!tree || parser.getErrorCount()) {
            handleParseErrors();
        }
    }

    #! Parse the text of the document.
    private:internal parseText() {
        parseInit();
        tree = parser.parseString(text);
        if (!tree || parser.getErrorCount()) {
            handleParseErrors();
        }
    }

    string getUri() { return uri; }
    string getLanguageId() { return languageId; }
    int getVersion() { return version; }
    *string getText() { return text; }

    #! Get count of parse errors.
    int getParseErrorCount() {
        return parser.getErrorCount();
    }

    #! Get parse diagnostics (errors).
    *list<auto> getDiagnostics() {
        return parser.getDiagnostics();
    }

    #! Applies a change to the document and parses it again
    /** @param change an LSP \c TextDocumentContentChangeEvent: a hash with a \c text key and, for an incremental
        change, a \c range key with the range that the text replaces; without a range, the text replaces the whole
        document
    */
    didChange(hash<auto> change) {
        text = change.range
            ? Document::applyEdit(text ?? "", change.range, change.text)
            : change.text;
        delete cachedLines;
        delete cachedParseOptions;

        # Re-parse the document.
        parseText();
    }

    #! Returns the text with the given range replaced
    /** @param text the text
        @param range the LSP range to replace; positions use UTF-16 code units like LSP, and positions beyond the end
        of a line or of the text are clamped
        @param newText the text that replaces the range

        @return the new text
    */
    static string applyEdit(string text, hash<auto> range, string newText) {
        int start = Document::getOffset(text, range.start);
        int end = Document::getOffset(text, range.end);
        if (end < start) {
            end = start;
        }
        return text.substr(0, start) + newText + text.substr(end);
    }

    #! Returns the character offset in the text of an LSP position
    /** @param text the text
        @param position an LSP position; the character is in UTF-16 code units

        @return the character offset; a position beyond the end of the line is at the end of the line (before
        \c "\r\n" or \c "\n"), and a line beyond the end of the text is at the end of the text
    */
    static int getOffset(string text, hash<auto> position) {
        int lineStart = 0;
        for (int line = 0; line < position.line; ++line) {
            int nl = text.find("\n", lineStart);
            if (nl == -1) {
                return text.length();
            }
            lineStart = nl + 1;
        }
        int lineEnd = text.find("\n", lineStart);
        if (lineEnd == -1) {
            lineEnd = text.length();
        } else if (lineEnd > lineStart && text[lineEnd - 1] == "\r") {
            --lineEnd;
        }
        # count UTF-16 code units: characters outside the Basic Multilingual Plane take two
        int units = 0;
        int offset = lineStart;
        while (offset < lineEnd && units < position.character) {
            units += text[offset].getUnicode() > 0xffff ? 2 : 1;
            ++offset;
        }
        return offset;
    }

    changeVersion(int n_version) {
        version = n_version;
    }

    #! Hover info for when no symbol info found or parse error happened.
    hash<auto> hover(hash<auto> Position) {
        int line = Position.line;
        int char = Position.character;
        if (!parseOk) {
            return {
                "contents": parseError,
                "range": {
                    "start": {
                        "line": line,
                        "character": char,
                    },
                    "end": {
                        "line": line,
                        "character": char + 1,
                    },
                },
            };
        }

        return {
            "contents": (),
        };
    }

    #! Find info about symbol at the passed position.
    *hash<auto> findSymbolInfo(hash<auto> Position) {
        AstTreeSearcher searcher();
        if (tree) {
            return searcher.findSymbolInfo(tree, Position.line, Position.character);
        }
        return NOTHING;
    }

    #! Return hover info about symbol at the passed position.
    /** The syntax tree of a document with syntax errors still has its declarations, so they have hover info while
        the document is edited.

        @param kind the symbol kind of the declaration
        @param Position LSP position in the declaration

        @return the hover info from @ref astparser::AstTreeSearcher::hoverInfo(): the signature in \c description
        and the doc comment in \c docComment, or NOTHING if there is no declaration of the kind at the position
    */
    *hash<auto> hoverInfo(int kind, hash<auto> Position) {
        if (tree) {
            AstTreeSearcher searcher();
            return searcher.hoverInfo(tree, kind, Position.line, Position.character);
        }
        return NOTHING;
    }

    #! Find references of the symbol at the passed position.
    *list<auto> findReferences(hash<auto> Position, bool includeDecl) {
        int line = Position.line;
        int char = Position.character;
        AstTreeSearcher searcher();
        if (tree) {
            return searcher.findReferences(tree, uri, line, char, includeDecl);
        }
        return NOTHING;
    }

    #! Find references across multiple documents.
    /** @param Position LSP position (line, character)
        @param includeDecl whether to include the declaration
        @param otherDocs hash mapping URI → AstTree for other documents
        @return list of {uri, range} locations, or NOTHING
    */
    *list<auto> findReferencesCrossDoc(hash<auto> Position, bool includeDecl,
            *hash<auto> otherDocs) {
        AstTreeSearcher searcher();
        if (tree) {
            return searcher.findReferencesCrossDoc(tree, uri, Position.line,
                Position.character, includeDecl, otherDocs);
        }
        return NOTHING;
    }

    #! Find all symbols.
    /**
        @param fixSymbols whether to fix symbol infos
        @param bareNames whether to return bare symbol names (without namespace and class prefixes)
        @return list of symbols
    */
    *list<auto> findSymbols(bool fixSymbols = True, bool bareNames = False) {
        AstTreeSearcher searcher();
        if (tree) {
            return searcher.findSymbols(tree, uri, fixSymbols, bareNames);
        }
        return NOTHING;
    }

    *list<auto> getNodesInfo() {
        return tree ? tree.getNodesInfo() : NOTHING;
    }

    #! Find matching symbols.
    /**
        @param query search query
        @param exactMatch whether to only find exact matches
        @param fixSymbols whether to fix symbol infos
        @param bareNames whether to return bare symbol names (without namespace and class prefixes)

        @return list of symbols
    */
    list<auto> findMatchingSymbols(string query, bool exactMatch = False, bool fixSymbols = True,
            bool bareNames = False) {
        AstTreeSearcher searcher();
        if (tree) {
            *list<auto> symbols = searcher.findMatchingSymbols(tree, uri, query, exactMatch, fixSymbols, bareNames);
            return symbols ?? ();
        }
        return ();
    }

    #! Find detailed scope symbols at the given position
    /** @param Position LSP position (line, character)
        @return list of CSTSymbolDetail hashes, or NOTHING
    */
    *list<auto> findScopeSymbolsDetailed(hash<auto> Position) {
        AstTreeSearcher searcher();
        if (tree) {
            return searcher.findScopeSymbolsDetailed(tree, uri, Position.line, Position.character);
        }
        return NOTHING;
    }

    #! Get the type of the symbol at the given position
    /** @param Position LSP position (line, character)
        @return type name string, or NOTHING
    */
    *string getSymbolType(hash<auto> Position) {
        AstTreeSearcher searcher();
        if (tree) {
            return searcher.getSymbolType(tree, Position.line, Position.character);
        }
        return NOTHING;
    }

    #! Returns the receiver chain of the expression at a position
    /** @param Position LSP position (line, character)
        @return the links of the chain from the outermost receiver, or NOTHING if there is no expression at the
        position; see @ref astparser::AstTreeSearcher::getExpressionChain()
    */
    *list<hash<AstExpressionLink>> getExpressionChain(hash<auto> Position) {
        if (tree) {
            AstTreeSearcher searcher();
            return searcher.getExpressionChain(tree, Position.line, Position.character);
        }
        return NOTHING;
    }

    #! Returns the symbol index of the document
    /** The index is created when first requested and discarded when the document changes.
    */
    FileIndex getIndex() {
        if (!cachedIndex) {
            cachedIndex = new FileIndex(self, text ? text.size() : 0);
        }
        return cachedIndex;
    }

    #! Returns the set of identifiers in the document text
    /** The set is created when first requested and discarded when the document changes; documents parsed from a
        file without text return an empty set.
    */
    hash<string, bool> getIdentifiers() {
        if (!exists cachedIdentifiers) {
            cachedIdentifiers = text ? ((map {$1: True}, (text =~ x/([A-Za-z_][A-Za-z0-9_]*)/g)) ?? {}) : {};
        }
        return cachedIdentifiers;
    }

    #! Get the underlying AST tree (for cross-document resolution)
    /** @return the AstTree object, or NOTHING if not parsed
    */
    *astparser::AstTree getTree() {
        return tree;
    }

    #! Resolve definition location(s) for the symbol at position
    /** @param Position LSP position {line, character}
        @return list of {uri, range} locations, or NOTHING
    */
    *list<auto> resolveDefinition(hash<auto> Position) {
        AstTreeSearcher searcher();
        if (tree) {
            return searcher.resolveDefinition(tree, uri, Position.line, Position.character);
        }
        return NOTHING;
    }

    #! Resolve definition across multiple documents
    /** @param Position LSP position {line, character}
        @param otherDocs hash mapping URI → AstTree for other open documents
        @return list of {uri, range} locations, or NOTHING
    */
    *list<auto> resolveDefinitionCrossDoc(hash<auto> Position, *hash<auto> otherDocs) {
        AstTreeSearcher searcher();
        if (tree) {
            return searcher.resolveDefinitionCrossDoc(tree, uri, Position.line,
                Position.character, otherDocs);
        }
        return NOTHING;
    }

    #! Find members of a type (class, namespace, hashdecl, enum)
    /** @param typeName name of the type to look up
        @param includeInherited whether to include inherited members
        @return list of CSTSymbolDetail hashes, or NOTHING
    */
    *list<auto> findTypeMembers(string typeName, bool includeInherited = True) {
        AstTreeSearcher searcher();
        if (tree) {
            return searcher.findTypeMembers(tree, uri, typeName, includeInherited);
        }
        return NOTHING;
    }

    #! Find node and all ancestor nodes at the given position
    /** @param Position LSP position (line, character)
        @return list of range hashes from innermost node to root, or NOTHING
    */
    *list<auto> findNodeAndParents(hash<auto> Position) {
        AstTreeSearcher searcher();
        if (tree) {
            return searcher.findNodeAndParents(tree, Position.line, Position.character);
        }
        return NOTHING;
    }

    #! Get superclass names for a class at the given position
    /** @param Position LSP position (line, character)
        @return list of superclass name strings, or NOTHING
    */
    *list<string> getSuperclassNames(hash<auto> Position) {
        AstTreeSearcher searcher();
        if (tree) {
            return searcher.getSuperclassNames(tree, Position.line, Position.character);
        }
        return NOTHING;
    }

    #! Collect semantic tokens from the parse tree
    /** @param startLine start of range (0-indexed, inclusive), or -1 for beginning
        @param endLine end of range (0-indexed, inclusive), or -1 for end
        @return list of {line, startChar, length, tokenType, tokenModifiers} hashes, or NOTHING
    */
    *list<auto> semanticTokens(int startLine = -1, int endLine = -1) {
        if (!tree) {
            return NOTHING;
        }
        AstTreeSearcher searcher();
        return searcher.getSemanticTokens(tree, startLine, endLine);
    }

    #! Get all comments from the AST tree
    /** @return list of comment hashes, or NOTHING
    */
    *list<auto> getComments() {
        return tree ? tree.getComments() : NOTHING;
    }

    #! Compute folding ranges for the document
    /** @return list of {startLine, endLine, kind} hashes for foldable regions
    */
    *list<auto> foldingRanges() {
        if (!tree) {
            return NOTHING;
        }

        # Set of foldable node types
        hash<string, bool> foldableTypes = {
            "class_declaration": True,
            "namespace_declaration": True,
            "function_declaration": True,
            "method_declaration": True,
            "constructor_declaration": True,
            "destructor_declaration": True,
            "hashdecl_declaration": True,
            "enum_declaration": True,
            "if_statement": True,
            "while_statement": True,
            "do_while_statement": True,
            "for_statement": True,
            "foreach_statement": True,
            "try_statement": True,
            "catch_clause": True,
            "switch_statement": True,
            # closures and literals in data structures, such as closures in nested hashes
            "closure_expression": True,
            "hash_literal": True,
            "paren_hash_literal": True,
            "list_literal": True,
        };

        list<auto> result = ();

        # Collect code folding ranges from CST nodes
        *list<auto> nodes = tree.getNodesInfo();
        if (nodes) {
            Document::collectFoldingRanges(nodes, foldableTypes, \result);
        }

        # Collect comment folding ranges
        *list<auto> comments = tree.getComments();
        if (comments) {
            foreach hash<auto> c in (comments) {
                # getComments() uses 1-indexed lines; LSP uses 0-indexed
                int startLine = c.loc.firstLine - 1;
                int endLine = c.loc.lastLine - 1;
                if (endLine > startLine) {
                    result += {
                        "startLine": startLine,
                        "endLine": endLine,
                        "kind": "comment",
                    };
                }
            }
        }

        return result;
    }

    #! Recursively collect folding ranges from CST nodes
    private static collectFoldingRanges(list<auto> nodes, hash<string, bool> foldableTypes,
            reference<list<auto>> result) {
        foreach hash<auto> node in (nodes) {
            if (foldableTypes{node.type}) {
                # getNodesInfo() uses 0-indexed lines
                int startLine = node.loc.start_line;
                int endLine = node.loc.end_line;
                if (endLine > startLine) {
                    result += {
                        "startLine": startLine,
                        "endLine": endLine,
                        "kind": "region",
                    };
                }
            }
            # Recurse into children
            if (node.children) {
                Document::collectFoldingRanges(node.children, foldableTypes, \result);
            }
        }
    }

    #! Get a single line of text from the document
    /** @param lineNum 0-indexed line number
        @return the line text, or NOTHING if out of range
    */
    *string getLine(int lineNum) {
        if (!text) {
            return NOTHING;
        }
        if (!cachedLines) {
            cachedLines = text.split("\n");
        }
        if (lineNum >= 0 && lineNum < cachedLines.size()) {
            return cachedLines[lineNum];
        }
        return NOTHING;
    }

    #! Returns the position of a declaration's name
    /** Declaration ranges start with modifiers and types (for example \c "int" in \c "int sub foo()"), while
        features that look up the declared symbol need the position of its name.

        @param name the declared name, without namespace or class prefixes
        @param range the LSP range of the declaration

        @return the position of the first occurrence of the name as a whole word in the range, or the start of the
        range if it is not found or the document has no text
    */
    hash<auto> getNamePosition(string name, hash<auto> range) {
        return findNamePosition(name, range) ?? {"line": range.start.line, "character": range.start.character};
    }

    #! Returns the position of a declaration's name, or @ref nothing if it is not found
    /** @param name the declared name, without namespace or class prefixes
        @param range the LSP range of the declaration

        @return the position of the first occurrence of the name as a whole word in the range, or @ref nothing if
        it is not found or the document has no text
    */
    *hash<auto> findNamePosition(string name, hash<auto> range) {
        int col = range.start.character;
        for (int line = range.start.line; line <= range.end.line; ++line, col = 0) {
            *string lineText = getLine(line);
            if (!exists lineText) {
                break;
            }
            for (int i = lineText.find(name, col); i != -1; i = lineText.find(name, i + 1)) {
                int end = i + name.length();
                if (line == range.end.line && end > range.end.character) {
                    break;
                }
                if ((!i || lineText[i - 1] !~ /^[A-Za-z0-9_$]$/)
                        && (end >= lineText.length() || lineText[end] !~ /^[A-Za-z0-9_]$/)) {
                    return {"line": line, "character": i};
                }
            }
        }
        return NOTHING;
    }

    #! Set conditional parsing defines and re-parse
    /** @param new_defines list of define names (e.g., ("MyDefine", "Debug")); pass NOTHING or empty list to clear
    */
    setDefines(*list<string> new_defines) {
        defines = new_defines ?? ();
        delete cachedLines;
        delete cachedParseOptions;
        parseText();
    }

    #! Get the current conditional parsing defines
    /** @return list of define names
    */
    list<string> getDefines() {
        return defines;
    }

    #! Get parse options derived from parse directives in the document text
    /** Scans the document text for lines starting with recognized parse directives
        (e.g., \c %modern, \c %strict-args, \c %no-thread-control) and accumulates the
        corresponding PO_* flags using the built-in @ref Qore::ParseOptionCmdStringMap
        "ParseOptionCmdStringMap". The result is cached and invalidated on content changes.

        @return bitfield of PO_* parse option flags, or 0 if no directives found
    */
    int getParseDirectiveOptions() {
        if (exists cachedParseOptions) {
            return cachedParseOptions;
        }
        int result = 0;
        if (text) {
            foreach string line in (text.split("\n")) {
                # Parse directives must start at column 0
                if (line.size() > 0 && line[0] == "%") {
                    # Extract the directive token (first %-prefixed word, strip leading %)
                    *list<auto> match = (line =~ x/^%([a-zA-Z][a-zA-Z0-9_-]*)/);
                    if (match) {
                        *int po = ParseOptionCmdStringMap{match[0]};
                        if (exists po) {
                            result |= po;
                        }
                    }
                }
            }
        }
        cachedParseOptions = result;
        return result;
    }
}

} # namespace QLS
