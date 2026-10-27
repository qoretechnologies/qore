# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SymbolIndex.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Parsed syntax trees of indexed files are cached up to this many bytes of source by default (8 MiB)
/** A syntax tree takes roughly 20 times the size of its source in memory.
*/
public const DefaultTreeCacheSize = 8 * 1024 * 1024;

#! A class declared in an indexed file
public hashdecl IndexedClass {
    #! The class name as declared
    string name;

    #! The class name qualified with its enclosing namespaces and classes
    string qualifiedName;

    #! The names of the parent classes as written in the declaration
    list<string> parents = ();

    #! The position of the class in the symbol list of the file
    int symbol;
}

#! Information about the symbol indexes and syntax trees held by a server
/** @see @ref QLS::QLSServer::getIndexInfo() "QLSServer::getIndexInfo()"
*/
public hashdecl IndexInfo {
    #! The number of indexed workspace files
    int workspaceFiles = 0;

    #! The number of symbols in the workspace index
    int workspaceSymbols = 0;

    #! Whether the standard module index is loaded
    bool stdModulesLoaded = False;

    #! The number of indexed standard module files
    int stdModuleFiles = 0;

    #! The number of open documents
    int openDocuments = 0;

    #! The number of syntax trees of indexed files in the cache
    int cachedTrees = 0;

    #! The total source size of the syntax trees in the cache
    int cachedTreeSourceSize = 0;

    #! The maximum total source size of the syntax trees in the cache
    int treeCacheSize = 0;

    #! The number of workspace and standard module files waiting to be indexed in the background
    int pendingFiles = 0;
}

#! Class inheritance relations, keyed by declared class name
/** Parent class names are reduced to the part after the last @c "::".
*/
public hashdecl ClassGraph {
    #! The parent class names of each class
    hash<string, hash<string, bool>> parents = {};

    #! The names of the classes that inherit each class
    hash<string, hash<string, bool>> children = {};
}

#! The symbols declared in a source file, without its syntax tree
/** The syntax tree of a file takes roughly 20 times the size of the source in memory, so files that are not open
    in the editor are kept in memory as an index of the symbols that they declare.  When a request needs the syntax
    tree of an indexed file, it is parsed again and kept in a @ref QLS::DocumentCache "DocumentCache".

    Symbols are stored in columns instead of one hash per symbol to keep the index small.

    @par Example
    @code{.py}
# index a file in a large workspace, then find where a class is declared without keeping its syntax tree
FileIndex index = FileIndex::fromFile(Document::getFileUri("/src/project/lib/InvoiceProcessor.qc"));
list<hash<auto>> decls = index.findMatchingSymbols("InvoiceProcessor", True);
    @endcode
*/
public class FileIndex {
    public {
        #! The number of integers stored per symbol: kind, start line, start character, end line, end character
        const RangeFields = 5;
    }

    private {
        #! The URI of the file
        string uri;

        #! The size of the source in bytes
        int sourceSize;

        #! The qualified name of each symbol, as returned by Document::findSymbols(True, False)
        list<string> names = ();

        #! The declared name of each symbol, as returned by Document::findSymbols(True, True)
        list<string> bareNames = ();

        #! The kind and range of each symbol, RangeFields integers per symbol
        list<int> ranges = ();

        #! The classes declared in the file
        list<hash<IndexedClass>> classes = ();

        #! Filter of the identifiers in the file; see astparser::AstTree::getIdentifierFilter()
        *binary identifierFilter;

        #! The LSP diagnostics of the syntax errors of the file, if any
        *list<hash<auto>> diagnostics;
    }

    #! Creates the index from a parsed document
    /** @param doc the parsed document; the index does not keep a reference to it
        @param sourceSize the size of the source in bytes
    */
    constructor(Document doc, int sourceSize) {
        uri = doc.getUri();
        self.sourceSize = sourceSize;
        *astparser::AstTree tree = doc.getTree();
        if (tree) {
            identifierFilter = tree.getIdentifierFilter();
        }
        # syntax errors are rare, so their diagnostics are kept for the diagnostics of files that are not open
        if (doc.getParseErrorCount()) {
            diagnostics = doc.getDiagnostics();
        }

        *list<auto> qualified = doc.findSymbols(True, False);
        *list<auto> bare = doc.findSymbols(True, True);
        # both lists come from the same walk of the tree, so they have the same order
        bool haveBare = bare.size() == qualified.size();
        for (int i = 0; i < qualified.size(); ++i) {
            hash<auto> sym = qualified[i];
            hash<auto> range = sym.location.range;
            push names, sym.name;
            push bareNames, haveBare ? bare[i].name : FileIndex::getLastComponent(sym.name);
            ranges += (sym.kind, range.start.line, range.start.character, range.end.line, range.end.character);
            if (sym.kind == ASTSymbolKind.Class) {
                push classes, <IndexedClass>{
                    "name": bareNames[i],
                    "qualifiedName": sym.name,
                    "parents": doc.getSuperclassNames(range.start) ?? (),
                    "symbol": i,
                };
            }
        }
    }

