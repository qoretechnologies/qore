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

    private:internal parseInit() {
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
        tree = NOTHING;
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
        string filePath = uri.substr(Document::getFileUriPrefix().size()); # trim "file://" see Document::getFileUriPrefix()
        filePath = Qore::decode_url(filePath);
        tree = parser.parseFile(filePath);
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

    #! The document has changed.
    didChange(hash<auto> changes) {
        # Modify document text.
        # Using whole document sync here.
        text = changes.text;
        delete cachedLines;

        # Re-parse the document.
        parseText();
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
    *hash<auto> hoverInfo(int kind, hash<auto> Position) {
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

        AstTreeSearcher searcher();
        if (tree) {
            return searcher.hoverInfo(tree, kind, line, char);
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

    #! Set conditional parsing defines and re-parse
    /** @param new_defines list of define names (e.g., ("MyDefine", "Debug")); pass NOTHING or empty list to clear
    */
    setDefines(*list<string> new_defines) {
        defines = new_defines ?? ();
        delete cachedLines;
        parseText();
    }

    #! Get the current conditional parsing defines
    /** @return list of define names
    */
    list<string> getDefines() {
        return defines;
    }
}

} # namespace QLS