    #! Parses a file and returns its index
    /** @param uri the file URI

        @return the index of the file; the syntax tree is released before the method returns
    */
    static FileIndex fromFile(string uri) {
        Document doc(uri);
        return new FileIndex(doc, hstat(Document::getFilePath(uri)).size ?? 0);
    }

    #! Returns the part of a name after the last @c "::"
    static string getLastComponent(string name) {
        int i = name.rfind("::");
        return i == -1 ? name : name.substr(i + 2);
    }

    #! Returns the URI of the file
    string getUri() {
        return uri;
    }

    #! Returns the size of the source in bytes
    int getSourceSize() {
        return sourceSize;
    }

    #! Returns the number of symbols in the index
    int getSymbolCount() {
        return names.size();
    }

    #! Returns the LSP diagnostics of the syntax errors of the file
    /** @return the diagnostics of the syntax errors found when the file was indexed, or an empty list
    */
    list<hash<auto>> getDiagnostics() {
        return diagnostics ?? ();
    }

    #! Returns the classes declared in the file
    list<hash<IndexedClass>> getClasses() {
        return classes;
    }

    #! Returns the symbol at the given position in the index
    /** @param i the position of the symbol

        @return a symbol hash in the format returned by Document::findMatchingSymbols()
    */
    hash<auto> getSymbol(int i) {
        int r = i * RangeFields;
        return {
            "name": names[i],
            "kind": ranges[r],
            "location": {
                "uri": uri,
                "range": {
                    "start": {
                        "line": ranges[r + 1],
                        "character": ranges[r + 2],
                    },
                    "end": {
                        "line": ranges[r + 3],
                        "character": ranges[r + 4],
                    },
                },
            },
        };
    }

    #! Finds matching symbols
    /** Matches the same symbols as Document::findMatchingSymbols() with @p fixSymbols = @ref True and
        @p bareNames = @ref False; doc comments are not stored in the index and therefore not returned

        @param query search query
        @param exactMatch whether to only find exact matches; otherwise a case-insensitive substring match is made

        @return list of symbols
    */
    list<hash<auto>> findMatchingSymbols(string query, bool exactMatch = False) {
        list<hash<auto>> ret = ();
        if (exactMatch) {
            # most files do not declare the name, so check that first
            if (!names.contains(query)) {
                return ret;
            }
            for (int i = 0; i < names.size(); ++i) {
                if (names[i] == query) {
                    push ret, getSymbol(i);
                }
            }
        } else {
            string lquery = query.lwr();
            for (int i = 0; i < names.size(); ++i) {
                if (names[i].lwr().find(lquery) != -1) {
                    push ret, getSymbol(i);
                }
            }
        }
        return ret;
    }

    #! Adds the classes declared in the file to a class graph
    addToClassGraph(reference<hash<ClassGraph>> graph) {
        foreach hash<IndexedClass> cls in (classes) {
            if (!graph.parents{cls.name}) {
                graph.parents{cls.name} = {};
            }
            foreach string parent in (cls.parents) {
                string parentName = FileIndex::getLastComponent(parent);
                graph.parents{cls.name}{parentName} = True;
                graph.children{parentName}{cls.name} = True;
            }
        }
    }

    #! Returns @ref True if the file declares a symbol with the given declared name
    /** @param name a declared name, without namespace or class prefixes

        @return @ref True if a symbol in the file is declared with this name
    */
    bool declares(string name) {
        return bareNames.contains(name);
    }

    #! Returns the declared names of the classes in the file that have a member with the given name
    /** @param member the declared name of the member

        @return the declared class names, without duplicates
    */
    list<string> getClassesWithMember(string member) {
        if (!classes || !bareNames.contains(member)) {
            return ();
        }
        hash<string, string> classByQualifiedName = (map {$1.qualifiedName: $1.name}, classes) ?? {};
        string suffix = "::" + member;
        list<string> ret = ();
        for (int i = 0; i < names.size(); ++i) {
            if (bareNames[i] != member || names[i].size() <= suffix.size()
                    || names[i].substr(-suffix.size()) != suffix) {
                continue;
            }
            *string cls = classByQualifiedName{names[i].substr(0, names[i].size() - suffix.size())};
            if (cls && !ret.contains(cls)) {
                push ret, cls;
            }
        }
        return ret;
    }

    #! Returns @ref True if the file can contain the identifier when it was indexed
    /** Uses a filter of the identifiers in the file without reading it: there are no false negatives, and about 1%
        of identifiers that do not occur in the file also give @ref True.

        @param name the identifier, as compared by reference searches

        @return @ref False if the file did not contain the identifier when it was indexed; @ref True if it can
        contain it or if the file could not be parsed
    */
    bool mayContainIdentifier(string name) {
        return identifierFilter ? astparser::AstTree::identifierFilterContains(identifierFilter, name) : True;
    }

    #! Returns @ref True if the source file currently contains the given text
    /** Used to find the files that can contain references to an identifier without parsing them.

        @param text the text to search for

        @return @ref True if the file contains the text; @ref False if not or if the file cannot be read
    */
    bool sourceContains(string text) {
        try {
            return ReadOnlyFile::readTextFile(Document::getFilePath(uri)).find(text) != -1;
        } catch (hash<ExceptionInfo> ex) {
            # a file that cannot be read has no references
        }
        return False;
    }
}

#! Keeps the syntax trees of recently used indexed files
/** Entries are evicted in least recently used order when the total size of their sources exceeds the maximum
    size.  A document that is still referenced by a request in progress stays valid after it is evicted.

    @par Example
    @code{.py}
# keep the syntax trees of up to 16 MiB of recently used source files
DocumentCache cache(16 * 1024 * 1024);
string uri = Document::getFileUri("/src/project/lib/InvoiceProcessor.qc");
Document doc = cache.get(uri, hstat(Document::getFilePath(uri)).size);
*list<auto> members = doc.findTypeMembers("InvoiceProcessor");
    @endcode

    @note This class is not thread-safe; each @ref QLS::QLSServer "QLSServer" uses its own cache from the thread
    that processes its requests
*/
public class DocumentCache {
    private {
        #! Cached documents in least recently used order
        hash<string, Document> docs = {};

        #! Source size of each cached document
        hash<string, int> sizes = {};

        #! Total source size of the cached documents
        int totalSize = 0;

        #! Maximum total source size of the cached documents; 0 disables caching
        int maxSize;
    }

    #! Creates the cache
    /** @param maxSize maximum total source size of the cached documents in bytes; 0 disables caching
    */
    constructor(int maxSize = DefaultTreeCacheSize) {
        self.maxSize = maxSize;
    }

    #! Sets the maximum total source size of the cached documents and evicts documents as necessary
    setMaxSize(int maxSize) {
        self.maxSize = maxSize;
        trim();
    }

    #! Returns the maximum total source size of the cached documents
    int getMaxSize() {
        return maxSize;
    }

    #! Returns the document for the given file, parsing it if it is not cached
    /** @param uri the file URI
        @param size the size of the source in bytes; used to limit the total size of the cache

        @return the parsed document; a file that cannot be read gives a document without a syntax tree
    */
    Document get(string uri, int size) {
        *Document doc = remove docs{uri};
        if (doc) {
            # reinsert as the most recently used document
            docs{uri} = doc;
            return doc;
        }
        doc = new Document(uri);
        docs{uri} = doc;
        totalSize += (sizes{uri} = size);
        trim();
        return doc;
    }

    #! Returns @ref True if the given file is cached
    bool contains(string uri) {
        return exists docs{uri};
    }

    #! Removes the given file from the cache
    invalidate(string uri) {
        if (remove docs{uri}) {
            totalSize -= remove sizes{uri};
        }
    }

    #! Removes all documents from the cache
    clear() {
        docs = {};
        sizes = {};
        totalSize = 0;
    }

    #! Returns the number of cached documents
    int size() {
        return docs.size();
    }

    #! Returns the total source size of the cached documents
    int getTotalSize() {
        return totalSize;
    }

    #! Evicts least recently used documents until the total source size is not greater than the maximum
    private trim() {
        while (totalSize > maxSize && docs) {
            string uri = docs.firstKey();
            remove docs{uri};
            totalSize -= remove sizes{uri};
        }
    }
}

} # namespace QLS
