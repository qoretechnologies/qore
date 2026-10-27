# -*- mode: qore; indent-tabs-mode: nil -*-

/*  QLSServer.qc Copyright 2017 - 2026 Qore Technologies, s.r.o.

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

#! LSP FileChangeType definition
public const FileChangeType = {
    "Created": 1,
    "Changed": 2,
    "Deleted": 3,
};

#! LSP TextDocumentSyncKind definition
public const TextDocumentSyncKind = {
    /**
     * Documents should not be synced at all.
     */
    "None": 0,

    /**
     * Documents are synced by always sending the full content
     * of the document.
     */
    "Full": 1,

    /**
     * Documents are synced by sending the full content on open.
     * After that only incremental updates to the document are
     * sent.
     */
    "Incremental": 2,
};

#! Semantic token type legend — indices must match C++ SemanticTokenType enum
public const SemanticTokenTypes = (
    "namespace",    # 0
    "type",         # 1
    "class",        # 2
    "enum",         # 3
    "interface",    # 4
    "struct",       # 5
    "typeParameter",# 6
    "parameter",    # 7
    "variable",     # 8
    "property",     # 9
    "enumMember",   # 10
    "function",     # 11
    "method",       # 12
    "keyword",      # 13
    "comment",      # 14
    "string",       # 15
    "number",       # 16
    "regexp",       # 17
    "operator",     # 18
    "decorator",    # 19
);

#! Semantic token modifier legend — bit indices must match C++ SemanticTokenModifier enum
public const SemanticTokenModifiers = (
    "declaration",    # bit 0
    "definition",     # bit 1
    "readonly",       # bit 2
    "static",         # bit 3
    "deprecated",     # bit 4
    "abstract",       # bit 5
    "async",          # bit 6
    "modification",   # bit 7
    "documentation",  # bit 8
    "defaultLibrary", # bit 9
);

#! Explicit astparser-to-LSP SymbolKind conversion table.
/** Most astparser kinds intentionally match their LSP counterparts, but
    @ref astparser::ASYK_TypeAlias "ASYK_TypeAlias" does not: LSP value 19 is
    Object, while a Qore typedef is represented as TypeParameter (26).
 */
public const LspDocumentSymbolKindMap = {
    string(ASTSymbolKind.File): 1,
    string(ASTSymbolKind.Module): 2,
    string(ASTSymbolKind.Namespace): 3,
    string(ASTSymbolKind.Package): 4,
    string(ASTSymbolKind.Class): 5,
    string(ASTSymbolKind.Method): 6,
    string(ASTSymbolKind.Property): 7,
    string(ASTSymbolKind.Field): 8,
    string(ASTSymbolKind.Constructor): 9,
    string(ASTSymbolKind.Enum): 10,
    string(ASTSymbolKind.Interface): 11,
    string(ASTSymbolKind.Function): 12,
    string(ASTSymbolKind.Variable): 13,
    string(ASTSymbolKind.Constant): 14,
    string(ASTSymbolKind.String): 15,
    string(ASTSymbolKind.Number): 16,
    string(ASTSymbolKind.Boolean): 17,
    string(ASTSymbolKind.Array): 18,
    string(ASTSymbolKind.TypeAlias): 26,
};

#! QLS LSP ServerCapabilities
public const ServerCapabilities = {
    /**
     * Defines how text documents are synced. Is either a detailed structure defining each notification or
     * for backwards compatibility the TextDocumentSyncKind number.
     *
     * Changes are sent incrementally, so large documents are not sent again for every edit.
     */
    "textDocumentSync": {
        "openClose": True,
        "change": TextDocumentSyncKind.Incremental,
        "save": {
            "includeText": False,
        },
    },

    /**
     * The server provides hover support.
     */
    "hoverProvider": True,

    /**
     * The server provides goto definition support.
     */
    "definitionProvider": True,

    /**
     * The server provides find references support.
     */
    "referencesProvider": True,

    /**
     * The server provides document symbol support.
     */
    "documentSymbolProvider": True,

    /**
     * The server provides workspace symbol support.
     */
    "workspaceSymbolProvider": True,

    /**
     * The server provides completion support.
     */
    "completionProvider": {
        "resolveProvider": True,
        "triggerCharacters": (".", ":"),
    },

    /**
     * The server provides on-type formatting (auto-indent on Enter, auto-dedent on }).
     */
    "documentOnTypeFormattingProvider": {
        "firstTriggerCharacter": "}",
        "moreTriggerCharacter": ("\n",),
    },

    /**
     * The server provides document formatting support.
     */
    "documentFormattingProvider": True,

    /**
     * The server provides document highlight support.
     */
    "documentHighlightProvider": True,

    /**
     * The server provides signature help support.
     */
    "signatureHelpProvider": {
        "triggerCharacters": ("(", ","),
    },

    /**
     * The server provides folding range support.
     */
    "foldingRangeProvider": True,

    /**
     * The server provides selection range support.
     */
    "selectionRangeProvider": True,

    /**
     * The server provides go to type definition support.
     */
    "typeDefinitionProvider": True,

    /**
     * The server provides code lens support.
     */
    "codeLensProvider": {
        "resolveProvider": True,
    },

    /**
     * The server provides type hierarchy support.
     */
    "typeHierarchyProvider": True,

    /**
     * The server provides rename support.
     */
    "renameProvider": {
        "prepareProvider": True,
    },

    /**
     * The server provides go to implementation support.
     */
    "implementationProvider": True,

    /**
     * The server provides inlay hint support.
     */
    "inlayHintProvider": True,

    /**
     * The server provides call hierarchy support.
     */
    "callHierarchyProvider": True,

    /**
     * The server provides semantic tokens support.
     */
    "semanticTokensProvider": {
        "legend": {
            "tokenTypes": SemanticTokenTypes,
            "tokenModifiers": SemanticTokenModifiers,
        },
        "full": {
            "delta": True,
        },
        "range": True,
    },

    /**
     * The server provides code action support.
     */
    "codeActionProvider": {
        "codeActionKinds": ("quickfix", "source.sortImports", "source"),
    },

    /**
     * The server provides formatting of a range of a document.
     */
    "documentRangeFormattingProvider": True,

    /**
     * The server provides go to declaration support; declarations are definitions in Qore.
     */
    "declarationProvider": True,

    /**
     * The server provides links for the files of module and include directives.
     */
    "documentLinkProvider": {
        "resolveProvider": False,
    },

    /**
     * Workspace-specific server capabilities: all workspace folders are indexed, and folders added to or removed
     * from the workspace are indexed or removed.
     */
    "workspace": {
        "workspaceFolders": {
            "supported": True,
            "changeNotifications": True,
        },
    },
};

#! QLS - Language Server Protocol server implementation for Qore.
public class QLSServer {
    private {
        #! LSP header and content parts delimiter
        const LSP_PART_DELIMITER = "\r\n";

        #! MIN log level
        const LOGLEVEL_MIN = 0;

        #! MAX log level
        const LOGLEVEL_MAX = 2;

        #! Whether QLS has been initialized ("initialize" method called).
        bool initialized = False;

        #! Whether client is fully initialized ("initialized" method called).
        bool clientInitialized = False;

        #! Whether QLS has been shut down ("shutdown" method called).
        bool shutdown = False;

        # Whether the main loop should still run (or QLS should quit).
        bool running = True;

        # Exit code to use when quitting
        int exitCode = 0;

        #! PID of parent process
        int parentProcessId;

        #! JSON-RPC version string (ex. "2.0")
        string jsonRpcVer = "2.0";

        #! Workspace root URI (ex. "file:///home/user/projects/lorem").
        /** See Document::getFileUriPrefix()
            See Document::getFileUri()
         */
        string rootUri;

        #! Workspace root path (ex. "/home/user/projects/lorem")
        string rootPath;

        #! Paths of the workspace folders; all are indexed (rootPath is the first one)
        list<string> workspaceRoots = ();

        #! Client capabilities
        hash<auto> clientCapabilities;

        #! Client configuration
        hash<auto> clientConfig;

        #! Whether to log QLS operations.
        bool logging = PlatformOS != "Windows";

        #! Whether to append to the log file.
        bool appendToLog = True;

        #! Logging verbosity. Only messages with this level or lower will be logged.
        int logVerbosity = 0;

        #! Log file
        string logFile;

        #! Additional messages that need to be sent to the client.
        list<auto> messagesToSend = ();

        #! Map of JSON-RPC methods
        hash<auto> methodMap;

        #! Open text documents. Hash keys are document URIs.
        hash<auto> documents;

        #! Symbol indexes of the Qore files in the current workspace that are not open; hash keys are URIs
        hash<string, FileIndex> workspaceIndex = {};

        #! Symbol indexes of the standard Qore modules; hash keys are URIs
        hash<string, FileIndex> stdModuleIndex = {};

        #! Standard Qore module URIs; files are indexed lazily from this list
        list<string> stdModuleUris = ();

        #! Whether standard module sources have been indexed into stdModuleIndex
        bool stdModuleIndexLoaded = False;

        #! Options for finding workspace and standard module files; set from the initialization options
        hash<Files::FileScanOptions> scanOptions = <Files::FileScanOptions>{};

        #! Syntax trees of indexed files parsed for requests
        DocumentCache treeCache();

        #! Class graph of the indexed files; built when needed and discarded when an index changes
        *hash<ClassGraph> indexedClassGraph;

        #! URIs of ignored documents
        hash<auto> ignoredUris;

        #! Builtin metadata from qpp .meta.json + reflection (may be NOTHING if loading fails)
        *hash<QoreApiMetadata::NamespaceInfo> builtinMetadata;

        #! Fast lookup: unqualified name → list of metadata entries
        hash<string, list<hash<auto>>> metaByName;

        #! Fast lookup: fully qualified name → metadata entry
        hash<string, hash<auto>> metaByQualifiedName;

        #! Fast lookup: class name → ClassInfo
        hash<string, hash<auto>> metaClassByName;

        #! Single-char prefix index: lowercased first char → list of names
        hash<string, list<string>> metaPrefixIndex;

        #! Process-local cache for immutable standard module URI lists and lazily built symbol indexes
        static hash<auto> stdModuleIndexCache = {};

        #! Lock protecting standard module index cache initialization
        static Mutex stdModuleIndexCacheMutex();

        #! Process-local cache for immutable builtin metadata and lookup indexes
        static hash<auto> builtinMetadataCache = {};

        #! Lock protecting builtin metadata cache initialization
        static Mutex builtinMetadataCacheMutex();

        #! Whether files are indexed between requests instead of before responding to requests; set by main()
        bool backgroundIndexing = False;

        #! Workspace files waiting to be indexed in the background
        list<string> pendingWorkspaceFiles = ();

        #! Standard module files waiting to be indexed in the background
        list<string> pendingStdFiles = ();

        #! Standard module indexes built so far; published to the shared cache when all files are indexed
        hash<string, FileIndex> stdIndexPartial = {};

        #! The number of files queued for background indexing since background indexing last finished
        int indexQueued = 0;

        #! The number of queued files that have been indexed
        int indexDone = 0;

        #! The progress token of the current background indexing, if the client supports progress reports
        *string progressToken;

        #! The ID of the pending window/workDoneProgress/create request
        *string progressRequestId;

        #! Whether progress is being reported with progressToken
        bool progressActive = False;

        #! The last percentage reported
        int progressPercent = -1;

        #! The number of progress tokens created
        int progressCount = 0;

        #! The connection used by main()
        *LspConnection connection;

        #! Requests that have been read and not yet answered; keys are JSON-encoded request IDs
        hash<string, bool> pendingRequests = {};

        #! Pending requests that the client has cancelled; keys are JSON-encoded request IDs
        hash<string, bool> cancelledRequests = {};

        #! Protects pendingRequests and cancelledRequests, which the message reader thread updates
        Mutex requestLock();

        #! The JSON-encoded ID of the request being processed
        *string currentRequestKey;

        #! The client's name from the initialize request, if sent
        *string clientName;

        #! The diagnostics mode: \c "qore" checks standalone scripts with the Qore parser, \c "syntax" only reports
        #! syntax errors of the syntax tree
        string diagnosticsMode = "qore";

        #! The Qore executable used to check scripts; the interpreter running the server if not set
        *string qoreExecutable;

        #! The diagnostics of the last Qore check of each open document: \c version and \c diagnostics keys
        hash<string, hash<auto>> qoreDiagnostics = {};

        #! The last semantic tokens returned for each open document: \c resultId and \c data keys
        hash<string, hash<auto>> semanticTokensResults = {};

        #! The number of semantic token results returned, for result IDs
        int semanticTokensResultCount = 0;
    }

    private:internal {
        #! The log file; a File object can be written from any thread, unlike a stream
        *File logFh;
    }

    constructor() {
        logFile = getDefaultLogFilePath();
        prepareLogFile(logFile);
        initMethodMap();
    }

    private:internal string getDefaultLogFilePath() {
        if (PlatformOS == "Windows") {
            *string appdata = getenv("APPDATA");
            return (appdata ?? ".") + DirSep + "QLS" + DirSep + "qls.log";
        } else {
            *string home = getenv("HOME");
            return (home ?? "/tmp") + DirSep + ".qls.log";
        }
    }

    private:internal prepareLogFile(string logFilePath, bool force = False) {
        if (logging || force) {
            # prepare the log directory
            Dir d();
            if (!d.chdir(dirname(logFilePath))) {
                try {
                    d.create(mode: 0755);
                } catch (hash<ExceptionInfo> ex) {
                    return;
                }
            }

            # check if the log file can be opened and truncate it if appending is turned off
            try {
                File f();
                f.open2(logFile, O_CREAT | O_WRONLY | (appendToLog ? O_APPEND : O_TRUNC));
                logFh = f;
            } catch (hash<ExceptionInfo> ex) {
            }
        }
    }

    private:internal log(int verbosity, string fmt, ...) {
        if (logFh && verbosity <= logVerbosity) {
            string str = sprintf("%s: ", format_date("YYYY-MM-DD HH:mm:SS", now()));
            logFh.write(vsprintf(str + fmt + "\n", argv));
        }
    }

    private:internal error(string fmt) {
        log(0, fmt, argv);
        stderr.vprintf("ERROR: " + fmt + "\n", argv);
        exit(1);
    }

    private:internal *string validateRequest(hash<auto> request) {
        if (!request.hasKey("jsonrpc")) {
            return ErrorResponse::invalidRequest(request, "Missing 'jsonrpc' attribute");
        }
        if (!request.hasKey("method")) {
            return ErrorResponse::invalidRequest(request, "Missing 'method' attribute");
        }
        return NOTHING;
    }

    private:internal initMethodMap() {
        methodMap = {
            # General methods
            "initialize": \meth_initialize(),
            "initialized": \meth_initialized(),
            "shutdown": \meth_shutdown(),
            "exit": \meth_exit(),
            "$/cancelRequest": \meth_cancelRequest(),

            # Workspace methods
            "workspace/didChangeConfiguration": \meth_ws_didChangeConfiguration(),
            "workspace/didChangeWatchedFiles": \meth_ws_didChangeWatchedFiles(),
            "workspace/symbol": \meth_ws_symbol(),
            "workspace/didChangeWorkspaceFolders": \meth_ws_didChangeWorkspaceFolders(),

            # Document methods
            "textDocument/didOpen": \meth_td_didOpen(),
            "textDocument/didChange": \meth_td_didChange(),
            "textDocument/willSave": \meth_td_willSave(),
            "textDocument/willSaveWaitUntil": \meth_td_willSaveWaitUntil(),
            "textDocument/didSave": \meth_td_didSave(),
            "textDocument/didClose": \meth_td_didClose(),
            "textDocument/hover": \meth_td_hover(),
            "textDocument/references": \meth_td_references(),
            "textDocument/documentSymbol": \meth_td_documentSymbol(),
            "textDocument/definition": \meth_td_definition(),
            "textDocument/completion": \meth_td_completion(),
            "completionItem/resolve": \meth_completionItem_resolve(),
            "textDocument/onTypeFormatting": \meth_td_onTypeFormatting(),
            "textDocument/formatting": \meth_td_formatting(),
            "textDocument/documentHighlight": \meth_td_documentHighlight(),
            "textDocument/signatureHelp": \meth_td_signatureHelp(),
            "textDocument/foldingRange": \meth_td_foldingRange(),
            "textDocument/selectionRange": \meth_td_selectionRange(),
            "textDocument/typeDefinition": \meth_td_typeDefinition(),
            "textDocument/codeLens": \meth_td_codeLens(),
            "codeLens/resolve": \meth_codeLens_resolve(),
            "textDocument/prepareTypeHierarchy": \meth_td_prepareTypeHierarchy(),
            "typeHierarchy/supertypes": \meth_typeHierarchy_supertypes(),
            "typeHierarchy/subtypes": \meth_typeHierarchy_subtypes(),
            "textDocument/prepareRename": \meth_td_prepareRename(),
            "textDocument/rename": \meth_td_rename(),
            "textDocument/implementation": \meth_td_implementation(),
            "textDocument/inlayHint": \meth_td_inlayHint(),
            "textDocument/prepareCallHierarchy": \meth_td_prepareCallHierarchy(),
            "callHierarchy/incomingCalls": \meth_callHierarchy_incomingCalls(),
            "callHierarchy/outgoingCalls": \meth_callHierarchy_outgoingCalls(),
            "textDocument/semanticTokens/full": \meth_td_semanticTokensFull(),
            "textDocument/semanticTokens/range": \meth_td_semanticTokensRange(),
            "textDocument/codeAction": \meth_td_codeAction(),
            # Qore has no declarations that are separate from definitions
            "textDocument/declaration": \meth_td_definition(),
            "textDocument/documentLink": \meth_td_documentLink(),
            "textDocument/rangeFormatting": \meth_td_rangeFormatting(),
            "textDocument/semanticTokens/full/delta": \meth_td_semanticTokensDelta(),
        };
    }


    #=================
    # Main logic
    #=================

    #! Runs the main JSON-RPC server loop
    /** Messages are read by a background thread, so that \c $/cancelRequest notifications can cancel requests
        that are being processed.  Workspace and standard module files are indexed one at a time while no message
        is waiting, so requests are answered while the workspace is being indexed; progress is reported to clients
        that support it.

        @param conn the connection to use; if not given, the process's standard input and output are used

        @return the exit code (0 for normal shutdown, 1 for error)
    */
    public int main(*LspConnection conn) {
        connection = conn ?? new LspConnection();
        backgroundIndexing = True;

        Queue messages();
        Counter readerRunning(1);
        background readMessages(connection, messages, readerRunning);

        while (running) {
            if (!messages.size() && indexPendingFile()) {
                sendQueuedMessages();
                continue;
            }

            # read JSON-RPC request
            hash<auto> received = messages.get();
            list<auto> responses;
            if (received.error) {
                # Normal shutdown when stdin closes (ex: client exit)
                if (received.error == "EOF") {
                    running = False;
                    exitCode = 0;
                    break;
                }
                responses = (ErrorResponse::internalError(jsonRpcVer, received.error),);
            } else if (!received.msg) {
                responses = (ErrorResponse::internalError(jsonRpcVer, "no message received"),);
            } else {
                responses = handleRequest(received.msg);
            }

            # send back response if any
            map connection.send($1), responses, $1.typeCode() == NT_STRING;
            sendQueuedMessages();
        }

        # the reader stops after reading the exit notification or the end of the input
        readerRunning.waitForZero();
        return exitCode;
    }

    #! Records a request that has been read and not yet answered, so that the client can cancel it
    /** The message reader of main() calls this method; applications that read messages themselves and call
        handleRequest() from another thread can call it to support cancellation.

        @param id the request ID
    */
    registerRequest(auto id) {
        AutoLock al(requestLock);
        pendingRequests{make_json(id)} = True;
    }

    #! Cancels a request that has been read and not yet answered
    /** A cancelled request that has not been started is answered with a \c RequestCancelled error; a request
        that is being processed stops at the next file that it would search and is answered with the same error.

        @param id the request ID

        @return @ref True if the request was pending; @ref False if it is unknown or has already been answered
    */
    bool cancelRequest(auto id) {
        AutoLock al(requestLock);
        string key = make_json(id);
        if (!pendingRequests{key}) {
            return False;
        }
        cancelledRequests{key} = True;
        return True;
    }

    #! Returns @ref True if the request with the given key has been cancelled
    private:internal bool isCancelled(string key) {
        AutoLock al(requestLock);
        return cancelledRequests{key} ?? False;
    }

    #! Forgets a request that has been answered
    private:internal finishRequest(string key) {
        AutoLock al(requestLock);
        remove pendingRequests{key};
        remove cancelledRequests{key};
    }

    #! Ends the current request if the client has cancelled it
    /** Called between files in operations that search many files.

        @throw QLS-REQUEST-CANCELLED the client has cancelled the request
    */
    private:internal checkCancelled() {
        if (currentRequestKey && isCancelled(currentRequestKey)) {
            throw "QLS-REQUEST-CANCELLED", sprintf("request %s was cancelled", currentRequestKey);
        }
    }

    #! Reads messages for main() until the input ends or the exit notification is read
    private:internal readMessages(LspConnection conn, Queue messages, Counter running) {
        on_exit {
            running.dec();
        }
        try {
            while (True) {
                hash<auto> received = conn.receive();
                bool exitMsg;
                if (received.msg) {
                    auto msg;
                    try {
                        msg = parse_json(received.msg);
                    } catch (hash<ExceptionInfo> ex) {
                        # invalid JSON is answered by the main loop
                    }
                    if (msg.typeCode() == NT_HASH) {
                        if (msg.method == "$/cancelRequest") {
                            if (exists msg.params.id) {
                                cancelRequest(msg.params.id);
                            }
                        } else if (msg.method && exists msg.id) {
                            registerRequest(msg.id);
                        }
                        exitMsg = msg.method == "exit";
                    }
                }
                messages.push(received);
                if (received.error == "EOF" || exitMsg) {
                    break;
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(0, "ERROR: reading messages failed: %s", get_exception_string(ex));
            messages.push({"error": "EOF"});
        }
    }

    #! Sends the messages queued by request handlers and background indexing
    private:internal sendQueuedMessages() {
        map connection.send($1), takeQueuedMessages();
    }

    #! Sets whether files are indexed between requests
    /** main() enables background indexing.  Applications that call handleRequest() themselves can enable it
        before the \c initialize request and call indexPendingFile() while no request is waiting, and send the
        messages returned by takeQueuedMessages() to the client.

        Background indexing starts after the client's \c initialized notification.  Without background
        indexing, files are indexed before \c initialize and workspace folder changes are answered.

        @param enable @ref True to index files between requests
    */
    setBackgroundIndexing(bool enable) {
        backgroundIndexing = enable;
    }

    #! Indexes the next file waiting for background indexing
    /** Workspace files are indexed first, with progress reports for clients that support them; standard module
        files follow.

        @return @ref True if a file was indexed; @ref False if no file is waiting or the client has not sent the
        \c initialized notification yet
    */
    bool indexPendingFile() {
        if (!clientInitialized || !indexPending()) {
            return False;
        }
        indexNextFile();
        return True;
    }

    #! Returns the notifications and requests for the client queued by the server, and removes them from the queue
    /** Messages are only returned after the client's \c initialized notification.

        @return JSON-RPC messages to send to the client in the order returned
    */
    list<string> takeQueuedMessages() {
        if (!initialized || !clientInitialized || !messagesToSend) {
            return ();
        }
        list<string> ret = messagesToSend;
        messagesToSend = ();
        return ret;
    }

    #! Handles a response from the client to a request sent by the server
    private:internal handleClientResponse(hash<auto> response) {
        if (progressRequestId && response.id == progressRequestId) {
            delete progressRequestId;
            if (response.error) {
                log(0, "client refused the indexing progress token: %y", response.error);
                return;
            }
            # the token can only be used after the client has created it
            if (pendingWorkspaceFiles) {
                progressActive = True;
                progressPercent = -1;
                queueProgress({"kind": "begin", "title": "Indexing Qore files", "cancellable": False,
                    "percentage": getIndexPercentage()});
            }
            return;
        }
        log(1, "ignoring response from the client: %y", response);
    }

    #! Returns @ref True if files are waiting to be indexed in the background
    private:internal bool indexPending() {
        return pendingWorkspaceFiles || pendingStdFiles;
    }

    #! Returns the percentage of the workspace files queued for background indexing that have been indexed
    private:internal int getIndexPercentage() {
        return indexQueued ? (indexDone * 100 / indexQueued) : 100;
    }

    #! Queues a $/progress notification for the indexing progress token
    private:internal queueProgress(hash<auto> value) {
        push messagesToSend, make_json({
            "jsonrpc": jsonRpcVer,
            "method": "$/progress",
            "params": {"token": progressToken, "value": value},
        });
    }

    #! Asks the client to create a progress token for background indexing, if it supports progress reports
    private:internal startIndexingProgress() {
        if (!backgroundIndexing || !pendingWorkspaceFiles || progressActive || progressRequestId
                || !clientCapabilities.window.workDoneProgress) {
            return;
        }
        ++progressCount;
        progressToken = sprintf("qls-indexing-%d", progressCount);
        progressRequestId = sprintf("qls-progress-%d", progressCount);
        push messagesToSend, make_json({
            "jsonrpc": jsonRpcVer,
            "id": progressRequestId,
            "method": "window/workDoneProgress/create",
            "params": {"token": progressToken},
        });
    }

    #! Reports the progress of indexing the workspace files in the background
    private:internal reportIndexingProgress() {
        if (!pendingWorkspaceFiles) {
            if (progressActive) {
                queueProgress({"kind": "end", "message": sprintf("%d files indexed", indexDone)});
                progressActive = False;
            }
            indexQueued = 0;
            indexDone = 0;
            return;
        }
        if (!progressActive) {
            return;
        }
        int percentage = getIndexPercentage();
        if (percentage != progressPercent) {
            progressPercent = percentage;
            queueProgress({"kind": "report", "message": sprintf("%d/%d files", indexDone, indexQueued),
                "percentage": percentage});
        }
    }

    #! Indexes the next file waiting for background indexing
    private:internal indexNextFile() {
        if (pendingWorkspaceFiles) {
            string uri = shift pendingWorkspaceFiles;
            # the file can have been opened, or its folder removed from the workspace, since it was queued
            if (!documents{uri} && !workspaceIndex{uri} && getWorkspaceRoot(Document::getFilePath(uri))) {
                *FileIndex index = indexFile(uri);
                if (index) {
                    workspaceIndex{uri} = index;
                    delete indexedClassGraph;
                }
            }
            ++indexDone;
            reportIndexingProgress();
            if (!pendingWorkspaceFiles) {
                log(0, "background indexing of workspace files done; %d workspace files indexed",
                    workspaceIndex.size());
                queueStdModules();
            }
        } else {
            string uri = shift pendingStdFiles;
            if (!stdModuleIndexLoaded) {
                *FileIndex index = indexFile(uri);
                if (index) {
                    stdIndexPartial{uri} = index;
                }
                if (!pendingStdFiles) {
                    publishStdModuleIndex();
                }
            }
        }
    }

    #! Queues the standard module files for background indexing if they are not indexed yet
    private:internal queueStdModules() {
        if (stdModuleIndexLoaded) {
            return;
        }
        {
            AutoLock al(stdModuleIndexCacheMutex);
            if (exists stdModuleIndexCache.index) {
                setStdModuleIndex(stdModuleIndexCache.index);
                return;
            }
        }
        pendingStdFiles = select stdModuleUris, !stdIndexPartial{$1};
    }

    #! Publishes the standard module index built in stdIndexPartial to the process-wide cache
    private:internal publishStdModuleIndex() {
        {
            AutoLock al(stdModuleIndexCacheMutex);
            if (!exists stdModuleIndexCache.index) {
                stdModuleIndexCache.index = stdIndexPartial;
            }
            setStdModuleIndex(stdModuleIndexCache.index);
        }
        stdIndexPartial = {};
        log(0, "%d standard module files indexed", stdModuleIndex.size());
    }

    #! Handles a single JSON-RPC request message and returns the response(s).
    /** @param msg the raw JSON-RPC message string
        @return a list of JSON-RPC response strings to send back (may be empty for notifications)
    */
    softlist<auto> handleRequest(string msg) {
        # parse the request
        auto request;
        try {
            request = parse_json(msg);
        } catch (hash<ExceptionInfo> ex) {
            log(0, "invalid JSON received: %s: %s", ex.err, ex.desc);
            return ErrorResponse::parseError(jsonRpcVer, sprintf("%s: %s", ex.err, ex.desc));
        }

        # validate request
        if (request.typeCode() != NT_HASH) {
            return ErrorResponse::invalidRequest({"received": msg, "jsonrpc": jsonRpcVer},
                "Invalid JSON-RPC request");
        }

        # responses to requests sent by the server
        if (!request.hasKey("method") && request.hasKey("id")
                && (request.hasKey("result") || request.hasKey("error"))) {
            handleClientResponse(request);
            return NOTHING;
        }

        *string validation = validateRequest(request);
        if (validation) {
            return validation;
        }

        # check that QLS has been initialized
        if (!initialized && request.method != "initialize") {
            return ErrorResponse::notInitialized(request);
        }

        # check that the method exists
        if (!methodMap.hasKey(request.method)) {
            if (request.id) {
                return ErrorResponse::methodNotFound(request);
            } else { # it's notification -> ignore
                return NOTHING;
            }
        }

        # requests that the client has cancelled before they were started are not processed
        *string key = exists request.id ? make_json(request.id) : NOTHING;
        if (key) {
            if (isCancelled(key)) {
                finishRequest(key);
                return ErrorResponse::requestCancelled(request);
            }
            currentRequestKey = key;
        }
        on_exit {
            if (key) {
                finishRequest(key);
                delete currentRequestKey;
            }
        }

        # call appropriate method
        try {
            softlist<auto> responses = methodMap{request.method}(request);
            return responses;
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "QLS-REQUEST-CANCELLED") {
                log(1, "request %s cancelled in method '%s'", key, request.method);
                return exists request.id ? ErrorResponse::requestCancelled(request) : NOTHING;
            }
            log(0, "unhandled exception in method '%s': %s: %s", request.method, ex.err, ex.desc);
            if (exists request.id) {
                return ErrorResponse::requestInternalError(request, sprintf("%s: %s", ex.err, ex.desc));
            }
            return NOTHING;
        }
    }


    #=================
    # Parsing
    #=================

    private:internal parseFilesInWorkspace() {
        if (!workspaceRoots) {
            log(0, "WARNING: no workspace folders - workspace files will not be parsed");
            return;
        }
        map indexWorkspaceRoot($1), workspaceRoots;
    }

    #! Adds a workspace folder; it is indexed by the caller
    /** @param uri the URI of the folder

        @return the path of the folder, or @ref nothing if it is not a local folder or is already a workspace folder
    */
    private:internal *string addWorkspaceRoot(string uri) {
        if (uri !~ /^file:\//) {
            log(0, "WARNING: ignoring workspace folder that is not a local directory: %y", uri);
            return NOTHING;
        }
        string path = regex_subst(Document::getFilePath(uri), "(.)/+$", "$1");
        if (workspaceRoots.contains(path)) {
            return NOTHING;
        }
        push workspaceRoots, path;
        if (!rootPath) {
            rootPath = path;
            rootUri = Document::getFileUri(path);
        }
        return path;
    }

    #! Indexes the Qore files in a workspace folder
    private:internal indexWorkspaceRoot(string root) {
        # find all Qore files in the workspace
        list<string> qoreFiles;
        list<string> skippedFiles;
        try {
            qoreFiles = Files::find_qore_files(root, scanOptions, \skippedFiles);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "DIR-READ-FAILURE") {
                log(0, "WARNING: root path scanning failed: %s", get_exception_string(ex));
                push messagesToSend, Notification::showMessage(
                    jsonRpcVer,
                    MessageType.Warning,
                    "An error happened during reading of workspace root, therefore Qore IntelliSense features might "
                        "not work fully. "
                        "Please check that the workspace does not contain any invalid symlinks or other invalid "
                        "files or directories.");
                return;
            }
            if (ex.err == "WORKSPACE-PATH-ERROR") {
                log(0, "WARNING: error processing root path: %s", get_exception_string(ex));
                push messagesToSend, Notification::showMessage(
                    jsonRpcVer,
                    MessageType.Warning,
                    "Could not open workspace root. Some Qore IntelliSense features might not work fully. "
                        "Please check that the workspace does not contain any invalid symlinks or other invalid "
                        "files or directories.");
                return;
            }
            rethrow;
        }
        if (skippedFiles) {
            log(0, "skipped %d workspace files larger than %d bytes: %Y", skippedFiles.size(),
                scanOptions.maxFileSize, skippedFiles);
        }

        # create a list of file URIs encoded like the URIs of documents opened by the client
        qoreFiles = map Document::getFileUri($1), qoreFiles;
        log(1, "qore files in workspace folder %y: %Y", root, qoreFiles);

        if (backgroundIndexing) {
            pendingWorkspaceFiles += qoreFiles;
            indexQueued += qoreFiles.size();
            log(0, "queued %d files in workspace folder %y for background indexing", qoreFiles.size(), root);
            return;
        }

        # measure start time
        date start = now_us();

        # index everything; syntax trees are released after indexing and parsed again when needed; open documents
        # and files in nested workspace folders that are already indexed are skipped
        int count = 0;
        foreach string uri in (qoreFiles) {
            if (documents{uri} || workspaceIndex{uri}) {
                continue;
            }
            *FileIndex index = indexFile(uri);
            if (index) {
                workspaceIndex{uri} = index;
                ++count;
            }
        }
        delete indexedClassGraph;

        # measure end time
        date end = now_us();
        log(0, "indexing of %d files in workspace folder %y took: %y; %d workspace files indexed", count, root,
            end - start, workspaceIndex.size());
    }

    #! Removes a workspace folder and the index of its files that are not in another workspace folder
    private:internal removeWorkspaceRoot(string uri) {
        if (uri !~ /^file:\//) {
            return;
        }
        string root = regex_subst(Document::getFilePath(uri), "(.)/+$", "$1");
        if (!workspaceRoots.contains(root)) {
            log(1, "removed workspace folder %y was not indexed", root);
            return;
        }
        workspaceRoots = select workspaceRoots, $1 != root;
        if (root == rootPath) {
            rootPath = workspaceRoots[0] ?? "";
            rootUri = rootPath ? Document::getFileUri(rootPath) : "";
        }
        int count = 0;
        foreach string fileUri in (keys workspaceIndex) {
            string path = Document::getFilePath(fileUri);
            if (path.equalPartial(root + DirSep) && !getWorkspaceRoot(path)) {
                remove workspaceIndex{fileUri};
                treeCache.invalidate(fileUri);
                ++count;
            }
        }
        delete indexedClassGraph;
        log(0, "removed workspace folder %y and the index of %d files", root, count);
    }

    #! Returns the workspace folder that contains the given path, or @ref nothing if it is not in the workspace
    /** If workspace folders are nested, the innermost folder is returned.
    */
    private:internal *string getWorkspaceRoot(string path) {
        *string ret;
        foreach string root in (workspaceRoots) {
            if (path.equalPartial(root + DirSep) && (!ret || root.size() > ret.size())) {
                ret = root;
            }
        }
        return ret;
    }

    #! Applies the initialization options sent by the client
    /** Supported options:
        - @c excludeDirs: list of shell-style glob patterns for directory names that are not searched for source
          files; replaces @ref QLS::Files::DefaultExcludeDirs
        - @c maxIndexFileSize: files larger than this many bytes are not indexed; 0 means no limit
        - @c treeCacheSize: syntax trees of indexed files are cached up to this many bytes of source; 0 disables
          the cache
        - @c diagnostics: \c "qore" (the default) to check standalone scripts with the Qore parser, or
          \c "syntax" to only report syntax errors
        - @c qoreExecutable: the Qore executable used to check scripts; the interpreter running the server by
          default

        Options with invalid values are logged and ignored.
    */
    private:internal applyInitializationOptions(auto opts) {
        if (!exists opts) {
            return;
        }
        if (opts.typeCode() != NT_HASH) {
            log(0, "WARNING: ignoring initializationOptions with invalid type: %s", opts.type());
            return;
        }
        if (exists opts.excludeDirs) {
            if (opts.excludeDirs.typeCode() == NT_LIST
                    && !(select opts.excludeDirs, $1.typeCode() != NT_STRING || !$1)) {
                scanOptions.excludeDirs = cast<list<string>>(opts.excludeDirs);
            } else {
                log(0, "WARNING: ignoring invalid initializationOptions.excludeDirs: %y", opts.excludeDirs);
            }
        }
        if (exists opts.maxIndexFileSize) {
            if (opts.maxIndexFileSize.typeCode() == NT_INT && opts.maxIndexFileSize >= 0) {
                scanOptions.maxFileSize = opts.maxIndexFileSize;
            } else {
                log(0, "WARNING: ignoring invalid initializationOptions.maxIndexFileSize: %y",
                    opts.maxIndexFileSize);
            }
        }
        if (exists opts.diagnostics) {
            if (opts.diagnostics == "qore" || opts.diagnostics == "syntax") {
                diagnosticsMode = opts.diagnostics;
            } else {
                log(0, "WARNING: ignoring invalid initializationOptions.diagnostics: %y", opts.diagnostics);
            }
        }
        if (exists opts.qoreExecutable) {
            if (opts.qoreExecutable.typeCode() == NT_STRING && opts.qoreExecutable) {
                qoreExecutable = opts.qoreExecutable;
            } else {
                log(0, "WARNING: ignoring invalid initializationOptions.qoreExecutable: %y", opts.qoreExecutable);
            }
        }
        if (exists opts.treeCacheSize) {
            if (opts.treeCacheSize.typeCode() == NT_INT && opts.treeCacheSize >= 0) {
                treeCache.setMaxSize(opts.treeCacheSize);
            } else {
                log(0, "WARNING: ignoring invalid initializationOptions.treeCacheSize: %y", opts.treeCacheSize);
            }
        }
        log(0, "file scan options: %y; tree cache size: %d", scanOptions, treeCache.getMaxSize());
    }

    #! Returns the symbol index of a file, or @ref nothing if the file cannot be parsed
    private:internal *FileIndex indexFile(string uri) {
        try {
            return FileIndex::fromFile(uri);
        } catch (hash<ExceptionInfo> ex) {
            log(0, "WARNING: cannot index %s: %s", uri, get_exception_string(ex));
        }
        return NOTHING;
    }

    private:internal parseStdModules() {
        {
            AutoLock al(stdModuleIndexCacheMutex);
            if (exists stdModuleIndexCache.files) {
                stdModuleUris = stdModuleIndexCache.files;
                if (exists stdModuleIndexCache.index) {
                    setStdModuleIndex(stdModuleIndexCache.index);
                }
                return;
            }
        }

        # find standard Qore modules
        list<string> moduleFiles;
        try {
            moduleFiles = Files::find_std_modules(scanOptions);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "DIR-READ-FAILURE") {
                log(0, "WARNING: std module scanning failed - %s: %s", ex.err, ex.desc);
                push messagesToSend, Notification::showMessage(
                    jsonRpcVer,
                    MessageType.Warning,
                    "An error happened during reading of standard Qore module files, therefore Qore IntelliSense "
                        "features might not work fully.");
                return;
            }
            rethrow;
        }

        # create a list of file URIs; indexing is deferred until a request needs standard module symbols
        stdModuleUris = map Document::getFileUri($1), moduleFiles;
        log(0, "standard module discovery found %d files", stdModuleUris.size());

        {
            AutoLock al(stdModuleIndexCacheMutex);
            if (!exists stdModuleIndexCache.files) {
                stdModuleIndexCache.files = stdModuleUris;
            } else {
                stdModuleUris = stdModuleIndexCache.files;
            }
            if (exists stdModuleIndexCache.index) {
                setStdModuleIndex(stdModuleIndexCache.index);
            }
        }
    }

    #! Sets the loaded standard module index
    private:internal setStdModuleIndex(hash<string, FileIndex> index) {
        stdModuleIndex = index;
        stdModuleIndexLoaded = True;
        delete indexedClassGraph;
    }

    private:internal ensureStdModuleIndex(string reason = "unspecified") {
        if (stdModuleIndexLoaded) {
            return;
        }

        {
            AutoLock al(stdModuleIndexCacheMutex);
            if (exists stdModuleIndexCache.index) {
                setStdModuleIndex(stdModuleIndexCache.index);
                if (!stdModuleUris.size() && exists stdModuleIndexCache.files) {
                    stdModuleUris = stdModuleIndexCache.files;
                }
                return;
            }
            if (!stdModuleUris.size() && exists stdModuleIndexCache.files) {
                stdModuleUris = stdModuleIndexCache.files;
            }
        }

        if (!stdModuleUris.size()) {
            parseStdModules();
            if (stdModuleIndexLoaded) {
                return;
            }
        }

        # complete the index; files indexed in the background or before a cancellation are kept
        date start = now_us();
        log(0, "indexing standard module files for %s", reason);
        foreach string uri in (stdModuleUris) {
            if (stdIndexPartial{uri}) {
                continue;
            }
            checkCancelled();
            *FileIndex fileIndex = indexFile(uri);
            if (fileIndex) {
                stdIndexPartial{uri} = fileIndex;
            }
        }
        # the files queued for background indexing are done
        pendingStdFiles = ();
        publishStdModuleIndex();

        date end = now_us();
        log(0, "indexing of %d standard module files for %s took: %y", stdModuleIndex.size(), reason, end - start);
    }

    #! Find the metadata directory containing qpp .meta.json files
    private static *string findMetadataDir() {
        # check env override first (for development/testing)
        *string env_dir = ENV.QORE_METADATA_DIR;
        if (env_dir && is_dir(env_dir)) {
            return env_dir;
        }
        # derive from module path: UserModuleDir is <datadir>/qore-modules/
        # strip trailing slash(es) before dirname() to get correct parent
        hash<auto> info = get_qore_library_info();
        string datadir = dirname(regex_subst(info.UserModuleDir, "/+$", ""));
        string metadir = datadir + "/qore/metadata";
        if (is_dir(metadir)) {
            return metadir;
        }
        return NOTHING;
    }

    #! Load builtin metadata from qpp .meta.json files + reflection
    private:internal loadBuiltinMetadata() {
        *string metadir = findMetadataDir();
        string cacheKey = metadir ?? "";

        {
            AutoLock al(builtinMetadataCacheMutex);
            if (builtinMetadataCache.builtinMetadata && builtinMetadataCache.dir == cacheKey) {
                builtinMetadata = builtinMetadataCache.builtinMetadata;
                metaByName = builtinMetadataCache.metaByName;
                metaByQualifiedName = builtinMetadataCache.metaByQualifiedName;
                metaClassByName = builtinMetadataCache.metaClassByName;
                metaPrefixIndex = builtinMetadataCache.metaPrefixIndex;
                log(0, "metadata index reused: %d names, %d qualified, %d classes",
                    metaByName.size(), metaByQualifiedName.size(), metaClassByName.size());
                return;
            }
        }

        # step 1: extract runtime structure via reflection
        hash<QoreApiMetadata::NamespaceInfo> runtimeMeta;
        try {
            runtimeMeta = QoreApiMetadata::ReflectionMetadataExtractor::extractFromRuntime();
        } catch (hash<ExceptionInfo> ex) {
            log(0, "WARNING: reflection metadata extraction failed - %s: %s", ex.err, ex.desc);
            return;
        }

        # step 2: load qpp doc index (if available)
        *hash<string, hash<auto>> docIndex;
        if (metadir) {
            try {
                docIndex = QoreApiMetadata::QppDocIndex::buildFromDirectory(metadir);
                log(0, "loaded %d qpp metadata entries from %s", docIndex.size(), metadir);
            } catch (hash<ExceptionInfo> ex) {
                log(0, "WARNING: qpp metadata loading failed - %s: %s", ex.err, ex.desc);
            }
        }

        # step 3: merge
        try {
            builtinMetadata = QoreApiMetadata::SymbolMerger::merge(runtimeMeta, NOTHING, docIndex);
        } catch (hash<ExceptionInfo> ex) {
            log(0, "WARNING: metadata merge failed - %s: %s", ex.err, ex.desc);
            return;
        }

        # step 4: build fast lookup indexes
        buildMetadataIndex(builtinMetadata);
        log(0, "metadata index built: %d names, %d qualified, %d classes",
            metaByName.size(), metaByQualifiedName.size(), metaClassByName.size());

        {
            AutoLock al(builtinMetadataCacheMutex);
            if (!builtinMetadataCache.builtinMetadata || builtinMetadataCache.dir != cacheKey) {
                builtinMetadataCache = {
                    "dir": cacheKey,
                    "builtinMetadata": builtinMetadata,
                    "metaByName": metaByName,
                    "metaByQualifiedName": metaByQualifiedName,
                    "metaClassByName": metaClassByName,
                    "metaPrefixIndex": metaPrefixIndex,
                };
            }
        }
    }

    #! Build fast lookup indexes from the metadata tree
    private buildMetadataIndex(hash<auto> ns) {
        # index classes
        foreach hash<auto> c in (ns.classes) {
            indexMetadataEntry(c.name, c.full_name, c + {"symbol_type": "class"});
            metaClassByName{c.name} = c;
            # also index by unqualified full_name parts
            if (c.full_name != c.name) {
                metaClassByName{c.full_name} = c;
            }
            # index methods
            foreach hash<auto> m in (c.instance_methods) {
                indexMetadataEntry(m.name, m.full_name, m + {"symbol_type": "method", "class_name": c.name});
            }
            foreach hash<auto> m in (c.static_methods) {
                indexMetadataEntry(m.name, m.full_name,
                    m + {"symbol_type": "static_method", "class_name": c.name});
            }
        }

        # index functions
        foreach hash<auto> f in (ns.functions) {
            indexMetadataEntry(f.name, f.full_name, f + {"symbol_type": "function"});
        }

        # index constants
        foreach hash<auto> c in (ns.constants) {
            indexMetadataEntry(c.name, c.full_name, c + {"symbol_type": "constant"});
        }

        # index hashdecls
        foreach hash<auto> h in (ns.hashdecls) {
            indexMetadataEntry(h.name, h.full_name, h + {"symbol_type": "hashdecl"});
        }

        # index enums
        foreach hash<auto> e in (ns.enums) {
            indexMetadataEntry(e.name, e.full_name, e + {"symbol_type": "enum"});
        }

        # recurse into child namespaces
        foreach hash<auto> child in (ns.namespaces) {
            buildMetadataIndex(child);
        }
    }

    #! Add an entry to the metadata indexes
    private indexMetadataEntry(string name, string fullName, hash<auto> entry) {
        if (!metaByName{name}) {
            metaByName{name} = ();
            # add to prefix index on first occurrence of this name
            string key = name[0].lwr();
            if (!metaPrefixIndex{key}) {
                metaPrefixIndex{key} = ();
            }
            metaPrefixIndex{key} += name;
        }
        metaByName{name} += entry;
        if (!metaByQualifiedName{fullName}) {
            metaByQualifiedName{fullName} = entry;
        }
    }


    #=================
    # General methods
    #=================

    #! "initialize" method handler
    private *string meth_initialize(hash<auto> request) {
        log(0, "initialize request received: %Y", request);
        jsonRpcVer = request.jsonrpc;

        # validate request
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        if (request.params.typeCode() != NT_HASH) {
            return ErrorResponse::invalidRequest(
                request,
                sprintf(
                    "required attribute 'params' has invalid type: %s",
                    request.params.type()
                )
            );
        }
        *string validation = ParamValidator::initializeParams(request, request.params);
        if (validation) {
            return validation;
        }

        # parse/save initialization params
        parentProcessId = request.params.processId;
        clientCapabilities = request.params.capabilities;
        clientName = request.params.clientInfo.name;
        # all workspace folders are indexed; the deprecated rootUri and rootPath parameters are only used when the
        # client does not send workspace folders
        if (request.params.workspaceFolders.typeCode() == NT_LIST && request.params.workspaceFolders) {
            foreach auto folder in (request.params.workspaceFolders) {
                if (folder.typeCode() == NT_HASH && folder.uri.typeCode() == NT_STRING) {
                    addWorkspaceRoot(folder.uri);
                } else {
                    log(0, "WARNING: ignoring invalid workspace folder: %y", folder);
                }
            }
        } else if (request.params.rootUri) {
            addWorkspaceRoot(request.params.rootUri);
        } else if (request.params.rootPath) {
            addWorkspaceRoot(Document::getFileUri(request.params.rootPath));
        }
        applyInitializationOptions(request.params.initializationOptions);

        try {
            # index all Qore files in the current workspace
            parseFilesInWorkspace();
        } catch (hash<ExceptionInfo> ex) {
            return ErrorResponse::invalidParams(request, sprintf("%s: %s: %Y", ex.err, ex.desc, ex));
        }

        try {
            # discover standard Qore modules; source parsing is deferred until needed
            parseStdModules();
        } catch (hash<ExceptionInfo> ex) {
            return ErrorResponse::requestInternalError(request, sprintf("%s: %s: %Y", ex.err, ex.desc, ex));
        }

        try {
            # load builtin metadata from qpp .meta.json + reflection
            loadBuiltinMetadata();
        } catch (hash<ExceptionInfo> ex) {
            log(0, "WARNING: builtin metadata loading failed - %s: %s", ex.err, ex.desc);
        }

        # create response
        hash<auto> result = {
            "capabilities": ServerCapabilities,
        };

        log(0, "initialization complete!");

        initialized = True;
        return make_jsonrpc_response(jsonRpcVer, request.id, result);
    }

    #! "initialized" notification method handler
    private *string meth_initialized(hash<auto> request) {
        clientInitialized = True;
        log(0, "client initialized!");
        startIndexingProgress();
        return NOTHING;
    }

    #! "shutdown" method handler
    private *string meth_shutdown(hash<auto> request) {
        shutdown = True;
        log(0, "shutdown request received");
        return make_jsonrpc_response(jsonRpcVer, request.id, {});
    }

    #! "exit" notification method handler
    private *string meth_exit(hash<auto> request) {
        log(0, "exit request received");
        if (!shutdown) {
            exitCode = 1;
        }
        log(0, "exitCode: %d", exitCode);
        running = False;
        return NOTHING;
    }

    #! "$/cancelRequest" notification method handler
    private *string meth_cancelRequest(hash<auto> request) {
        # main() cancels requests when the notification is read, so that requests being processed can be
        # cancelled; this handles applications that read messages themselves
        if (exists request.params.id) {
            cancelRequest(request.params.id);
        }
        return NOTHING;
    }


    #===================
    # Workspace methods
    #===================

    #! "workspace/didChangeConfiguration" notification method handler
    private *string meth_ws_didChangeConfiguration(hash<auto> request) {
        # validate request
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::didChangeConfigParams(request, request.params);
        if (validation) {
            return validation;
        }

        clientConfig = request.params.settings;
        log(0, "changed configuration received: %Y", clientConfig);

        logging = clientConfig.qore.logging ?? False;
        appendToLog = clientConfig.qore.appendToLog ?? True;
        logVerbosity = clientConfig.qore.logVerbosity ?? 0;
        logVerbosity = (logVerbosity < LOGLEVEL_MIN)
            ? LOGLEVEL_MIN
            : ((logVerbosity > LOGLEVEL_MAX) ? LOGLEVEL_MAX : logVerbosity);

        if ((clientConfig.qore.logFile != NOTHING) &&
            (clientConfig.qore.logFile.typeCode() == NT_STRING)) {
            logFile = clientConfig.qore.logFile;
        } else {
            logFile = getDefaultLogFilePath();
        }
        # close the current log file; it is opened again if logging is enabled
        delete logFh;
        prepareLogFile(logFile);

        if (logging && !logFh) {
            return Notification::showMessage(jsonRpcVer, MessageType.Warning,
                "QLS log file could not be opened. Logging will be turned off.");
        }
        return NOTHING;
    }

    #! "workspace/didChangeWatchedFiles" notification method handler
    private *string meth_ws_didChangeWatchedFiles(hash<auto> request) {
        # validate request
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::didChangeWatchedFilesParams(request, request.params);
        if (validation) {
            return validation;
        }

        # handle changes
        list<auto> changes = request.params.changes;
        foreach hash<auto> change in (changes) {
            string uri = change.uri;
            treeCache.invalidate(uri);
            switch (change.type) {
                case FileChangeType.Created:
                case FileChangeType.Changed:
                    # open documents are kept up to date by the editor
                    if (documents{uri}) {
                        break;
                    }
                    if (!isIndexedWorkspaceFile(uri)) {
                        remove workspaceIndex{uri};
                        delete indexedClassGraph;
                        break;
                    }
                    *FileIndex index = indexFile(uri);
                    if (index) {
                        workspaceIndex{uri} = index;
                    } else {
                        remove workspaceIndex{uri};
                    }
                    delete indexedClassGraph;
                    break;
                case FileChangeType.Deleted:
                    remove workspaceIndex{uri};
                    delete indexedClassGraph;
                    break;
                default:
                    break;
            }
        }
        return NOTHING;
    }

    #! "workspace/didChangeWorkspaceFolders" notification method handler
    /** Indexes added workspace folders and removes the index of the files in removed workspace folders.
    */
    private *string meth_ws_didChangeWorkspaceFolders(hash<auto> request) {
        *hash<auto> event = request.params.event;
        if (event.typeCode() != NT_HASH) {
            log(0, "WARNING: 'workspace/didChangeWorkspaceFolders' notification without a valid event: %y",
                request.params);
            return NOTHING;
        }
        foreach auto folder in (event.removed) {
            if (folder.typeCode() == NT_HASH && folder.uri.typeCode() == NT_STRING) {
                removeWorkspaceRoot(folder.uri);
            }
        }
        foreach auto folder in (event.added) {
            if (folder.typeCode() == NT_HASH && folder.uri.typeCode() == NT_STRING) {
                *string root = addWorkspaceRoot(folder.uri);
                if (root) {
                    indexWorkspaceRoot(root);
                }
                startIndexingProgress();
            }
        }
        return NOTHING;
    }

    #! Returns @ref True if the given file is in the workspace and is indexed according to the scan options
    private:internal bool isIndexedWorkspaceFile(string uri) {
        if (uri !~ /^file:\//) {
            return False;
        }
        string path = Document::getFilePath(uri);
        *string root = getWorkspaceRoot(path);
        if (!root || !Files::is_valid_qore_file(path)) {
            return False;
        }
        return !Files::is_excluded_path(path, root, scanOptions);
    }

    #! "workspace/symbol" method handler
    private *string meth_ws_symbol(hash<auto> request) {
        # validate request
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::workspaceSymbolParams(request, request.params);
        if (validation) {
            return validation;
        }

        # get the search query
        string query = request.params.query;
        log(1, "workspace symbols requested; query: '%s'", query);

        # find matching symbols
        list<auto> symbols = findWorkspaceSymbols(query, False);
        log(1, "found %d workspace symbols for query: '%s'", symbols.size(), query);

        return make_jsonrpc_response(jsonRpcVer, request.id, symbols);
    }


    #==================
    # Document methods
    #==================

    #! "textDocument/didOpen" notification method handler
    private softlist<auto> meth_td_didOpen(hash<auto> request) {
        # validate request
        if (!request.hasKey("params")) {
            log(1, "'textDocument/didOpen' request missing required attribute 'params'");
            return NOTHING;
        }

        # extract params hashes
        list<auto> paramsList;
        if (request.params.typeCode() == NT_LIST) {
            foreach auto p in (request.params) {
                if (p) {
                    paramsList += p;
                }
            }
        } else if (request.params.typeCode() == NT_HASH) {
            paramsList += request.params;
        } else {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is not an array or object");
        }

        # go through the params hashes
        list<auto> responses;
        foreach hash<auto> params in (paramsList) {
            *string validation = ParamValidator::textDocumentItem(request, params);
            if (validation) {
                log(1, "'textDocument/didOpen' request failed validation: " + validation);
                continue;
            }

            reference textDoc = \params.textDocument;
            *Document doc;
            # ignore non-local files
            if (textDoc.uri =~ /^file:\//) {
                # the open document replaces the index of the file; the editor text may differ from the file on disk
                if (remove workspaceIndex{textDoc.uri}) {
                    delete indexedClassGraph;
                }
                treeCache.invalidate(textDoc.uri);
                doc = new Document(textDoc.uri, textDoc.text, textDoc.languageId, textDoc.version);
            } else {
                ignoredUris{textDoc.uri} = True;
                log(1, "added URI to ignored: '%s'", textDoc.uri);
            }
            if (doc != NOTHING) {
                documents{textDoc.uri} = doc;
                log(1, "opened text document: '%s'", textDoc.uri);
                # the Qore parser checks the file, so the editor text must be the same as the file
                if (QLSServer::sameAsFile(doc)) {
                    checkWithQore(doc);
                }
                responses += publishDiagnostics(doc);
                continue;
            }

            responses += Notification::diagnostics(jsonRpcVer, textDoc.uri);
        }
        return responses;
    }

    #! "textDocument/didChange" notification method handler
    private *string meth_td_didChange(hash<auto> request) {
        # validate request
        if (!request.hasKey("params")) {
            log(1, "'textDocument/didChange' request missing required attribute 'params'");
            return NOTHING;
        }
        *string validation = ParamValidator::versionedTextDocumentIdentifier(request, request.params);
        if (validation) {
            log(1, "'textDocument/didChange' request failed validation: " + validation);
            return NOTHING;
        }

        # handle changes
        hash<auto> textDoc = request.params.textDocument;
        if (ignoredUris{textDoc.uri}) {
            log(2, sprintf("'textDocument/didChange' request for ignored URI '%s'", textDoc.uri));
            return NOTHING;
        }
        if (!exists documents{textDoc.uri}) {
            log(1, sprintf("'textDocument/didChange' request attempted to change non-existent or not opened "
                "document '%s'", textDoc.uri));
            return NOTHING;
        }

        reference doc = \documents{textDoc.uri};
        foreach hash<auto> change in (request.params.contentChanges) {
            doc.didChange(change);
        }
        doc.changeVersion(textDoc.version);
        log(1, "text document changed: '%s'", textDoc.uri);

        # the diagnostics of the last Qore check are for the saved text, so the syntax errors are published until
        # the document is saved again
        return publishDiagnostics(doc);
    }

    #! "textDocument/willSave" notification method handler
    private *string meth_td_willSave(hash<auto> request) {
        # ignore for now
        return NOTHING;
    }

    #! "textDocument/willSaveWaitUntil" method handler
    private *string meth_td_willSaveWaitUntil(hash<auto> request) {
        return ErrorResponse::invalidRequest(request);
    }

    #! "textDocument/didSave" notification method handler
    private *string meth_td_didSave(hash<auto> request) {
        *string uri = request.params.textDocument.uri;
        if (!uri || ignoredUris{uri} || !documents{uri}) {
            return NOTHING;
        }
        Document doc = documents{uri};
        checkWithQore(doc);
        return publishDiagnostics(doc);
    }

    #! Returns the diagnostics notification for an open document
    /** The diagnostics of the last Qore check are published if the document has not changed since, as they
        include the syntax errors and are more accurate; otherwise the syntax errors of the syntax tree are
        published.
    */
    private:internal string publishDiagnostics(Document doc) {
        string uri = doc.getUri();
        *hash<auto> checked = qoreDiagnostics{uri};
        if (checked && checked.version == doc.getVersion()) {
            return Notification::diagnostics(jsonRpcVer, uri, checked.diagnostics);
        }
        return Notification::diagnostics(jsonRpcVer, uri, doc.getParseErrorCount() ? doc.getDiagnostics() : ());
    }

    #! Returns @ref True if the text of an open document is the same as the file
    private:internal static bool sameAsFile(Document doc) {
        try {
            return ReadOnlyFile::readTextFile(Document::getFilePath(doc.getUri())) == doc.getText();
        } catch (hash<ExceptionInfo> ex) {
            # a file that cannot be read cannot be checked
        }
        return False;
    }

    #! Returns @ref True if the file is a standalone script that can be checked with the Qore parser
    /** Module, class and Qorus files are parts of a larger program and would report errors for the parts that
        they do not contain.
    */
    static bool isStandaloneScript(string path) {
        *string ext = (path =~ x/\.([^.\/]+)$/)[0];
        if (ext) {
            return StandaloneScriptExtensions{ext.lwr()} ?? False;
        }
        # scripts without an extension are recognized by their "#!" line
        return Files::is_valid_qore_file(path);
    }

    #! The extensions of standalone Qore scripts
    const StandaloneScriptExtensions = {
        "q": True,
        "qr": True,
        "qtest": True,
    };

    #! Checks a saved document with the Qore parser and stores its diagnostics
    /** Standalone scripts are parsed with <tt>qore --diag-format=json</tt> in a separate process, so the modules
        that the script requires are loaded there and not in the server.  Other documents, and documents whose
        check fails, keep the syntax errors of the syntax tree.
    */
    private:internal checkWithQore(Document doc) {
        string uri = doc.getUri();
        remove qoreDiagnostics{uri};
        if (diagnosticsMode != "qore" || uri !~ /^file:\//) {
            return;
        }
        string path = Document::getFilePath(uri);
        if (!QLSServer::isStandaloneScript(path)) {
            return;
        }
        # the Windows command shell expands %name% even in quoted arguments, so such paths cannot be passed safely
        if (PlatformOS == "Windows" && (path.find("%") != -1 || getQoreExecutable().find("%") != -1)) {
            log(0, "WARNING: not checking %s: the path cannot be passed to the command shell", path);
            return;
        }
        string cmd = sprintf("%s --diag-format=json -W %s %s", QLSServer::shellQuote(getQoreExecutable()),
            QLSServer::shellQuote(path), PlatformOS == "Windows" ? "2>NUL" : "2>/dev/null");
        int rc;
        auto parsed;
        try {
            string out = backquote(cmd, \rc);
            parsed = parse_json(trim(out));
        } catch (hash<ExceptionInfo> ex) {
            log(0, "WARNING: checking %s with %y failed (exit code %d): %s: %s", path, getQoreExecutable(), rc,
                ex.err, ex.desc);
            return;
        }
        if (parsed.typeCode() != NT_LIST) {
            log(0, "WARNING: checking %s with %y gave no diagnostics list (exit code %d)", path, getQoreExecutable(),
                rc);
            return;
        }
        list<hash<auto>> diagnostics = ();
        foreach auto d in (parsed) {
            # diagnostics in included files are not shown in this document
            if (d.typeCode() == NT_HASH && d.file == path) {
                push diagnostics, QLSServer::makeQoreDiagnostic(d, doc);
            }
        }
        qoreDiagnostics{uri} = {"version": doc.getVersion(), "diagnostics": diagnostics};
        log(1, "Qore check of %s: %d diagnostics", path, diagnostics.size());
    }

    #! Returns the Qore executable used to check scripts
    private:internal string getQoreExecutable() {
        if (!qoreExecutable) {
            # the interpreter running the server has the same version and module path
            qoreExecutable = is_file("/proc/self/exe") ? realpath("/proc/self/exe") : "qore";
        }
        return qoreExecutable;
    }

    #! Returns the argument quoted for the command shell
    static string shellQuote(string arg) {
        if (PlatformOS == "Windows") {
            return "\"" + replace(arg, "\"", "\\\"") + "\"";
        }
        return "'" + replace(arg, "'", "'\\''") + "'";
    }

    #! Returns an LSP diagnostic for a Qore parse diagnostic
    /** @param d a diagnostic in the format of @ref Qore::ParseDiagnosticInfo "ParseDiagnosticInfo": lines are
        1-based, columns are 0-based and the end column is exclusive; a column of -1 is unknown
        @param doc the document, for the length of lines when the end column is unknown

        @return an LSP diagnostic; the fixes of the Qore diagnostic are in its \c data
    */
    static hash<auto> makeQoreDiagnostic(hash<auto> d, Document doc) {
        int startLine = max(d.startLine - 1, 0);
        int endLine = d.endLine > 0 ? d.endLine - 1 : startLine;
        int startChar = d.startColumn >= 0 ? d.startColumn : 0;
        int endChar = d.endColumn >= 0 ? d.endColumn : (doc.getLine(endLine) ?? "").length();
        if (endLine == startLine && endChar < startChar) {
            endChar = startChar;
        }
        hash<auto> diag = {
            "range": {
                "start": {"line": startLine, "character": startChar},
                "end": {"line": endLine, "character": endChar},
            },
            "severity": d.severity == "error" ? DiagnosticSeverity.Error : DiagnosticSeverity.Warning,
            "source": "qore",
            "code": d.id ?? d.code,
            "message": d.hint ? sprintf("%s\n%s", d.message, d.hint) : d.message,
        };
        if (d.fixes) {
            diag.data = {"fixes": d.fixes};
        }
        return diag;
    }

    #! LSP DiagnosticSeverity values
    const DiagnosticSeverity = {
        "Error": 1,
        "Warning": 2,
    };

    #! "textDocument/didClose" notification method handler
    private *string meth_td_didClose(hash<auto> request) {
        # validate request
        if (!request.hasKey("params")) {
            log(1, "'textDocument/didClose' request missing required attribute 'params'");
            return NOTHING;
        }
        *string validation = ParamValidator::textDocumentIdentifier(request, request.params);
        if (validation) {
            log(1, "'textDocument/didClose' request failed validation: " + validation);
            return NOTHING;
        }

        if (ignoredUris{request.params.textDocument.uri}) {
            log(2, sprintf("'textDocument/didClose' request for ignored URI '%s'",
                request.params.textDocument.uri));
            return NOTHING;
        }

        if (!exists documents{request.params.textDocument.uri}) {
            log(1, sprintf("'textDocument/didClose' request attempted to close non-existent or not opened "
                "document '%s'", request.params.textDocument.uri));
            return NOTHING;
        }

        string uri = request.params.textDocument.uri;
        # keep the symbols of the closed document and release its syntax tree
        workspaceIndex{uri} = (remove documents{uri}).getIndex();
        delete indexedClassGraph;
        remove qoreDiagnostics{uri};
        remove semanticTokensResults{uri};
        log(1, "closed text document: '%s'", uri);
        # the diagnostics of a closed document are removed
        return Notification::diagnostics(jsonRpcVer, uri);
    }

    #! "textDocument/hover" method handler
    private *string meth_td_hover(hash<auto> request) {
        # validate request
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::textDocumentPositionParams(request, request.params);
        if (validation) {
            return validation;
        }

        if (ignoredUris{request.params.textDocument.uri}) {
            log(2, sprintf("'textDocument/hover' request for ignored URI '%s'",
                request.params.textDocument.uri));
            return make_jsonrpc_response(jsonRpcVer, request.id, {});
        }

        if (!exists documents{request.params.textDocument.uri}) {
            return ErrorResponse::nonExistentDocument(request);
        }

        # get the document
        Document doc = documents{request.params.textDocument.uri};

        # find requested symbol in the document
        *hash<auto> symbolInfo = doc.findSymbolInfo(request.params.position);
        if (!symbolInfo) {
            return make_jsonrpc_response(jsonRpcVer, request.id, doc.hover(request.params.position));
        }

        # prepare search query
        string query = symbolInfo.name;
        {
            int lastDoubleColon = query.rfind("::");
            if (lastDoubleColon != -1) {
                query = query.substr(lastDoubleColon + 2);
            }
            if (query.equalPartial("*")) {
                query = query.substr(1);
            }
        }
        bool queryCanBeSourceSymbol = query =~ /^[A-Za-z_][A-Za-z0-9_]*$/;

        # find matching symbols in already parsed documents first; standard modules are loaded lazily below
        list<auto> symbols = findWorkspaceSymbols(query, True);

        for (int i = symbols.size() - 1; i >= 0; i--) {
            if (SymbolUsageFastMap{string(symbolInfo.usage)}) {
                if (!SymbolUsageFastMap{string(symbolInfo.usage)}.contains(symbols[i].kind)) {
                    splice symbols, i, 1;
                }
            }
        }
        bool parsedSymbolMatched = !symbols.empty();

        # prepare results
        hash<auto> result = {"range": symbolInfo.range, "contents": ()};
        foreach hash<auto> symbol in (symbols) {
            *hash<auto> description;
            *Document symbolDoc = getAnyDocument(symbol.location.uri);
            if (symbolDoc) {
                description = symbolDoc.hoverInfo(symbol.kind, symbol.location.range.start);
            }

            if (description) {
                result.contents += replace(description.description, "*", "\\*");
            }
        }

        # metadata fallback for builtins not found in parsed documents
        if (!result.contents.size() && builtinMetadata) {
            *string hover_md = buildMetadataHover(query, symbolInfo);
            if (hover_md) {
                result.contents += hover_md;
            }
        }

        # full standard-module source parsing is expensive, so only use it after metadata/open docs miss
        if (!result.contents.size() && !parsedSymbolMatched && queryCanBeSourceSymbol) {
            list<auto> stdSymbols = findStdModuleSymbols(query, "hover: " + query);

            for (int i = stdSymbols.size() - 1; i >= 0; i--) {
                if (SymbolUsageFastMap{string(symbolInfo.usage)}) {
                    if (!SymbolUsageFastMap{string(symbolInfo.usage)}.contains(stdSymbols[i].kind)) {
                        splice stdSymbols, i, 1;
                    }
                }
            }

            foreach hash<auto> symbol in (stdSymbols) {
                *hash<auto> description;
                *Document symbolDoc = getIndexedDocument(symbol.location.uri);
                if (symbolDoc) {
                    description = symbolDoc.hoverInfo(symbol.kind, symbol.location.range.start);
                }

                if (description) {
                    result.contents += replace(description.description, "*", "\\*");
                }
            }
        }

        return make_jsonrpc_response(jsonRpcVer, request.id, result);
    }

    #! Build hover Markdown from builtin metadata
    /** @param name unqualified symbol name
        @param symbolInfo symbol info from findSymbolInfo (has usage, name, range)
        @return Markdown string or NOTHING if no metadata found
    */
    private *string buildMetadataHover(string name, hash<auto> symbolInfo) {
        *list<hash<auto>> entries = metaByName{name};
        if (!entries) {
            return NOTHING;
        }

        # filter by symbol kind using usage info
        *list<auto> validKinds = SymbolUsageFastMap{string(symbolInfo.usage)};
        list<hash<auto>> filtered = ();
        foreach hash<auto> entry in (entries) {
            if (validKinds) {
                int kind = metaSymbolTypeToKind(entry.symbol_type);
                if (kind && !validKinds.contains(kind)) {
                    continue;
                }
            }
            filtered += entry;
        }
        if (!filtered.size()) {
            # if filtering removed everything, try all entries
            filtered = entries;
        }

        list<string> parts = ();
        foreach hash<auto> entry in (filtered) {
            *string md = formatMetadataEntry(entry);
            if (md) {
                parts += md;
            }
        }

        return parts.size() ? parts.join("\n\n---\n\n") : NOTHING;
    }

    #! Map metadata symbol_type to ASTSymbolKind
    private static int metaSymbolTypeToKind(string symbol_type) {
        switch (symbol_type) {
            case "class": return ASTSymbolKind.Class;
            case "function": return ASTSymbolKind.Function;
            case "method": return ASTSymbolKind.Method;
            case "static_method": return ASTSymbolKind.Method;
            case "constant": return ASTSymbolKind.Constant;
            case "hashdecl": return ASTSymbolKind.Interface;
            case "enum": return ASTSymbolKind.Enum;
        }
        return 0;
    }

    #! Format a single metadata entry as Markdown for hover
    private static *string formatMetadataEntry(hash<auto> entry) {
        list<string> parts = ();

        # signature as fenced code block
        if (entry.signature) {
            parts += sprintf("```qore\n%s\n```", entry.signature);
        } else if (entry.symbol_type == "class") {
            string header = "class " + entry.full_name;
            if (entry.parents) {
                header += " inherits " + entry.parents.join(", ");
            }
            parts += sprintf("```qore\n%s\n```", header);
        } else if (entry.symbol_type == "constant") {
            string header = "const " + entry.full_name;
            if (entry.type_name) {
                header = entry.type_name + " " + header;
            }
            parts += sprintf("```qore\n%s\n```", header);
        } else if (entry.symbol_type == "hashdecl") {
            parts += sprintf("```qore\nhashdecl %s\n```", entry.full_name);
        } else if (entry.symbol_type == "enum") {
            parts += sprintf("```qore\nenum %s\n```", entry.full_name);
        }

        # description
        if (entry.description) {
            parts += string(entry.description);
        }

        # parameters table
        if (entry.params && entry.params.size()) {
            list<string> paramLines = ("", "**Parameters:**", "");
            foreach hash<auto> p in (entry.params) {
                string line = sprintf("- `%s %s`", p.type_name ?? "auto", p.name);
                if (p.default_value) {
                    line += sprintf(" = `%s`", p.default_value);
                }
                if (p.description) {
                    line += " — " + string(p.description);
                }
                paramLines += line;
            }
            parts += paramLines.join("\n");
        }

        # return description
        if (entry.return_description) {
            parts += sprintf("**Returns:** %s", entry.return_description);
        }

        # throws
        if (entry.throws && entry.throws.size()) {
            list<string> throwLines = ("**Throws:**",);
            foreach hash<auto> t in (entry.throws) {
                string line = sprintf("- `%s`", t.name);
                if (t.description) {
                    line += " — " + string(t.description);
                }
                throwLines += line;
            }
            parts += throwLines.join("\n");
        }

        # annotations
        if (entry.since) {
            parts += sprintf("**Since:** %s", entry.since);
        }
        if (entry.is_deprecated) {
            string dep = "**Deprecated**";
            if (entry.deprecated_reason) {
                dep += ": " + string(entry.deprecated_reason);
            }
            parts += dep;
        }

        return parts.size() ? parts.join("\n\n") : NOTHING;
    }

    #! "textDocument/references" method handler
    private *string meth_td_references(hash<auto> request) {
        # validate request
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::referenceParams(request, request.params);
        if (validation) {
            return validation;
        }

        if (ignoredUris{request.params.textDocument.uri}) {
            log(2, sprintf("'textDocument/references' request for ignored URI '%s'",
                request.params.textDocument.uri));
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        if (!exists documents{request.params.textDocument.uri}) {
            return ErrorResponse::nonExistentDocument(request);
        }

        # find references in the open documents and the workspace
        Document doc = documents{request.params.textDocument.uri};
        bool includeDecl = request.params.context.includeDeclaration ?? False;
        *list<auto> references = findReferencesCrossDoc(doc, request.params.position, includeDecl)
            ?? doc.findReferences(request.params.position, includeDecl);
        return make_jsonrpc_response(jsonRpcVer, request.id, references ?? ());
    }

    #! Symbol kinds whose declarations contain local variables
    const CallableSymbolKinds = {
        ASTSymbolKind.Function: True,
        ASTSymbolKind.Method: True,
        ASTSymbolKind.Constructor: True,
    };

    #! Returns @ref True if the outer LSP range contains the inner range
    private:internal static bool rangeContains(hash<auto> outer, hash<auto> inner) {
        return (outer.start.line < inner.start.line
                || (outer.start.line == inner.start.line && outer.start.character <= inner.start.character))
            && (inner.end.line < outer.end.line
                || (inner.end.line == outer.end.line && inner.end.character <= outer.end.character));
    }

    #! Returns the symbols of a document as a tree of LSP \c DocumentSymbol hashes
    /** Symbols are nested by their ranges and named without namespace or class prefixes; \c selectionRange is the
        declared name.  Local variables of functions and methods are not included.
    */
    private:internal list<hash<auto>> getDocumentSymbolTree(Document doc) {
        *list<auto> symbols = doc.findSymbols(True, True);
        if (!symbols) {
            return ();
        }
        # symbols are in tree order, so the enclosing symbols of a symbol are the open ones that contain it
        list<int> parents = ();
        list<int> open = ();
        for (int i = 0; i < symbols.size(); ++i) {
            hash<auto> range = symbols[i].location.range;
            while (open && !QLSServer::rangeContains(symbols[open.last()].location.range, range)) {
                pop open;
            }
            push parents, open ? open.last() : -1;
            push open, i;
        }
        hash<string, list<int>> children = {};
        list<int> roots = ();
        for (int i = 0; i < symbols.size(); ++i) {
            int parent = parents[i];
            if (parent == -1) {
                push roots, i;
            } else if (symbols[i].kind != ASTSymbolKind.Variable || !CallableSymbolKinds{symbols[parent].kind}) {
                push children{parent}, i;
            }
        }
        code<hash<auto>(int)> makeSymbol;
        makeSymbol = hash<auto> sub (int i) {
            hash<auto> sym = symbols[i];
            hash<auto> range = sym.location.range;
            *hash<auto> namePos = doc.findNamePosition(sym.name, range);
            hash<auto> selectionRange = namePos
                ? {"start": namePos, "end": namePos + {"character": namePos.character + sym.name.length()}}
                : {"start": range.start, "end": range.start};
            return {
                "name": sym.name,
                "kind": LspDocumentSymbolKindMap{string(sym.kind)} ?? 1,
                "range": range,
                "selectionRange": selectionRange,
                "children": map makeSymbol($1), children{i} ?? (),
            };
        };
        return map makeSymbol($1), roots;
    }

    #! "textDocument/documentSymbol" method handler
    private softlist<auto> meth_td_documentSymbol(hash<auto> request) {
        # validate request
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }

        # extract params hashes from the params key
        list<auto> paramsList;
        if (request.params.typeCode() == NT_LIST) {
            foreach auto p in (request.params) {
                if (p && p.typeCode() == NT_HASH) {
                    paramsList += p;
                }
            }
        } else if (request.params.typeCode() == NT_HASH) {
            paramsList += request.params;
        } else {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is not an array or object");
        }

        # go through the params hashes
        list<auto> responses;
        foreach hash<auto> params in (paramsList) {
            *string validation = ParamValidator::textDocumentIdentifier(request, params);
            if (validation) {
                return validation;
            }

            if (ignoredUris{params.textDocument.uri}) {
                log(2, sprintf("'textDocument/documentSymbol' request for ignored URI '%s'",
                    params.textDocument.uri));
                responses += make_jsonrpc_response(jsonRpcVer, request.id, ());
                continue;
            }

            if (!exists documents{params.textDocument.uri}) {
                responses += ErrorResponse::nonExistentDocument(request);
                continue;
            }

            # find document symbols
            Document doc = documents{params.textDocument.uri};

            *list<auto> ret;
            switch (params.retType) {
                case 'node_info':
                    ret = doc.getNodesInfo();
                    break;
                default: {
                    # the extension API asks for the flat list with an explicit retType
                    if (!params.retType && clientCapabilities.textDocument.documentSymbol
                            .hierarchicalDocumentSymbolSupport) {
                        ret = getDocumentSymbolTree(doc);
                        break;
                    }
                    ret = doc.findSymbols();
                    if (ret) {
                        for (int i = 0; i < ret.size(); ++i) {
                            ret[i] = ret[i] + {
                                "kind": LspDocumentSymbolKindMap{string(ret[i].kind)} ?? 1,
                            };
                        }
                    }
                }
            }
            responses += make_jsonrpc_response(jsonRpcVer, request.id, ret ?? ());
            continue;
        }
        return responses;
    }

    #! LSP CompletionItemKind constants
    const LspCompletionItemKind = {
        "keyword": 14,
        "function": 3,
        "class": 7,
        "variable": 6,
        "constant": 21,
        "namespace": 9,
        "method": 2,
        "member": 5,
        "hashdecl": 22,
        "enum": 13,
        "enum_value": 20,
        "type_parameter": 25,
    };

    #! "textDocument/completion" method handler
    private *string meth_td_completion(hash<auto> request) {
        # validate request
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::textDocumentPositionParams(request, request.params);
        if (validation) {
            return validation;
        }

        if (ignoredUris{request.params.textDocument.uri}) {
            log(2, sprintf("'textDocument/completion' request for ignored URI '%s'",
                request.params.textDocument.uri));
            return make_jsonrpc_response(jsonRpcVer, request.id, {
                "isIncomplete": False,
                "items": (),
            });
        }

        if (!exists documents{request.params.textDocument.uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, {
                "isIncomplete": False,
                "items": (),
            });
        }

        # get the document
        Document doc = documents{request.params.textDocument.uri};

        # Extract current line text up to cursor
        int line = request.params.position.line;
        int col = request.params.position.character;
        *string lineText = doc.getLine(line);
        string input = lineText && col <= lineText.length() ? lineText.substr(0, col) : "";

        # Create provider + engine for this request
        CSTCompletionProvider provider(doc, request.params.position, metaByName, metaClassByName,
            metaPrefixIndex);
        QoreCodeCompletion::CompletionEngine engine(provider);

        # Get completions
        list<hash<QoreCodeCompletion::CompletionItem>> items = engine.complete(input);

        # Convert to LSP format
        hash<auto> itemCaps = clientCapabilities.textDocument.completion.completionItem ?? {};
        bool snippets = itemCaps.snippetSupport ?? False;
        bool markdown = (itemCaps.documentationFormat ?? ()).contains("markdown");
        # the commands that show parameter hints and suggestions after inserting an item are VS Code commands
        bool editorCommands = clientName =~ /visual studio code|vscodium|code - oss/i;
        list<hash<auto>> lspItems = map QLSServer::makeCompletionItem($1, snippets, markdown, editorCommands), items;

        return make_jsonrpc_response(jsonRpcVer, request.id, {
            "isIncomplete": False,
            "items": lspItems,
        });
    }

    #! Returns an LSP \c CompletionItem for a completion
    /** @param item the completion
        @param snippets whether the client supports snippets; callables with arguments are then inserted with
        parentheses and the cursor between them; callables without arguments are always inserted with parentheses
        @param markdown whether the client supports Markdown documentation
        @param editorCommands whether the client is VS Code or a compatible editor, whose commands show parameter
        hints after a callable and suggestions after a namespace are inserted

        @return the LSP completion item
    */
    static hash<auto> makeCompletionItem(hash<QoreCodeCompletion::CompletionItem> item, bool snippets, bool markdown,
            bool editorCommands) {
        hash<auto> lspItem = {
            "label": item.label,
            "kind": LspCompletionItemKind{item.kind} ?? 1,
            "data": {
                "name": item.label,
                "symbolKind": item.kind,
            },
        };
        if (exists item.detail) {
            lspItem.detail = item.detail;
        }
        if (item.documentation) {
            lspItem.documentation = markdown
                ? {"kind": "markdown", "value": item.documentation}
                : item.documentation;
        }
        if (item.isCallable && item.noArgsOnly) {
            # nothing to enter between the parentheses
            lspItem.insertText = item.label + "()";
        } else if (item.isCallable) {
            if (snippets) {
                # the cursor is placed between the parentheses
                lspItem.insertText = QLSServer::escapeSnippet(item.label) + "($0)";
                lspItem.insertTextFormat = InsertTextFormat.Snippet;
            } else {
                lspItem.insertText = item.label + "(";
            }
            if (editorCommands) {
                lspItem.command = {
                    "title": "Show parameter hints",
                    "command": "editor.action.triggerParameterHints",
                };
            }
        } else if (item.kind == "namespace") {
            lspItem.insertText = item.label + "::";
            if (editorCommands) {
                lspItem.command = {"title": "Show suggestions", "command": "editor.action.triggerSuggest"};
            }
        } else {
            lspItem.insertText = item.label;
        }
        return lspItem;
    }

    #! LSP InsertTextFormat values
    const InsertTextFormat = {
        "PlainText": 1,
        "Snippet": 2,
    };

    #! Returns the text with the characters that have a meaning in LSP snippets escaped
    private:internal static string escapeSnippet(string text) {
        # a literal backslash before the matched character
        return regex_subst(text, "([\\\\$}])", "\\\\$1", RE_Global);
    }

    #! "completionItem/resolve" method handler
    /** Enriches a completion item with documentation from builtin metadata.
        @param request LSP request with params: the CompletionItem to resolve
        @return JSON-RPC response with enriched CompletionItem
    */
    private *string meth_completionItem_resolve(hash<auto> request) {
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }

        hash<auto> item = request.params;

        # if already has documentation, return as-is
        if (item.documentation) {
            return make_jsonrpc_response(jsonRpcVer, request.id, item);
        }

        # try to enrich from metadata
        if (builtinMetadata && item.data.name) {
            string name = item.data.name;
            *list<hash<auto>> entries = metaByName{name};
            if (entries) {
                # find best match by symbol kind
                *hash<auto> best;
                string targetKind = item.data.symbolKind ?? "";
                foreach hash<auto> entry in (entries) {
                    if (targetKind == "class" && entry.symbol_type == "class") {
                        best = entry;
                        break;
                    } else if (targetKind == "function" && entry.symbol_type == "function") {
                        best = entry;
                        break;
                    } else if (targetKind == "method"
                            && (entry.symbol_type == "method"
                                || entry.symbol_type == "static_method")) {
                        best = entry;
                        break;
                    } else if (targetKind == "constant" && entry.symbol_type == "constant") {
                        best = entry;
                        break;
                    } else if (!best) {
                        best = entry;
                    }
                }

                if (best) {
                    *string doc = formatMetadataEntry(best);
                    if (doc) {
                        item.documentation = {
                            "kind": "markdown",
                            "value": doc,
                        };
                    }
                }
            }
        }

        return make_jsonrpc_response(jsonRpcVer, request.id, item);
    }

    #! "textDocument/onTypeFormatting" method handler
    /** Handles auto-indent on Enter and auto-dedent on closing brace.
        @param request LSP request with params: textDocument, position, ch, options
        @return JSON-RPC response with TextEdit[] or empty list
    */
    private *string meth_td_onTypeFormatting(hash<auto> request) {
        # validate request
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        if (!request.params.hasKey("textDocument") || !request.params.hasKey("position")
                || !request.params.hasKey("ch")) {
            return ErrorResponse::invalidRequest(request,
                "required attributes 'textDocument', 'position', and 'ch' are missing");
        }

        string uri = request.params.textDocument.uri;

        if (ignoredUris{uri}) {
            log(2, sprintf("'textDocument/onTypeFormatting' request for ignored URI '%s'", uri));
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        if (!exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        Document doc = documents{uri};
        string ch = request.params.ch;
        int line = request.params.position.line;

        if (ch == "}") {
            # Auto-dedent: if the line up to the } is all whitespace, reduce indent by one level
            *string lineText = doc.getLine(line);
            if (!lineText) {
                return make_jsonrpc_response(jsonRpcVer, request.id, ());
            }

            int charPos = request.params.position.character;
            # The character position is after the }, so the prefix is everything before it
            int prefixLen = charPos > 0 ? charPos - 1 : 0;
            string prefix = prefixLen > 0 ? lineText.substr(0, prefixLen) : "";

            # Only dedent if the prefix is all whitespace
            int indentSize = QoreCodeFormat::SourceEmitter::INDENT_STR.length();
            if (prefix.regex("^\\s*$") && prefix.length() >= indentSize) {
                int desiredSpaces = prefix.length() - indentSize;
                if (desiredSpaces < 0) {
                    desiredSpaces = 0;
                }
                list<auto> edits = ({
                    "range": {
                        "start": {"line": line, "character": 0},
                        "end": {"line": line, "character": prefixLen},
                    },
                    "newText": strmul(" ", desiredSpaces),
                },);
                return make_jsonrpc_response(jsonRpcVer, request.id, edits);
            }

            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        } else if (ch == "\n") {
            # Auto-indent: calculate indent level from all code up to the previous line
            if (line < 1) {
                return make_jsonrpc_response(jsonRpcVer, request.id, ());
            }

            *string docText = doc.getText();
            if (!docText) {
                return make_jsonrpc_response(jsonRpcVer, request.id, ());
            }

            list<string> allLines = docText.split("\n");
            # Take lines 0 through line-1 (the code before the new line)
            int prevLineCount = line < allLines.size() ? line : allLines.size();
            string codeUpToHere = (extract allLines, 0, prevLineCount).join("\n");

            int indent = QoreCodeFormat::CodeFormatter::calculateIndentLevel(codeUpToHere);
            if (indent > 0) {
                string indentText = strmul(QoreCodeFormat::SourceEmitter::INDENT_STR, indent);
                list<auto> edits = ({
                    "range": {
                        "start": {"line": line, "character": 0},
                        "end": {"line": line, "character": 0},
                    },
                    "newText": indentText,
                },);
                return make_jsonrpc_response(jsonRpcVer, request.id, edits);
            }

            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        return make_jsonrpc_response(jsonRpcVer, request.id, ());
    }

    #! "textDocument/formatting" method handler
    /** Formats the entire document using QoreCodeFormat::CodeFormatter.
        @param request LSP request with params: textDocument, options
        @return JSON-RPC response with TextEdit[] or empty list
    */
    private *string meth_td_formatting(hash<auto> request) {
        # validate request
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::textDocumentIdentifier(request, request.params);
        if (validation) {
            return validation;
        }

        string uri = request.params.textDocument.uri;

        if (ignoredUris{uri}) {
            log(2, sprintf("'textDocument/formatting' request for ignored URI '%s'", uri));
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        if (!exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        Document doc = documents{uri};
        *string text = doc.getText();
        if (!text || text.empty()) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        # Format the text
        string formatted;
        try {
            QoreCodeFormat::CodeFormatter fmt();
            formatted = fmt.format(text);
        } catch (hash<ExceptionInfo> ex) {
            log(1, "formatting failed: %s: %s", ex.err, ex.desc);
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        # If formatted text is the same as original, return empty list
        if (formatted == text) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        # Build a single TextEdit replacing the entire document
        list<string> lines = text.split("\n");
        int lastLine = lines.size() - 1;
        int lastLineLen = lines[lastLine].length();

        list<auto> edits = ({
            "range": {
                "start": {"line": 0, "character": 0},
                "end": {"line": lastLine, "character": lastLineLen},
            },
            "newText": formatted,
        },);

        return make_jsonrpc_response(jsonRpcVer, request.id, edits);
    }

    #! "textDocument/rangeFormatting" method handler
    /** Formats the document and returns the changed lines that overlap the range, as the formatter needs the
        whole document.
    */
    private *string meth_td_rangeFormatting(hash<auto> request) {
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::textDocumentIdentifier(request, request.params);
        if (validation) {
            return validation;
        }
        if (request.params.range.start.line.typeCode() != NT_INT
                || request.params.range.end.line.typeCode() != NT_INT) {
            return ErrorResponse::invalidParams(request,
                "required attribute 'range' with 'start' and 'end' is missing");
        }
        string uri = request.params.textDocument.uri;
        if (ignoredUris{uri} || !exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }
        *string text = documents{uri}.getText();
        if (!text) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }
        string formatted;
        try {
            QoreCodeFormat::CodeFormatter fmt();
            formatted = fmt.format(text);
        } catch (hash<ExceptionInfo> ex) {
            log(1, "formatting failed: %s: %s", ex.err, ex.desc);
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }
        return make_jsonrpc_response(jsonRpcVer, request.id,
            QLSServer::getRangeFormattingEdits(text, formatted, request.params.range));
    }

    #! Returns the edits of a formatted document that overlap a range
    /** @param text the document text
        @param formatted the formatted document text
        @param range the LSP range to format; a range that ends at the start of a line does not include that line

        @return the LSP text edits for the changed lines that overlap the range
    */
    static list<hash<auto>> getRangeFormattingEdits(string text, string formatted, hash<auto> range) {
        int first = range.start.line;
        int last = range.end.line;
        if (last > first && !range.end.character) {
            --last;
        }
        code<bool(hash<LineHunk>)> overlaps = bool sub (hash<LineHunk> h) {
            # lines inserted between two lines belong to the range if the line after them is in it
            return h.oldStart <= last && (h.oldEnd > first || (h.oldEnd == h.oldStart && h.oldStart >= first));
        };
        return TextDiff::getTextEdits(text, formatted, overlaps);
    }

    #! "textDocument/documentHighlight" method handler
    /** Highlights all occurrences of the symbol under the cursor.
        @param request LSP request with params: textDocument, position
        @return JSON-RPC response with DocumentHighlight[] or empty list
    */
    private *string meth_td_documentHighlight(hash<auto> request) {
        # validate request
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::textDocumentPositionParams(request, request.params);
        if (validation) {
            return validation;
        }

        string uri = request.params.textDocument.uri;

        if (ignoredUris{uri}) {
            log(2, sprintf("'textDocument/documentHighlight' request for ignored URI '%s'", uri));
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        if (!exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        Document doc = documents{uri};
        *list<auto> references = doc.findReferences(request.params.position, True);
        if (!references) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        # DocumentHighlightKind: Text=1, Read=2, Write=3
        list<auto> highlights = ();
        foreach hash<auto> ref in (references) {
            highlights += {"range": ref.range, "kind": 1};
        }
        return make_jsonrpc_response(jsonRpcVer, request.id, highlights);
    }

    #! "textDocument/signatureHelp" method handler
    /** Provides signature help for function/method calls.
        @param request LSP request with params: textDocument, position
        @return JSON-RPC response with SignatureHelp or empty signatures
    */
    private *string meth_td_signatureHelp(hash<auto> request) {
        # validate request
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::textDocumentPositionParams(request, request.params);
        if (validation) {
            return validation;
        }

        string uri = request.params.textDocument.uri;

        if (ignoredUris{uri}) {
            log(2, sprintf("'textDocument/signatureHelp' request for ignored URI '%s'", uri));
            return make_jsonrpc_response(jsonRpcVer, request.id, {
                "signatures": (),
                "activeSignature": 0,
                "activeParameter": 0,
            });
        }

        if (!exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, {
                "signatures": (),
                "activeSignature": 0,
                "activeParameter": 0,
            });
        }

        Document doc = documents{uri};
        int line = request.params.position.line;
        int col = request.params.position.character;

        *string lineText = doc.getLine(line);
        if (!lineText) {
            return make_jsonrpc_response(jsonRpcVer, request.id, {
                "signatures": (),
                "activeSignature": 0,
                "activeParameter": 0,
            });
        }

        string prefix = col <= lineText.length() ? lineText.substr(0, col) : lineText;

        # Walk backwards to find the unbalanced ( and count commas
        int depth = 0;
        int commaCount = 0;
        int parenPos = -1;
        for (int i = prefix.length() - 1; i >= 0; i--) {
            string ch = prefix[i];
            if (ch == ")") {
                depth++;
            } else if (ch == "(") {
                if (depth == 0) {
                    parenPos = i;
                    break;
                }
                depth--;
            } else if (ch == "," && depth == 0) {
                commaCount++;
            }
        }

        if (parenPos < 0) {
            return make_jsonrpc_response(jsonRpcVer, request.id, {
                "signatures": (),
                "activeSignature": 0,
                "activeParameter": 0,
            });
        }

        # Extract the callable name before the (
        string beforeParen = prefix.substr(0, parenPos);
        beforeParen =~ s/\s+$//;  # trim trailing whitespace
        if (beforeParen.empty()) {
            return make_jsonrpc_response(jsonRpcVer, request.id, {
                "signatures": (),
                "activeSignature": 0,
                "activeParameter": 0,
            });
        }

        # Extract the function/method name token
        # Handle: name(, obj.method(, Ns::func(
        # Use extraction operator (=~ x//) to safely capture groups
        string funcName;
        *string target;
        bool isDotAccess = False;
        bool isScopeAccess = False;

        *list<auto> dotMatch = (beforeParen =~ x/\.(\w+)$/);
        *list<auto> scopeMatch = dotMatch ? NOTHING : (beforeParen =~ x/(\w+)::(\w+)$/);
        *list<auto> plainMatch = !dotMatch && !scopeMatch ? (beforeParen =~ x/(\w+)$/) : NOTHING;

        if (dotMatch) {
            funcName = dotMatch[0];
            isDotAccess = True;
            # Extract the target (before the dot)
            int dotPos = beforeParen.rfind(".");
            string beforeDot = beforeParen.substr(0, dotPos);
            beforeDot =~ s/\s+$//;
            *list<auto> targetMatch = (beforeDot =~ x/(\w+)$/);
            if (targetMatch) {
                target = targetMatch[0];
            }
        } else if (scopeMatch) {
            target = scopeMatch[0];
            funcName = scopeMatch[1];
            isScopeAccess = True;
        } else if (plainMatch) {
            funcName = plainMatch[0];
        } else {
            return make_jsonrpc_response(jsonRpcVer, request.id, {
                "signatures": (),
                "activeSignature": 0,
                "activeParameter": 0,
            });
        }

        string argumentPrefix = prefix.substr(parenPos + 1);
        int minimumParamCount = commaCount + (argumentPrefix =~ /\S/ ? 1 : 0);

        # Look up the callable
        *list<hash<auto>> params;
        *string returnType;

        if (isDotAccess && target) {
            # Look up the variable in scope symbols to find its declared type
            *string typeName;
            *list<auto> scopeSymbols = doc.findScopeSymbolsDetailed({
                "line": line,
                "character": col,
            });
            if (scopeSymbols) {
                foreach hash<auto> sym in (scopeSymbols) {
                    if (sym.name == target && sym.typeName) {
                        typeName = sym.typeName;
                        break;
                    }
                }
            }
            if (!typeName) {
                # Fall back to the target name itself as a type name
                typeName = target;
            }
            *list<auto> members = doc.findTypeMembers(typeName, True);
            if (members) {
                foreach hash<auto> sym in (members) {
                    if (sym.name == funcName
                            && (sym.kind == ASTSymbolKind.Method
                                || sym.kind == ASTSymbolKind.Function
                                || sym.kind == ASTSymbolKind.Constructor)
                            && (sym.params ? sym.params.size() : 0) >= minimumParamCount) {
                        params = sym.params ?? ();
                        returnType = sym.returnType;
                        break;
                    }
                }
            }
        } else if (isScopeAccess && target) {
            *list<auto> members = doc.findTypeMembers(target, True);
            if (members) {
                foreach hash<auto> sym in (members) {
                    if (sym.name == funcName
                            && (sym.kind == ASTSymbolKind.Method
                                || sym.kind == ASTSymbolKind.Function
                                || sym.kind == ASTSymbolKind.Constructor)
                            && (sym.params ? sym.params.size() : 0) >= minimumParamCount) {
                        params = sym.params ?? ();
                        returnType = sym.returnType;
                        break;
                    }
                }
            }
        } else {
            # Plain function name — search scope symbols
            *list<auto> symbols = doc.findScopeSymbolsDetailed(request.params.position);
            if (symbols) {
                foreach hash<auto> sym in (symbols) {
                    if (sym.name == funcName
                            && (sym.kind == ASTSymbolKind.Method
                                || sym.kind == ASTSymbolKind.Function
                                || sym.kind == ASTSymbolKind.Constructor)
                            && (sym.params ? sym.params.size() : 0) >= minimumParamCount) {
                        params = sym.params ?? ();
                        returnType = sym.returnType;
                        break;
                    }
                }
            }
        }

        # metadata fallback for builtins
        *string methodDoc;
        if (!exists params && builtinMetadata) {
            *hash<auto> metaInfo;
            if (isDotAccess && target) {
                # resolve type from scope symbols
                *string typeName;
                *list<auto> scopeSymbols2 = doc.findScopeSymbolsDetailed({
                    "line": line,
                    "character": col,
                });
                if (scopeSymbols2) {
                    foreach hash<auto> sym in (scopeSymbols2) {
                        if (sym.name == target && sym.typeName) {
                            typeName = sym.typeName;
                            break;
                        }
                    }
                }
                if (!typeName) {
                    typeName = target;
                }
                metaInfo = findMethodInMetadata(typeName, funcName, minimumParamCount);
            } else if (isScopeAccess && target) {
                metaInfo = findMethodInMetadata(target, funcName, minimumParamCount);
            } else {
                metaInfo = findFunctionInMetadata(funcName, minimumParamCount);
            }

            if (metaInfo) {
                methodDoc = metaInfo.description;
                returnType = metaInfo.return_type;
                # build params from metadata
                params = ();
                if (metaInfo.params) {
                    foreach hash<auto> mp in (metaInfo.params) {
                        params += {
                            "typeName": mp.type_name ?? "auto",
                            "name": mp.name,
                            "defaultVal": mp.default_value,
                            "description": mp.description,
                        };
                    }
                }
            }
        }

        if (!exists params) {
            return make_jsonrpc_response(jsonRpcVer, request.id, {
                "signatures": (),
                "activeSignature": 0,
                "activeParameter": 0,
            });
        }

        # Build SignatureInformation
        list<string> paramLabels = ();
        list<hash<auto>> paramInfos = ();
        foreach hash<auto> p in (params) {
            string plabel = p.typeName ? (p.typeName + " " + p.name) : p.name;
            if (p.defaultVal) {
                plabel += " = " + p.defaultVal;
            }
            paramLabels += plabel;
            if (p.description) {
                paramInfos += {
                    "label": plabel,
                    "documentation": {
                        "kind": "markdown",
                        "value": string(p.description),
                    },
                };
            } else {
                paramInfos += {"label": plabel};
            }
        }

        string sigLabel = funcName + "(" + paramLabels.join(", ") + ")";
        if (returnType) {
            sigLabel += " returns " + returnType;
        }

        hash<auto> signature = {
            "label": sigLabel,
            "parameters": paramInfos,
        };
        if (methodDoc) {
            signature.documentation = {
                "kind": "markdown",
                "value": string(methodDoc),
            };
        }

        return make_jsonrpc_response(jsonRpcVer, request.id, {
            "signatures": (signature,),
            "activeSignature": 0,
            "activeParameter": paramInfos.size()
                ? (commaCount < paramInfos.size() ? commaCount : paramInfos.size() - 1)
                : 0,
        });
    }

    #! Find a method in builtin metadata by class name and method name
    private *hash<auto> findMethodInMetadata(string className, string methodName,
            int minimumParamCount = 0) {
        *hash<auto> ci = metaClassByName{className};
        if (!ci) {
            return NOTHING;
        }
        # search instance methods first, then static
        foreach hash<auto> m in (ci.instance_methods) {
            if (m.name == methodName) {
                if ((m.params ? m.params.size() : 0) >= minimumParamCount) {
                    return m;
                }
            }
        }
        foreach hash<auto> m in (ci.static_methods) {
            if (m.name == methodName) {
                if ((m.params ? m.params.size() : 0) >= minimumParamCount) {
                    return m;
                }
            }
        }
        return NOTHING;
    }

    #! Find a function in builtin metadata by name
    private *hash<auto> findFunctionInMetadata(string funcName, int minimumParamCount = 0) {
        *list<hash<auto>> entries = metaByName{funcName};
        if (!entries) {
            return NOTHING;
        }
        foreach hash<auto> entry in (entries) {
            if (entry.symbol_type == "function") {
                if ((entry.params ? entry.params.size() : 0) >= minimumParamCount) {
                    return entry;
                }
            }
        }
        return NOTHING;
    }

    #! "textDocument/foldingRange" method handler
    /** Provides folding ranges for the document.
        @param request LSP request with params: textDocument
        @return JSON-RPC response with FoldingRange[]
    */
    private *string meth_td_foldingRange(hash<auto> request) {
        # validate request
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::textDocumentIdentifier(request, request.params);
        if (validation) {
            return validation;
        }

        string uri = request.params.textDocument.uri;

        if (ignoredUris{uri}) {
            log(2, sprintf("'textDocument/foldingRange' request for ignored URI '%s'", uri));
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        if (!exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        Document doc = documents{uri};
        *list<auto> ranges = doc.foldingRanges();
        return make_jsonrpc_response(jsonRpcVer, request.id, ranges ?? ());
    }

    #! "textDocument/selectionRange" method handler
    /** Provides smart selection ranges using AST node ancestry.
        @param request LSP request with params: textDocument, positions
        @return JSON-RPC response with SelectionRange[] (nested parent chain)
    */
    private *string meth_td_selectionRange(hash<auto> request) {
        # validate request
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::selectionRangeParams(request, request.params);
        if (validation) {
            return validation;
        }

        string uri = request.params.textDocument.uri;

        if (ignoredUris{uri}) {
            log(2, sprintf("'textDocument/selectionRange' request for ignored URI '%s'", uri));
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        if (!exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        Document doc = documents{uri};

        list<auto> result = ();
        foreach hash<auto> pos in (request.params.positions) {
            *list<auto> ancestors = doc.findNodeAndParents(pos);
            if (!ancestors) {
                # No node found — return a zero-width range at the requested position
                result += {
                    "range": {
                        "start": {"line": pos.line, "character": pos.character},
                        "end": {"line": pos.line, "character": pos.character},
                    },
                };
                continue;
            }

            # Build the nested SelectionRange chain from outermost to innermost
            # ancestors[0] is innermost, ancestors[last] is root
            *hash<auto> selRange;
            for (int i = ancestors.size() - 1; i >= 0; i--) {
                selRange = {
                    "range": ancestors[i].range,
                    "parent": selRange,
                };
            }
            result += selRange;
        }

        return make_jsonrpc_response(jsonRpcVer, request.id, result);
    }

    #! Set of primitive type names that have no navigable type declaration
    const PrimitiveTypes = {
        "int": True, "float": True, "number": True, "string": True,
        "bool": True, "boolean": True, "binary": True, "date": True,
        "hash": True, "list": True, "auto": True, "any": True,
        "nothing": True, "null": True, "softint": True, "softfloat": True,
        "softnumber": True, "softstring": True, "softbool": True,
        "softdate": True, "softlist": True, "timeout": True, "code": True,
        "data": True, "reference": True, "object": True, "void": True,
    };

    #! "textDocument/typeDefinition" method handler
    /** Jumps from a variable/parameter to its type's declaration.
        @param request LSP request with params: textDocument, position
        @return JSON-RPC response with Location[] or empty
    */
    private *string meth_td_typeDefinition(hash<auto> request) {
        # validate request
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::textDocumentPositionParams(request, request.params);
        if (validation) {
            return validation;
        }

        string uri = request.params.textDocument.uri;

        if (ignoredUris{uri}) {
            log(2, sprintf("'textDocument/typeDefinition' request for ignored URI '%s'", uri));
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        if (!exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        Document doc = documents{uri};

        # Get the type of the symbol at position
        *string typeName = doc.getSymbolType(request.params.position);
        if (!typeName || PrimitiveTypes{typeName}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        # Strip any leading * (nullable prefix)
        if (typeName[0] == "*") {
            typeName = typeName.substr(1);
        }
        # Strip any generic params (e.g., "hash<string>" → "hash")
        int angleBracket = typeName.find("<");
        if (angleBracket >= 0) {
            typeName = typeName.substr(0, angleBracket);
        }

        # Check again after stripping
        if (PrimitiveTypes{typeName}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        # Search for type declaration across already parsed documents first.
        list<auto> symbols = findWorkspaceSymbols(typeName, True);

        # Filter to declaration types only (class, hashdecl, typedef, enum)
        list<auto> result = ();
        foreach hash<auto> sym in (symbols) {
            if (sym.kind == ASTSymbolKind.Class
                    || sym.kind == ASTSymbolKind.Interface  # hashdecl
                    || sym.kind == ASTSymbolKind.Enum
                    || sym.kind == ASTSymbolKind.Namespace) {
                result += sym.location;
            }
        }

        if (!result.empty()) {
            return make_jsonrpc_response(jsonRpcVer, request.id, result);
        }

        symbols = findStdModuleSymbols(typeName, "typeDefinition: " + typeName);
        foreach hash<auto> sym in (symbols) {
            if (sym.kind == ASTSymbolKind.Class
                    || sym.kind == ASTSymbolKind.Interface
                    || sym.kind == ASTSymbolKind.Enum
                    || sym.kind == ASTSymbolKind.Namespace) {
                result += sym.location;
            }
        }

        return make_jsonrpc_response(jsonRpcVer, request.id, result);
    }

    #! LSP SymbolKind constants
    const LspSymbolKind = {
        "Class": 5,
        "Method": 6,
        "Property": 7,
        "Function": 12,
        "Variable": 13,
        "Constant": 14,
        "Namespace": 3,
        "Enum": 10,
        "Interface": 11,
        "Constructor": 9,
    };

    #! "textDocument/codeLens" method handler
    /** Provides code lens items showing reference counts above declarations.
        @param request LSP request with params: textDocument
        @return JSON-RPC response with CodeLens[]
    */
    private *string meth_td_codeLens(hash<auto> request) {
        # validate request
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::textDocumentIdentifier(request, request.params);
        if (validation) {
            return validation;
        }

        string uri = request.params.textDocument.uri;

        if (ignoredUris{uri}) {
            log(2, sprintf("'textDocument/codeLens' request for ignored URI '%s'", uri));
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        if (!exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        Document doc = documents{uri};
        *list<auto> symbols = doc.findSymbols(True, True);
        if (!symbols) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        # Filter to top-level declarations
        list<auto> result = ();
        foreach hash<auto> sym in (symbols) {
            if (sym.kind == ASTSymbolKind.Class
                    || sym.kind == ASTSymbolKind.Function
                    || sym.kind == ASTSymbolKind.Constant
                    || sym.kind == ASTSymbolKind.Namespace
                    || sym.kind == ASTSymbolKind.Interface
                    || sym.kind == ASTSymbolKind.Enum) {
                result += {
                    "range": sym.location.range,
                    "data": {
                        "uri": uri,
                        "name": sym.name,
                        "kind": sym.kind,
                        # the references of the symbol are found from its name
                        "position": doc.getNamePosition(sym.name, sym.location.range),
                    },
                };
            }
        }

        return make_jsonrpc_response(jsonRpcVer, request.id, result);
    }

    #! "codeLens/resolve" method handler
    /** Resolves a code lens item by counting references.
        @param request LSP request with params: the CodeLens to resolve
        @return JSON-RPC response with resolved CodeLens
    */
    private *string meth_codeLens_resolve(hash<auto> request) {
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }

        hash<auto> lens = request.params;
        if (!lens.data || !lens.data.uri) {
            return make_jsonrpc_response(jsonRpcVer, request.id, lens);
        }

        string uri = lens.data.uri;
        if (!exists documents{uri}) {
            lens.command = {"title": "0 references", "command": ""};
            return make_jsonrpc_response(jsonRpcVer, request.id, lens);
        }

        # count the references like textDocument/references, so the title matches the references shown
        Document doc = documents{uri};
        *list<auto> refs = findReferencesCrossDoc(doc, lens.data.position, False)
            ?? doc.findReferences(lens.data.position, False);
        int refCount = refs ? refs.size() : 0;

        lens.command = {
            "title": sprintf("%d reference%s", refCount, refCount == 1 ? "" : "s"),
            "command": "qls.showReferences",
            "arguments": (uri, lens.data.position),
        };

        return make_jsonrpc_response(jsonRpcVer, request.id, lens);
    }

    #! "textDocument/prepareTypeHierarchy" method handler
    /** Finds the class at the given position and returns a TypeHierarchyItem.
        @param request LSP request with params: textDocument, position
        @return JSON-RPC response with TypeHierarchyItem[] or null
    */
    private *string meth_td_prepareTypeHierarchy(hash<auto> request) {
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::textDocumentPositionParams(request, request.params);
        if (validation) {
            return validation;
        }

        string uri = request.params.textDocument.uri;

        if (ignoredUris{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, NOTHING);
        }

        if (!exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, NOTHING);
        }

        Document doc = documents{uri};
        *hash<auto> symbolInfo = doc.findSymbolInfo(request.params.position);
        if (!symbolInfo || symbolInfo.kind != ASTSymbolKind.Class) {
            return make_jsonrpc_response(jsonRpcVer, request.id, NOTHING);
        }

        list<auto> result = ({
            "name": symbolInfo.name,
            "kind": LspSymbolKind.Class,
            "uri": uri,
            "range": symbolInfo.range,
            "selectionRange": symbolInfo.range,
        },);

        return make_jsonrpc_response(jsonRpcVer, request.id, result);
    }

    #! "typeHierarchy/supertypes" method handler
    /** Finds the parent classes of the given TypeHierarchyItem.
        @param request LSP request with params: TypeHierarchyItem
        @return JSON-RPC response with TypeHierarchyItem[]
    */
    private *string meth_typeHierarchy_supertypes(hash<auto> request) {
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }

        hash<auto> item = request.params.item ?? request.params;

        if (!item.name || !item.uri || !item.range || !item.range.start) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        string uri = item.uri;
        if (!exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        Document doc = documents{uri};
        *list<string> superNames = doc.getSuperclassNames(item.range.start);
        if (!superNames) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        # Resolve each superclass to its declaration
        list<auto> result = ();
        foreach string parentName in (superNames) {
            # Search parsed/open docs first; defer standard module parsing until needed.
            list<auto> symbols = findWorkspaceSymbols(parentName, True);

            bool found = False;
            foreach hash<auto> sym in (symbols) {
                if (sym.kind == ASTSymbolKind.Class) {
                    result += {
                        "name": sym.name,
                        "kind": LspSymbolKind.Class,
                        "uri": sym.location.uri,
                        "range": sym.location.range,
                        "selectionRange": sym.location.range,
                    };
                    found = True;
                    break;  # take first match
                }
            }
            if (found) {
                continue;
            }

            symbols = findStdModuleSymbols(parentName, "typeHierarchy/supertypes: " + parentName);
            foreach hash<auto> sym in (symbols) {
                if (sym.kind == ASTSymbolKind.Class) {
                    result += {
                        "name": sym.name,
                        "kind": LspSymbolKind.Class,
                        "uri": sym.location.uri,
                        "range": sym.location.range,
                        "selectionRange": sym.location.range,
                    };
                    break;  # take first match
                }
            }
        }

        return make_jsonrpc_response(jsonRpcVer, request.id, result);
    }

    #! "typeHierarchy/subtypes" method handler
    /** Finds classes that extend the given TypeHierarchyItem.
        @param request LSP request with params: TypeHierarchyItem
        @return JSON-RPC response with TypeHierarchyItem[]
    */
    private *string meth_typeHierarchy_subtypes(hash<auto> request) {
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }

        hash<auto> item = request.params.item ?? request.params;

        if (!item.name) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        string targetName = item.name;

        # Scan all documents for classes whose superclass list contains the target
        list<auto> result = ();

        code<nothing(FileIndex)> scanIndex = sub (FileIndex index) {
            checkCancelled();
            foreach hash<IndexedClass> cls in (index.getClasses()) {
                # Check if this class inherits from targetName
                if (cls.parents.contains(targetName)) {
                    hash<auto> range = index.getSymbol(cls.symbol).location.range;
                    result += {
                        "name": cls.name,
                        "kind": LspSymbolKind.Class,
                        "uri": index.getUri(),
                        "range": range,
                        "selectionRange": range,
                    };
                }
            }
        };

        map scanIndex($1), getAllIndexes(False);
        if (result.empty()) {
            ensureStdModuleIndex("typeHierarchy/subtypes: " + targetName);
            map scanIndex($1), stdModuleIndex.iterator();
        }

        return make_jsonrpc_response(jsonRpcVer, request.id, result);
    }

    #! Node types that should not be renamed
    const NonRenameableTypes = {
        "parse_directive": True,
        "string_literal": True,
        "number_literal": True,
        "regex_literal": True,
        "line_comment": True,
        "block_comment": True,
    };

    #! "textDocument/prepareRename" method handler
    /** Checks if the symbol at position can be renamed and returns its range.
        @param request LSP request with params: textDocument, position
        @return JSON-RPC response with {range, placeholder} or null
    */
    private *string meth_td_prepareRename(hash<auto> request) {
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::textDocumentPositionParams(request, request.params);
        if (validation) {
            return validation;
        }

        string uri = request.params.textDocument.uri;

        if (ignoredUris{uri} || !exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, NOTHING);
        }

        Document doc = documents{uri};
        *hash<auto> symbolInfo = doc.findSymbolInfo(request.params.position);
        if (!symbolInfo || !symbolInfo.name) {
            return make_jsonrpc_response(jsonRpcVer, request.id, NOTHING);
        }

        # Reject rename on non-renameable constructs (strings, comments, etc.)
        *list<auto> nodes = doc.getNodesInfo();
        if (nodes) {
            *string nodeType = findNodeTypeAtPosition(nodes, request.params.position);
            if (nodeType && NonRenameableTypes{nodeType}) {
                return make_jsonrpc_response(jsonRpcVer, request.id, NOTHING);
            }
        }

        return make_jsonrpc_response(jsonRpcVer, request.id, {
            "range": symbolInfo.range,
            "placeholder": symbolInfo.name,
        });
    }

    #! "textDocument/rename" method handler
    /** Renames a symbol across all open documents.
        @param request LSP request with params: textDocument, position, newName
        @return JSON-RPC response with WorkspaceEdit
    */
    private *string meth_td_rename(hash<auto> request) {
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::renameParams(request, request.params);
        if (validation) {
            return validation;
        }

        string uri = request.params.textDocument.uri;
        string newName = request.params.newName;

        if (ignoredUris{uri} || !exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, NOTHING);
        }

        Document doc = documents{uri};

        # Get all references (include declaration) via cross-doc search
        *list<auto> refs = findReferencesCrossDoc(doc, request.params.position, True);
        if (!refs) {
            # Fall back to single-doc references
            refs = doc.findReferences(request.params.position, True);
        }

        if (!refs) {
            return make_jsonrpc_response(jsonRpcVer, request.id, NOTHING);
        }

        # Group edits by URI
        hash<auto> changes;
        foreach hash<auto> ref in (refs) {
            string refUri = ref.uri;
            if (!changes{refUri}) {
                changes{refUri} = ();
            }
            push changes{refUri}, {
                "range": ref.range,
                "newText": newName,
            };
        }

        return make_jsonrpc_response(jsonRpcVer, request.id, {
            "changes": changes,
        });
    }

    #! "textDocument/implementation" method handler
    /** For abstract methods, finds concrete implementations in subclasses.
        For classes, finds subclasses. For concrete methods, returns the definition itself.
        @param request LSP request with params: textDocument, position
        @return JSON-RPC response with Location[]
    */
    private *string meth_td_implementation(hash<auto> request) {
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::textDocumentPositionParams(request, request.params);
        if (validation) {
            return validation;
        }

        string uri = request.params.textDocument.uri;

        if (ignoredUris{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        if (!exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        Document doc = documents{uri};
        *hash<auto> symbolInfo = doc.findSymbolInfo(request.params.position);
        if (!symbolInfo) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        list<auto> result = ();

        if (symbolInfo.kind == ASTSymbolKind.Class) {
            # Find all subtypes
            foreach FileIndex index in (getAllIndexes()) {
                checkCancelled();
                foreach hash<IndexedClass> cls in (index.getClasses()) {
                    if (cls.parents.contains(symbolInfo.name)) {
                        result += index.getSymbol(cls.symbol).location;
                    }
                }
            }
        } else if (symbolInfo.kind == ASTSymbolKind.Method
                || symbolInfo.kind == ASTSymbolKind.Function) {
            # For methods: find implementations in subclasses
            # First, find the enclosing class
            *string enclosingClass;
            *list<auto> scopeSyms = doc.findScopeSymbolsDetailed(request.params.position);
            if (scopeSyms) {
                foreach hash<auto> sym in (scopeSyms) {
                    if (sym.kind == ASTSymbolKind.Class) {
                        enclosingClass = sym.name;
                        break;
                    }
                }
            }

            if (enclosingClass) {
                # Find subclasses and check if they have a method with the same name
                string methodName = symbolInfo.name;

                foreach FileIndex index in (getAllIndexes()) {
                    checkCancelled();
                    *Document d;
                    foreach hash<IndexedClass> cls in (index.getClasses()) {
                        if (!cls.parents.contains(enclosingClass)) {
                            continue;
                        }
                        # only subclasses need the syntax tree
                        if (!d) {
                            d = getAnyDocument(index.getUri());
                            if (!d) {
                                break;
                            }
                        }
                        # Check if this class has the method
                        *list<auto> members = d.findTypeMembers(cls.name, False);
                        if (members) {
                            foreach hash<auto> member in (members) {
                                if (member.name == methodName
                                        && (member.kind == ASTSymbolKind.Method
                                            || member.kind == ASTSymbolKind.Function)) {
                                    result += member.location;
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            # If no implementations found and this isn't abstract, return the definition itself
            if (result.empty()) {
                *list<auto> defs = doc.resolveDefinition(request.params.position);
                if (defs) {
                    result = defs;
                }
            }
        } else {
            # For other symbols, return the definition
            *list<auto> defs = doc.resolveDefinition(request.params.position);
            if (defs) {
                result = defs;
            }
        }

        return make_jsonrpc_response(jsonRpcVer, request.id, result);
    }

    #! "textDocument/inlayHint" method handler
    /** Shows parameter name hints at function/method call sites.
        @param request LSP request with params: textDocument, range
        @return JSON-RPC response with InlayHint[]
    */
    private *string meth_td_inlayHint(hash<auto> request) {
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::textDocumentIdentifier(request, request.params);
        if (validation) {
            return validation;
        }

        string uri = request.params.textDocument.uri;

        if (!request.params.range || !request.params.range.start || !request.params.range.end) {
            return ErrorResponse::invalidParams(request,
                "required attribute 'range' with 'start' and 'end' is missing");
        }

        if (ignoredUris{uri} || !exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        Document doc = documents{uri};

        # Get the requested range
        int startLine = request.params.range.start.line;
        int endLine = request.params.range.end.line;

        # Walk the CST to find call_expression and new_expression nodes in range
        *list<auto> nodes = doc.getNodesInfo();
        if (!nodes) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        list<auto> hints = ();
        findCallHints(doc, nodes, startLine, endLine, \hints);

        return make_jsonrpc_response(jsonRpcVer, request.id, hints);
    }

    #! Recursively find call expressions and generate inlay hints
    private findCallHints(Document doc, list<auto> nodes, int startLine, int endLine,
            reference<list<auto>> hints) {
        foreach hash<auto> node in (nodes) {
            # Skip nodes outside the range
            if (node.loc.end_line < startLine || node.loc.start_line > endLine) {
                continue;
            }

            if (node.type == "call_expression" || node.type == "new_expression") {
                addHintsForCall(doc, node, \hints);
            }

            # Recurse into children
            if (node.children) {
                findCallHints(doc, node.children, startLine, endLine, \hints);
            }
        }
    }

    #! Add parameter name hints for a call expression
    private addHintsForCall(Document doc, hash<auto> callNode,
            reference<list<auto>> hints) {
        # Find the argument list
        *list<auto> children = callNode.children;
        if (!children) {
            return;
        }

        # Find the function/method name and argument_list
        *string funcName;
        *hash<auto> argListNode;
        *string targetType;

        foreach hash<auto> child in (children) {
            if (child.type == "argument_list") {
                argListNode = child;
            } else if (child.type == "identifier") {
                funcName = child.text;
            } else if (child.type == "scoped_identifier") {
                funcName = child.text;
            } else if (child.type == "member_expression") {
                # obj.method — get the method name
                if (child.children) {
                    foreach hash<auto> mc in (child.children) {
                        if (mc.type == "identifier") {
                            funcName = mc.text;
                        }
                    }
                }
            } else if (child.type == "simple_type" || child.type == "scoped_type") {
                # new ClassName(...) — the class name
                funcName = "constructor";
                targetType = child.text;
            }
        }

        if (!argListNode || !argListNode.children) {
            return;
        }

        # Collect actual argument nodes (skip commas and parens)
        list<hash<auto>> args = ();
        foreach hash<auto> argChild in (argListNode.children) {
            if (argChild.type != "," && argChild.type != "(" && argChild.type != ")") {
                args += argChild;
            }
        }

        # Skip single-argument calls (too noisy)
        if (args.size() <= 1) {
            return;
        }

        # Look up the function/method to get parameter names
        *list<hash<auto>> params;

        if (targetType) {
            # Constructor call
            *list<auto> members = doc.findTypeMembers(targetType, True);
            if (members) {
                foreach hash<auto> sym in (members) {
                    if (sym.name == "constructor" && sym.params) {
                        params = sym.params;
                        break;
                    }
                }
            }
        } else if (funcName) {
            # Try scope symbols first
            *list<auto> scopeSyms = doc.findScopeSymbolsDetailed({
                "line": callNode.loc.start_line,
                "character": callNode.loc.start_column,
            });
            if (scopeSyms) {
                foreach hash<auto> sym in (scopeSyms) {
                    if (sym.name == funcName && sym.params) {
                        params = sym.params;
                        break;
                    }
                }
            }
        }

        if (!params) {
            return;
        }

        # Add hints for each argument that we have a param name for
        for (int i = 0; i < args.size() && i < params.size(); i++) {
            string paramName = params[i].name;
            # Skip if the argument text already matches the param name
            if (args[i].text == paramName) {
                continue;
            }
            hints += {
                "position": {
                    "line": args[i].loc.start_line,
                    "character": args[i].loc.start_column,
                },
                "label": paramName + ":",
                "kind": 2,  # InlayHintKind.Parameter
                "paddingRight": True,
            };
        }
    }

    #! "textDocument/prepareCallHierarchy" method handler
    /** Validates the position is on a callable and returns a CallHierarchyItem.
        @param request LSP request with params: textDocument, position
        @return JSON-RPC response with CallHierarchyItem[] or null
    */
    private *string meth_td_prepareCallHierarchy(hash<auto> request) {
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::textDocumentPositionParams(request, request.params);
        if (validation) {
            return validation;
        }

        string uri = request.params.textDocument.uri;

        if (ignoredUris{uri} || !exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, NOTHING);
        }

        Document doc = documents{uri};
        *hash<auto> symbolInfo = doc.findSymbolInfo(request.params.position);
        if (!symbolInfo) {
            return make_jsonrpc_response(jsonRpcVer, request.id, NOTHING);
        }

        # Only allow callables
        if (symbolInfo.kind != ASTSymbolKind.Function
                && symbolInfo.kind != ASTSymbolKind.Method
                && symbolInfo.kind != ASTSymbolKind.Constructor) {
            return make_jsonrpc_response(jsonRpcVer, request.id, NOTHING);
        }

        int lspKind;
        switch (symbolInfo.kind) {
            case ASTSymbolKind.Function:
                lspKind = LspSymbolKind.Function;
                break;
            case ASTSymbolKind.Method:
                lspKind = LspSymbolKind.Method;
                break;
            case ASTSymbolKind.Constructor:
                lspKind = LspSymbolKind.Constructor;
                break;
            default:
                lspKind = LspSymbolKind.Function;
                break;
        }

        list<auto> result = ({
            "name": symbolInfo.name,
            "kind": lspKind,
            "uri": uri,
            "range": symbolInfo.range,
            "selectionRange": symbolInfo.range,
        },);

        return make_jsonrpc_response(jsonRpcVer, request.id, result);
    }

    #! "callHierarchy/incomingCalls" method handler
    /** Finds callers of the given function/method.
        @param request LSP request with params: item (CallHierarchyItem)
        @return JSON-RPC response with CallHierarchyIncomingCall[]
    */
    private *string meth_callHierarchy_incomingCalls(hash<auto> request) {
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }

        hash<auto> item = request.params.item ?? request.params;
        if (!item.name || !item.uri || !item.range || !item.range.start) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        string uri = item.uri;
        if (!exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        Document doc = documents{uri};

        # Find all references to this function across docs
        hash<string, Document> searchedDocs = {};
        *list<auto> refs = findReferencesCrossDoc(doc, item.range.start, False, \searchedDocs);
        if (!refs) {
            refs = doc.findReferences(item.range.start, False);
        }
        if (!refs) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        # For each reference, find the enclosing function/method
        # Group by enclosing function
        hash<string, hash<auto>> callers;
        foreach hash<auto> ref in (refs) {
            string refUri = ref.uri;
            *Document refDoc = documents{refUri} ?? searchedDocs{refUri} ?? getIndexedDocument(refUri);
            if (!refDoc) {
                continue;
            }

            # Find the enclosing callable by checking which function's range
            # contains the reference position
            int refLine = ref.range.start.line;
            *list<auto> scopeSyms = refDoc.findScopeSymbolsDetailed(ref.range.start);
            *hash<auto> enclosing;
            if (scopeSyms) {
                foreach hash<auto> sym in (scopeSyms) {
                    if (sym.kind == ASTSymbolKind.Function
                            || sym.kind == ASTSymbolKind.Method
                            || sym.kind == ASTSymbolKind.Constructor) {
                        # Check if this function's range contains the reference
                        hash<auto> r = sym.location.range;
                        if (r.start.line <= refLine && r.end.line >= refLine) {
                            enclosing = sym;
                            break;
                        }
                    }
                }
            }

            if (enclosing) {
                string key = refUri + ":" + enclosing.name + ":" + string(enclosing.location.range.start.line);
                if (!callers{key}) {
                    int callerKind;
                    switch (enclosing.kind) {
                        case ASTSymbolKind.Function: callerKind = LspSymbolKind.Function; break;
                        case ASTSymbolKind.Method: callerKind = LspSymbolKind.Method; break;
                        case ASTSymbolKind.Constructor: callerKind = LspSymbolKind.Constructor; break;
                        default: callerKind = LspSymbolKind.Function; break;
                    }
                    callers{key} = {
                        "from": {
                            "name": enclosing.name,
                            "kind": callerKind,
                            "uri": refUri,
                            "range": enclosing.location.range,
                            "selectionRange": enclosing.location.range,
                        },
                        "fromRanges": (),
                    };
                }
                push callers{key}.fromRanges, ref.range;
            }
        }

        list<auto> result = callers.values();
        return make_jsonrpc_response(jsonRpcVer, request.id, result);
    }

    #! "callHierarchy/outgoingCalls" method handler
    /** Finds functions/methods called from the given function/method.
        @param request LSP request with params: item (CallHierarchyItem)
        @return JSON-RPC response with CallHierarchyOutgoingCall[]
    */
    private *string meth_callHierarchy_outgoingCalls(hash<auto> request) {
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }

        hash<auto> item = request.params.item ?? request.params;
        if (!item.name || !item.uri || !item.range || !item.range.start || !item.range.end) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        string uri = item.uri;
        if (!exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        Document doc = documents{uri};

        # Walk the function body for call_expression nodes
        *list<auto> nodes = doc.getNodesInfo();
        if (!nodes) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        int funcStartLine = item.range.start.line;
        int funcEndLine = item.range.end.line;

        # Find call targets within the function body
        list<hash<auto>> callTargets = ();
        findCallTargets(nodes, funcStartLine, funcEndLine, \callTargets);

        # Resolve each call target to its definition
        hash<string, hash<auto>> outgoing;
        foreach hash<auto> target in (callTargets) {
            # Try to resolve the definition
            *list<auto> defs = doc.resolveDefinition(target.position);
            if (!defs) {
                defs = resolveDefinitionCrossDoc(doc, target.position);
            }

            if (defs && defs.size() > 0) {
                hash<auto> def = defs[0];
                string key = def.uri + ":" + target.name + ":" + string(def.range.start.line);
                if (!outgoing{key}) {
                    # Resolve the target's symbol kind
                    int targetKind = LspSymbolKind.Function;
                    *Document targetDoc = getAnyDocument(def.uri);
                    if (targetDoc) {
                        *hash<auto> targetInfo = targetDoc.findSymbolInfo(def.range.start);
                        if (targetInfo) {
                            switch (targetInfo.kind) {
                                case ASTSymbolKind.Method:
                                    targetKind = LspSymbolKind.Method;
                                    break;
                                case ASTSymbolKind.Constructor:
                                    targetKind = LspSymbolKind.Constructor;
                                    break;
                            }
                        }
                    }
                    outgoing{key} = {
                        "to": {
                            "name": target.name,
                            "kind": targetKind,
                            "uri": def.uri,
                            "range": def.range,
                            "selectionRange": def.range,
                        },
                        "fromRanges": (),
                    };
                }
                push outgoing{key}.fromRanges, target.range;
            }
        }

        list<auto> result = outgoing.values();
        return make_jsonrpc_response(jsonRpcVer, request.id, result);
    }

    #! Find call target names and positions within a function body
    private findCallTargets(list<auto> nodes, int startLine, int endLine,
            reference<list<hash<auto>>> targets) {
        foreach hash<auto> node in (nodes) {
            if (node.loc.end_line < startLine || node.loc.start_line > endLine) {
                continue;
            }

            if (node.type == "call_expression") {
                # Extract the function name from the call
                *string name;
                *hash<auto> nameNode;
                if (node.children) {
                    foreach hash<auto> child in (node.children) {
                        if (child.type == "identifier") {
                            name = child.text;
                            nameNode = child;
                            break;
                        } else if (child.type == "member_expression") {
                            # obj.method — get the method name
                            if (child.children) {
                                foreach hash<auto> mc in (child.children) {
                                    if (mc.type == "identifier") {
                                        name = mc.text;
                                        nameNode = mc;
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
                if (name && nameNode) {
                    targets += {
                        "name": name,
                        "position": {
                            "line": nameNode.loc.start_line,
                            "character": nameNode.loc.start_column,
                        },
                        "range": {
                            "start": {
                                "line": nameNode.loc.start_line,
                                "character": nameNode.loc.start_column,
                            },
                            "end": {
                                "line": nameNode.loc.end_line,
                                "character": nameNode.loc.end_column,
                            },
                        },
                    };
                }
            }

            if (node.children) {
                findCallTargets(node.children, startLine, endLine, \targets);
            }
        }
    }

    #! "textDocument/semanticTokens/full" method handler
    /** Returns all semantic tokens for the entire document.
        @param request LSP request with params: textDocument
        @return JSON-RPC response with {data: [deltaLine, deltaStartChar, length, tokenType, tokenModifiers, ...]}
    */
    private *string meth_td_semanticTokensFull(hash<auto> request) {
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::textDocumentIdentifier(request, request.params);
        if (validation) {
            return validation;
        }

        string uri = request.params.textDocument.uri;

        if (ignoredUris{uri} || !exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, {"data": ()});
        }

        Document doc = documents{uri};
        list<int> data = encodeSemanticTokens(doc.semanticTokens());
        return make_jsonrpc_response(jsonRpcVer, request.id, storeSemanticTokens(uri, data));
    }

    #! Stores the semantic tokens returned for a document, for later delta requests
    /** @return an LSP \c SemanticTokens hash with \c resultId and \c data keys
    */
    private:internal hash<auto> storeSemanticTokens(string uri, list<int> data) {
        hash<auto> result = {"resultId": string(++semanticTokensResultCount), "data": data};
        semanticTokensResults{uri} = result;
        return result;
    }

    #! "textDocument/semanticTokens/full/delta" method handler
    /** Returns the changes to the semantic tokens since the result with the given ID, or all tokens if that result
        is not the last one returned for the document.
    */
    private *string meth_td_semanticTokensDelta(hash<auto> request) {
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::textDocumentIdentifier(request, request.params);
        if (validation) {
            return validation;
        }
        string uri = request.params.textDocument.uri;
        if (ignoredUris{uri} || !exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, {"data": ()});
        }
        list<int> data = encodeSemanticTokens(documents{uri}.semanticTokens());
        *hash<auto> previous = semanticTokensResults{uri};
        if (!previous || previous.resultId != request.params.previousResultId) {
            return make_jsonrpc_response(jsonRpcVer, request.id, storeSemanticTokens(uri, data));
        }
        list<hash<auto>> edits = QLSServer::getSemanticTokensEdits(previous.data, data);
        hash<auto> result = storeSemanticTokens(uri, data);
        return make_jsonrpc_response(jsonRpcVer, request.id, {"resultId": result.resultId, "edits": edits});
    }

    #! Returns the LSP \c SemanticTokensEdit list that changes the old token data to the new data
    /** The data between the common prefix and the common suffix is replaced by one edit.

        @param oldData the old token data
        @param newData the new token data

        @return an empty list if the data is the same, otherwise one edit
    */
    static list<hash<auto>> getSemanticTokensEdits(list<int> oldData, list<int> newData) {
        int prefix = 0;
        while (prefix < oldData.size() && prefix < newData.size() && oldData[prefix] == newData[prefix]) {
            ++prefix;
        }
        if (prefix == oldData.size() && prefix == newData.size()) {
            return ();
        }
        int suffix = 0;
        while (suffix < oldData.size() - prefix && suffix < newData.size() - prefix
                && oldData[oldData.size() - 1 - suffix] == newData[newData.size() - 1 - suffix]) {
            ++suffix;
        }
        int end = newData.size() - suffix;
        return ({
            "start": prefix,
            "deleteCount": oldData.size() - prefix - suffix,
            "data": end > prefix ? newData[prefix..end - 1] : (),
        },);
    }

    #! "textDocument/semanticTokens/range" method handler
    /** Returns semantic tokens for a specified range.
        @param request LSP request with params: textDocument, range
        @return JSON-RPC response with {data: [deltaLine, deltaStartChar, length, tokenType, tokenModifiers, ...]}
    */
    private *string meth_td_semanticTokensRange(hash<auto> request) {
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::textDocumentIdentifier(request, request.params);
        if (validation) {
            return validation;
        }

        string uri = request.params.textDocument.uri;

        if (!request.params.range || !request.params.range.start || !request.params.range.end) {
            return ErrorResponse::invalidParams(request,
                "required attribute 'range' with 'start' and 'end' is missing");
        }

        if (ignoredUris{uri} || !exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, {"data": ()});
        }

        Document doc = documents{uri};

        int startLine = request.params.range.start.line;
        int endLine = request.params.range.end.line;
        *list<auto> tokens = doc.semanticTokens(startLine, endLine);

        list<int> data = encodeSemanticTokens(tokens);
        return make_jsonrpc_response(jsonRpcVer, request.id, {"data": data});
    }

    #! Delta-encode semantic tokens per LSP spec
    /** @param tokens list of {line, startChar, length, tokenType, tokenModifiers} hashes
        @return flat list of integers: [deltaLine, deltaStartChar, length, tokenType, tokenModifiers, ...]
    */
    private list<int> encodeSemanticTokens(*list<auto> tokens) {
        list<int> data = ();
        if (!tokens) {
            return data;
        }

        int prevLine = 0;
        int prevChar = 0;

        foreach hash<auto> t in (tokens) {
            int deltaLine = t.line - prevLine;
            int deltaChar = deltaLine == 0 ? t.startChar - prevChar : t.startChar;

            push data, deltaLine;
            push data, deltaChar;
            push data, t.length;
            push data, t.tokenType;
            push data, t.tokenModifiers;

            prevLine = t.line;
            prevChar = t.startChar;
        }

        return data;
    }

    #! "textDocument/codeAction" method handler
    /** Returns available code actions for the given range/context.
        @param request LSP request with params: textDocument, range, context
        @return JSON-RPC response with CodeAction[]
    */
    private *string meth_td_codeAction(hash<auto> request) {
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::textDocumentIdentifier(request, request.params);
        if (validation) {
            return validation;
        }

        string uri = request.params.textDocument.uri;

        if (ignoredUris{uri} || !exists documents{uri}) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        Document doc = documents{uri};
        *string text = doc.getText();
        if (!exists text) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        list<auto> actions = QLSServer::getQuickFixes(uri, request.params.context.diagnostics);

        # --- Sort imports ---
        *list<auto> sortAction = buildSortImportsAction(uri, text);
        if (sortAction) {
            push actions, sortAction[0];
        }

        # --- Add %modern ---
        *list<auto> modernAction = buildAddModernAction(uri, text);
        if (modernAction) {
            push actions, modernAction[0];
        }

        return make_jsonrpc_response(jsonRpcVer, request.id, actions);
    }

    #! "textDocument/documentLink" method handler
    /** Returns links for the files of the module and include directives of an open document.
    */
    private *string meth_td_documentLink(hash<auto> request) {
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::textDocumentIdentifier(request, request.params);
        if (validation) {
            return validation;
        }
        string uri = request.params.textDocument.uri;
        if (ignoredUris{uri} || !exists documents{uri} || uri !~ /^file:\//) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }
        *string text = documents{uri}.getText();
        return make_jsonrpc_response(jsonRpcVer, request.id,
            text ? DocumentLinks::getLinks(Document::getFilePath(uri), text) : ());
    }

    #! Returns quick fix code actions for the fixes of Qore diagnostics
    /** @param uri the document URI
        @param diagnostics the diagnostics in the code action request; Qore diagnostics have their fixes in
        \c data

        @return the quick fix code actions; machine-applicable fixes are preferred
    */
    static list<hash<auto>> getQuickFixes(string uri, *list<auto> diagnostics) {
        list<hash<auto>> actions = ();
        string path = Document::getFilePath(uri);
        foreach auto diag in (diagnostics) {
            if (diag.typeCode() != NT_HASH || diag.data.fixes.typeCode() != NT_LIST) {
                continue;
            }
            foreach auto fix in (diag.data.fixes) {
                if (fix.typeCode() != NT_HASH || !fix.edits) {
                    continue;
                }
                # only edits in this document with known positions can be applied
                list<hash<auto>> edits = ();
                foreach auto edit in (fix.edits) {
                    if (edit.typeCode() != NT_HASH || (exists edit.file && edit.file != path)
                            || edit.startColumn < 0 || edit.endColumn < 0 || edit.startLine < 1) {
                        edits = ();
                        break;
                    }
                    push edits, {
                        "range": {
                            "start": {"line": edit.startLine - 1, "character": edit.startColumn},
                            "end": {"line": edit.endLine - 1, "character": edit.endColumn},
                        },
                        "newText": edit.replacement,
                    };
                }
                if (!edits) {
                    continue;
                }
                push actions, {
                    "title": fix.title,
                    "kind": "quickfix",
                    "diagnostics": (diag,),
                    "isPreferred": fix.applicability == "machine-applicable",
                    "edit": {"changes": {uri: edits}},
                };
            }
        }
        return actions;
    }

    #! Build a "Sort imports" code action if %requires directives are unsorted
    /** @return single-element list with the CodeAction, or NOTHING if already sorted
    */
    private *list<auto> buildSortImportsAction(string uri, string text) {
        # Extract %requires lines with their positions
        list<string> lines = text.split("\n");
        list<hash<auto>> requiresLines = ();

        for (int i = 0; i < lines.size(); i++) {
            if (lines[i] =~ /^%requires\s/) {
                push requiresLines, {"line": i, "text": lines[i]};
            }
        }

        if (requiresLines.size() < 2) {
            return NOTHING;
        }

        # Check if already sorted (case-insensitive by module name)
        list<string> moduleNames = map $1.text, requiresLines;
        list<string> sorted = sort(moduleNames, int sub (string l, string r) {
            return l.lwr() <=> r.lwr();
        });

        bool alreadySorted = True;
        for (int i = 0; i < moduleNames.size(); i++) {
            if (moduleNames[i] != sorted[i]) {
                alreadySorted = False;
                break;
            }
        }

        if (alreadySorted) {
            return NOTHING;
        }

        # Build text edits to reorder the lines
        list<auto> edits = ();
        for (int i = 0; i < requiresLines.size(); i++) {
            int lineNum = requiresLines[i].line;
            push edits, {
                "range": {
                    "start": {"line": lineNum, "character": 0},
                    "end": {"line": lineNum, "character": lines[lineNum].size()},
                },
                "newText": sorted[i],
            };
        }

        return ({
            "title": "Sort imports",
            "kind": "source.sortImports",
            "edit": {
                "changes": {
                    uri: edits,
                },
            },
        },);
    }

    #! Build an "Add %modern" code action if %modern is missing
    /** @return single-element list with the CodeAction, or NOTHING if already has %modern
    */
    private *list<auto> buildAddModernAction(string uri, string text) {
        # Check if %modern is already present
        if (text =~ /^%modern$/m) {
            return NOTHING;
        }

        # Add %modern at the beginning of the file
        return ({
            "title": "Add %modern directive",
            "kind": "source",
            "edit": {
                "changes": {
                    uri: ({
                        "range": {
                            "start": {"line": 0, "character": 0},
                            "end": {"line": 0, "character": 0},
                        },
                        "newText": "%modern\n",
                    },),
                },
            },
        },);
    }

    #! Find the type of the most specific CST node at the given position
    /** @param nodes list from getNodesInfo()
        @param position {line, character}
        @return node type string, or NOTHING if not found
    */
    private *string findNodeTypeAtPosition(list<auto> nodes, hash<auto> position) {
        int line = position.line;
        int col = position.character;

        foreach hash<auto> node in (nodes) {
            if (node.loc.start_line <= line && node.loc.end_line >= line) {
                # Check children first for more specific match
                if (node.children) {
                    *string childType = findNodeTypeAtPosition(node.children, position);
                    if (childType) {
                        return childType;
                    }
                }
                # Check if position is within this node's column range on its line
                if (node.loc.start_line == node.loc.end_line) {
                    if (col >= node.loc.start_column && col < node.loc.end_column) {
                        return node.type;
                    }
                } else {
                    return node.type;
                }
            }
        }
        return NOTHING;
    }

    #! Returns the parsed document for an indexed file, or @ref nothing if the file is not indexed or cannot be parsed
    private:internal *Document getIndexedDocument(string uri) {
        *FileIndex index = workspaceIndex{uri} ?? stdModuleIndex{uri};
        if (!index) {
            return NOTHING;
        }
        try {
            return treeCache.get(uri, index.getSourceSize());
        } catch (hash<ExceptionInfo> ex) {
            log(0, "WARNING: cannot parse %s: %s", uri, get_exception_string(ex));
        }
        return NOTHING;
    }

    #! Returns the open document or the parsed document of an indexed file for the given URI
    private:internal *Document getAnyDocument(string uri) {
        return documents{uri} ?? getIndexedDocument(uri);
    }

    #! Returns the symbol indexes searched for cross-document results, in search order
    /** Standard module indexes are only included if they have already been loaded.
    */
    private:internal list<hash<string, FileIndex>> getSearchIndexes() {
        list<hash<string, FileIndex>> ret = (workspaceIndex,);
        if (stdModuleIndexLoaded) {
            push ret, stdModuleIndex;
        }
        return ret;
    }

    #! Returns the identifier at the given position of an open document, or @ref nothing if there is none
    private:internal static *string getIdentifierAt(Document doc, hash<auto> position) {
        *string line = doc.getLine(position.line);
        if (!line) {
            return NOTHING;
        }
        int col = position.character;
        code<bool(int)> isWordChar = bool sub (int i) {
            return i >= 0 && i < line.size() && line[i] =~ /^[A-Za-z0-9_]$/;
        };
        if (!isWordChar(col)) {
            # the position may be just after the identifier
            if (!isWordChar(col - 1)) {
                return NOTHING;
            }
            --col;
        }
        int start = col;
        while (isWordChar(start - 1)) {
            --start;
        }
        int end = col;
        while (isWordChar(end + 1)) {
            ++end;
        }
        return line.substr(start, end - start + 1);
    }

    #! Returns the other open documents, keyed by URI
    private:internal hash<string, Document> getOtherOpenDocuments(string excludeUri) {
        hash<string, Document> ret = {};
        foreach hash<auto> i in (documents.pairIterator()) {
            if (i.key != excludeUri) {
                ret{i.key} = i.value;
            }
        }
        return ret;
    }

    #! Returns a hash of URI → AstTree for the given documents
    private:internal static hash<auto> getTrees(hash<string, Document> docs) {
        hash<auto> trees = {};
        foreach hash<auto> i in (docs.pairIterator()) {
            *astparser::AstTree tree = i.value.getTree();
            if (tree) {
                trees{i.key} = tree;
            }
        }
        return trees;
    }

    #! Adds the parsed document of an indexed file to a set of documents
    private:internal addIndexedDocument(reference<hash<string, Document>> docs, FileIndex index) {
        string uri = index.getUri();
        if (exists docs{uri}) {
            return;
        }
        *Document doc = getIndexedDocument(uri);
        if (doc) {
            docs{uri} = doc;
        }
    }

    #! Returns the documents that can contain references to the identifier at the given position
    /** Returns the other open documents and the indexed files whose source contains the identifier; files that
        cannot contain a reference are not parsed.

        @param doc the open document
        @param position the LSP position of the identifier

        @return a hash of URI → Document, in search order
    */
    private:internal hash<string, Document> getReferenceCandidates(Document doc, hash<auto> position) {
        string uri = doc.getUri();
        hash<string, Document> ret = getOtherOpenDocuments(uri);
        *string name = QLSServer::getIdentifierAt(doc, position);
        if (!name) {
            return ret;
        }
        foreach hash<string, FileIndex> indexes in (getSearchIndexes()) {
            foreach FileIndex index in (indexes.iterator()) {
                checkCancelled();
                # the identifier filter excludes most files without reading them
                if (index.getUri() != uri && index.mayContainIdentifier(name) && index.sourceContains(name)) {
                    addIndexedDocument(\ret, index);
                }
            }
        }
        return ret;
    }

    #! Finds references across documents
    /** @param doc the open document
        @param position the LSP position of the identifier
        @param includeDecl whether to include the declaration
        @param searchedDocs receives the documents searched, keyed by URI

        @return list of {uri, range} locations, or @ref nothing
    */
    private:internal *list<auto> findReferencesCrossDoc(Document doc, hash<auto> position, bool includeDecl,
            *reference<hash<string, Document>> searchedDocs) {
        hash<string, Document> docs = getReferenceCandidates(doc, position);
        searchedDocs = docs;
        return doc.findReferencesCrossDoc(position, includeDecl, QLSServer::getTrees(docs));
    }

    #! Number of files that only declare the identifier searched per round of cross-document definition resolution
    const DefinitionBatchSize = 32;

    #! Resolves a definition across documents
    /** The definition resolution in @c astparser looks up declarations by name: the identifier itself, and the
        classes on the inheritance path from a class used in the document to the class that declares a member with
        the identifier's name.  Only the files that declare these classes are parsed for the first search, together
        with the open documents.  The files that declare the identifier are then added in batches; files are always
        searched in the order of the indexes, so the first declaration found is the same as if all files were
        searched, without parsing every file that declares the name.

        @param doc the open document
        @param position the LSP position of the identifier

        @return list of {uri, range} locations, or @ref nothing
    */
    private:internal *list<auto> resolveDefinitionCrossDoc(Document doc, hash<auto> position) {
        string uri = doc.getUri();
        hash<string, Document> base = getOtherOpenDocuments(uri);

        # the names that the search can look up
        hash<string, bool> lookupNames = {};
        *string name = QLSServer::getIdentifierAt(doc, position);
        if (name) {
            lookupNames{name} = True;
        }
        *hash<auto> symbolInfo = doc.findSymbolInfo(position);
        if (symbolInfo.name) {
            lookupNames{symbolInfo.name} = True;
            lookupNames{FileIndex::getLastComponent(symbolInfo.name)} = True;
        }
        if (!lookupNames) {
            return doc.resolveDefinitionCrossDoc(position, QLSServer::getTrees(base));
        }

        list<hash<string, FileIndex>> searchIndexes = getSearchIndexes();

        # the class graph of all open and indexed files
        list<FileIndex> openIndexes = getOpenDocumentIndexes();
        hash<ClassGraph> graph = getIndexedClassGraph();
        map $1.addToClassGraph(\graph), openIndexes;
        hash<string, hash<string, bool>> parents = graph.parents;
        hash<string, hash<string, bool>> children = graph.children;

        # the classes that declare a member with one of the names
        hash<string, bool> memberClasses = {};
        code<nothing(FileIndex)> addMemberClasses = sub (FileIndex index) {
            foreach string lookupName in (keys lookupNames) {
                map memberClasses{$1} = True, index.getClassesWithMember(lookupName);
            }
        };
        map addMemberClasses($1), openIndexes;
        foreach hash<string, FileIndex> indexes in (searchIndexes) {
            map addMemberClasses($1), indexes.iterator();
        }

        # the classes named in the document and their ancestors
        hash<string, bool> ancestors = {};
        list<string> todo = ();
        todo = (select keys doc.getIdentifiers(), exists parents{$1}) ?? ();
        while (todo) {
            string cls = shift todo;
            if (!ancestors{cls}) {
                ancestors{cls} = True;
                todo += keys parents{cls} ?? ();
            }
        }

        # the classes that declare a member with the name, and their descendants
        hash<string, bool> descendants = {};
        todo = keys memberClasses ?? ();
        while (todo) {
            string cls = shift todo;
            if (!descendants{cls}) {
                descendants{cls} = True;
                todo += keys children{cls} ?? ();
            }
        }

        # the classes that can be on the inheritance path
        hash<string, bool> pathClasses = (map {$1: True}, keys ancestors, descendants{$1}) ?? {};

        # the files needed for every search, and the files that only declare a name in search order
        hash<string, bool> pathFiles = {};
        list<string> declaringFiles = ();
        foreach hash<string, FileIndex> indexes in (searchIndexes) {
            foreach FileIndex index in (indexes.iterator()) {
                checkCancelled();
                string indexUri = index.getUri();
                if (indexUri == uri) {
                    continue;
                }
                if (pathClasses && (select index.getClasses(), pathClasses{$1.name})) {
                    pathFiles{indexUri} = True;
                } else if (select keys lookupNames, index.declares($1)) {
                    push declaringFiles, indexUri;
                }
            }
        }

        int next = 0;
        while (True) {
            # search the files in the same order as all files would be searched, so that the first declaration
            # found is the same
            hash<string, bool> searchFiles = pathFiles;
            for (int end = next + DefinitionBatchSize; next < end && next < declaringFiles.size(); ++next) {
                searchFiles{declaringFiles[next]} = True;
            }
            hash<string, Document> docs = base;
            foreach hash<string, FileIndex> indexes in (searchIndexes) {
                foreach FileIndex index in (indexes.iterator()) {
                    checkCancelled();
                    if (searchFiles{index.getUri()}) {
                        addIndexedDocument(\docs, index);
                    }
                }
            }
            *list<auto> defs = doc.resolveDefinitionCrossDoc(position, QLSServer::getTrees(docs));
            if (defs || next >= declaringFiles.size()) {
                return defs;
            }
        }
    }

    #! Returns information about the symbol indexes and the syntax tree cache
    /** Open documents keep their syntax trees; other workspace and standard module files are only kept as symbol
        indexes, and their syntax trees are parsed for requests and kept in a cache of limited size.

        @return the current number of indexed files and symbols and the state of the syntax tree cache

        @par Example
        @code{.py}
hash<IndexInfo> info = server.getIndexInfo();
printf("%d files indexed, %d syntax trees cached\n", info.workspaceFiles, info.cachedTrees);
        @endcode
    */
    hash<IndexInfo> getIndexInfo() {
        int workspaceSymbols = 0;
        map workspaceSymbols += $1.getSymbolCount(), workspaceIndex.iterator();
        return <IndexInfo>{
            "workspaceFiles": workspaceIndex.size(),
            "workspaceSymbols": workspaceSymbols,
            "stdModulesLoaded": stdModuleIndexLoaded,
            "stdModuleFiles": stdModuleIndex.size(),
            "openDocuments": documents.size(),
            "cachedTrees": treeCache.size(),
            "cachedTreeSourceSize": treeCache.getTotalSize(),
            "treeCacheSize": treeCache.getMaxSize(),
            "pendingFiles": pendingWorkspaceFiles.size() + pendingStdFiles.size(),
        };
    }

    #! Returns the class graph of the indexed files that are searched for cross-document results
    private:internal hash<ClassGraph> getIndexedClassGraph() {
        if (!indexedClassGraph) {
            hash<ClassGraph> graph();
            foreach hash<string, FileIndex> indexes in (getSearchIndexes()) {
                map $1.addToClassGraph(\graph), indexes.iterator();
            }
            indexedClassGraph = graph;
        }
        return indexedClassGraph;
    }

    #! Returns a symbol index for every open document
    private:internal list<FileIndex> getOpenDocumentIndexes() {
        return map $1.getIndex(), documents.iterator();
    }

    #! Returns the symbol indexes of all open and indexed files, in search order
    /** @param includeStdModules whether to include the standard module indexes if they are loaded
    */
    private:internal list<FileIndex> getAllIndexes(bool includeStdModules = True) {
        list<FileIndex> ret = getOpenDocumentIndexes();
        ret += workspaceIndex.values();
        if (includeStdModules && stdModuleIndexLoaded) {
            ret += stdModuleIndex.values();
        }
        return ret;
    }

    #! Finds matching symbols in the open documents and the workspace index
    /** @param query search query
        @param exactMatch whether to only find exact matches

        @return list of symbols
    */
    private:internal list<auto> findWorkspaceSymbols(string query, bool exactMatch) {
        list<auto> symbols = ();
        map symbols += $1.getIndex().findMatchingSymbols(query, exactMatch), documents.iterator();
        map symbols += $1.findMatchingSymbols(query, exactMatch), workspaceIndex.iterator();
        return symbols;
    }

    #! Finds matching symbols in the standard module index, loading it if necessary
    /** @param query search query
        @param reason the reason for loading the index, for logging

        @return list of symbols
    */
    private:internal list<auto> findStdModuleSymbols(string query, string reason) {
        ensureStdModuleIndex(reason);
        list<auto> symbols = ();
        map symbols += $1.findMatchingSymbols(query, True), stdModuleIndex.iterator();
        return symbols;
    }

    #! "textDocument/definition" method handler
    private *string meth_td_definition(hash<auto> request) {
        # validate request
        if (!request.hasKey("params")) {
            return ErrorResponse::invalidRequest(request, "required attribute 'params' is missing");
        }
        *string validation = ParamValidator::textDocumentPositionParams(request, request.params);
        if (validation) {
            return validation;
        }

        if (ignoredUris{request.params.textDocument.uri}) {
            log(2, sprintf("'textDocument/definition' request for ignored URI '%s'",
                request.params.textDocument.uri));
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        if (!exists documents{request.params.textDocument.uri}) {
            return ErrorResponse::nonExistentDocument(request);
        }

        # get the document
        Document doc = documents{request.params.textDocument.uri};

        # try single-document resolveDefinition first (handles locals, params, members)
        *list<auto> resolved = doc.resolveDefinition(request.params.position);
        if (resolved) {
            return make_jsonrpc_response(jsonRpcVer, request.id, resolved);
        }

        # try cross-document definition resolution (inheritance, cross-file declarations)
        if (documents.size() > 1 || workspaceIndex || (stdModuleIndexLoaded && stdModuleIndex)) {
            resolved = resolveDefinitionCrossDoc(doc, request.params.position);
            if (resolved) {
                return make_jsonrpc_response(jsonRpcVer, request.id, resolved);
            }
        }

        # fall back to cross-document keyword search
        *hash<auto> symbolInfo = doc.findSymbolInfo(request.params.position);
        if (!symbolInfo) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        # prepare search query
        string query = symbolInfo.name;
        {
            int lastDoubleColon = query.rfind("::");
            if (lastDoubleColon != -1) {
                query = query.substr(lastDoubleColon + 2);
            }
            if (query.equalPartial("*")) {
                query = query.substr(1);
            }
        }
        bool queryCanBeSourceSymbol = query =~ /^[A-Za-z_][A-Za-z0-9_]*$/;

        # find matching symbols in already parsed documents first
        list<auto> symbols = findWorkspaceSymbols(query, True);

        for (int i = symbols.size() - 1; i >= 0; i--) {
            if (SymbolUsageFastMap{string(symbolInfo.usage)}) {
                if (!SymbolUsageFastMap{string(symbolInfo.usage)}.contains(symbols[i].kind)) {
                    splice symbols, i, 1;
                }
            }
        }

        # prepare results
        list<auto> result = map $1.location, symbols;
        if (!result.empty()) {
            return make_jsonrpc_response(jsonRpcVer, request.id, result);
        }

        if (!queryCanBeSourceSymbol) {
            return make_jsonrpc_response(jsonRpcVer, request.id, ());
        }

        symbols = findStdModuleSymbols(query, "definition: " + query);

        for (int i = symbols.size() - 1; i >= 0; i--) {
            if (SymbolUsageFastMap{string(symbolInfo.usage)}) {
                if (!SymbolUsageFastMap{string(symbolInfo.usage)}.contains(symbols[i].kind)) {
                    splice symbols, i, 1;
                }
            }
        }

        result = map $1.location, symbols;
        return make_jsonrpc_response(jsonRpcVer, request.id, result);
    }
}

#! ASTSymbolKind to CompletionItem.kind mapping
const CSTSymbolKindMap = {
    string(ASTSymbolKind.Function): "function",
    string(ASTSymbolKind.Class): "class",
    string(ASTSymbolKind.Method): "method",
    string(ASTSymbolKind.Property): "member",
    string(ASTSymbolKind.Field): "member",
    string(ASTSymbolKind.Variable): "variable",
    string(ASTSymbolKind.Constant): "constant",
    string(ASTSymbolKind.Namespace): "namespace",
    string(ASTSymbolKind.Enum): "enum",
    string(ASTSymbolKind.Constructor): "method",
    string(ASTSymbolKind.Interface): "hashdecl",
    string(ASTSymbolKind.Module): "namespace",
    string(ASTSymbolKind.TypeAlias): "type_parameter",
};

#! Completion provider using CSTSearcher for static analysis
/** This provider resolves symbols from the parsed syntax tree of the current document.
*/
class CSTCompletionProvider inherits QoreCodeCompletion::AbstractCompletionProvider {
    private {
        Document doc;
        hash<auto> position;
        #! Metadata indexes for builtin symbol completion (may be empty)
        hash<string, list<hash<auto>>> metaByName;
        hash<string, hash<auto>> metaClassByName;
        #! Single-char prefix index for fast lookup
        hash<string, list<string>> metaPrefixIndex;
    }

    constructor(Document doc, hash<auto> position,
            *hash<string, list<hash<auto>>> metaByName,
            *hash<string, hash<auto>> metaClassByName,
            *hash<string, list<string>> metaPrefixIndex) {
        self.doc = doc;
        self.position = position;
        self.metaByName = metaByName ?? {};
        self.metaClassByName = metaClassByName ?? {};
        self.metaPrefixIndex = metaPrefixIndex ?? {};
    }

    int getParseOptions() {
        return doc.getParseDirectiveOptions();
    }

    list<hash<QoreCodeCompletion::CompletionItem>> getVisibleSymbols(string prefix) {
        *list<auto> symbols = doc.findScopeSymbolsDetailed(position);

        list<hash<QoreCodeCompletion::CompletionItem>> result = ();
        hash<string, bool> seen;

        if (symbols) {
            foreach hash<auto> sym in (symbols) {
                string name = sym.name;
                if (!seen{name}
                        && QoreCodeCompletion::CompletionEngine::matchesPrefix(name, prefix)) {
                    seen{name} = True;
                    string kind = CSTSymbolKindMap{string(sym.kind)} ?? "variable";
                    bool callable = kind == "function" || kind == "method";
                    result += <QoreCodeCompletion::CompletionItem>{
                        "label": name,
                        "kind": kind,
                        "isCallable": callable,
                    };
                }
            }
        }

        # add builtin symbols from metadata (classes, functions, constants)
        # use the prefix index for fast single-character lookups
        if (metaByName && prefix.length() >= 1) {
            # get candidate names from the prefix index using the first character
            string key = prefix[0].lwr();
            *list<string> candidates = metaPrefixIndex{key};
            foreach string name in (candidates) {
                if (!seen{name}
                        && QoreCodeCompletion::CompletionEngine::matchesPrefix(name, prefix)) {
                    seen{name} = True;
                    # use first entry's symbol_type for kind
                    hash<auto> entry = metaByName{name}[0];
                    string kind;
                    bool callable;
                    switch (entry.symbol_type) {
                        case "class":
                            kind = "class";
                            callable = True;
                            break;
                        case "function":
                            kind = "function";
                            callable = True;
                            break;
                        case "constant":
                            kind = "constant";
                            callable = False;
                            break;
                        case "hashdecl":
                            kind = "hashdecl";
                            callable = False;
                            break;
                        case "enum":
                            kind = "enum";
                            callable = False;
                            break;
                        case "namespace":
                            kind = "namespace";
                            callable = False;
                            break;
                        default:
                            continue;
                    }
                    result += <QoreCodeCompletion::CompletionItem>{
                        "label": name,
                        "kind": kind,
                        "isCallable": callable,
                    };
                }
            }
        }

        return result;
    }

    list<hash<QoreCodeCompletion::CompletionItem>> getClassNames(string prefix) {
        *list<auto> symbols = doc.findScopeSymbolsDetailed(position);

        list<hash<QoreCodeCompletion::CompletionItem>> result = ();
        hash<string, bool> seen;

        if (symbols) {
            foreach hash<auto> sym in (symbols) {
                if (sym.kind == ASTSymbolKind.Class) {
                    string name = sym.name;
                    if (!seen{name}
                            && QoreCodeCompletion::CompletionEngine::matchesPrefix(name, prefix)) {
                        seen{name} = True;
                        result += <QoreCodeCompletion::CompletionItem>{
                            "label": name,
                            "kind": "class",
                            "isCallable": True,
                        };
                    }
                }
            }
        }

        # add builtin classes from metadata
        if (metaClassByName && prefix.length()) {
            HashIterator it(metaClassByName);
            while (it.next()) {
                string name = it.getKey();
                # skip fully qualified names (only add unqualified)
                if (name.find("::") >= 0) {
                    continue;
                }
                if (!seen{name}
                        && QoreCodeCompletion::CompletionEngine::matchesPrefix(name, prefix)) {
                    seen{name} = True;
                    result += <QoreCodeCompletion::CompletionItem>{
                        "label": name,
                        "kind": "class",
                        "isCallable": True,
                    };
                }
            }
        }

        return result;
    }

    list<hash<QoreCodeCompletion::CompletionItem>> getInstanceMembers(string target, string prefix) {
        # Resolve the type of the target variable
        int targetCharacter = position.character - prefix.length() - 2;
        *string typeName = doc.getSymbolType({
            "line": position.line,
            "character": targetCharacter >= 0 ? targetCharacter : 0,
        });
        if (!typeName) {
            # Fall back: try the target name as a type directly
            typeName = target;
        }

        *list<auto> members = doc.findTypeMembers(typeName, True);

        list<hash<QoreCodeCompletion::CompletionItem>> result = ();
        hash<string, bool> seen;

        if (members) {
            foreach hash<auto> sym in (members) {
                string name = sym.name;
                if (name == "constructor" || name == "destructor" || name == "copy") {
                    continue;
                }
                # Skip static members for dot access
                if (sym.isStatic) {
                    continue;
                }
                if (!seen{name}
                        && QoreCodeCompletion::CompletionEngine::matchesPrefix(name, prefix)) {
                    seen{name} = True;
                    string kind = CSTSymbolKindMap{string(sym.kind)} ?? "variable";
                    bool callable = kind == "function" || kind == "method";
                    result += <QoreCodeCompletion::CompletionItem>{
                        "label": name,
                        "kind": kind,
                        "isCallable": callable,
                    };
                }
            }
        }

        # metadata fallback for builtin class members
        if (!result.size() && metaClassByName{typeName}) {
            hash<auto> ci = metaClassByName{typeName};
            foreach hash<auto> m in (ci.instance_methods) {
                string name = m.name;
                if (name == "constructor" || name == "destructor" || name == "copy") {
                    continue;
                }
                if (!seen{name}
                        && QoreCodeCompletion::CompletionEngine::matchesPrefix(name, prefix)) {
                    seen{name} = True;
                    result += <QoreCodeCompletion::CompletionItem>{
                        "label": name,
                        "kind": "method",
                        "isCallable": True,
                    };
                }
            }
            # also add public members
            foreach hash<auto> m in (ci.members) {
                if (m.access != "public") {
                    continue;
                }
                string name = m.name;
                if (!seen{name}
                        && QoreCodeCompletion::CompletionEngine::matchesPrefix(name, prefix)) {
                    seen{name} = True;
                    result += <QoreCodeCompletion::CompletionItem>{
                        "label": name,
                        "kind": "member",
                        "isCallable": False,
                    };
                }
            }
        }

        return result;
    }

    list<hash<QoreCodeCompletion::CompletionItem>> getScopeMembers(string target, string prefix) {
        *list<auto> members = doc.findTypeMembers(target, True);

        list<hash<QoreCodeCompletion::CompletionItem>> result = ();
        hash<string, bool> seen;

        if (members) {
            foreach hash<auto> sym in (members) {
                string name = sym.name;
                if (name == "constructor" || name == "destructor") {
                    continue;
                }
                if (!seen{name}
                        && QoreCodeCompletion::CompletionEngine::matchesPrefix(name, prefix)) {
                    seen{name} = True;
                    string kind = CSTSymbolKindMap{string(sym.kind)} ?? "variable";
                    bool callable = kind == "function" || kind == "method";
                    result += <QoreCodeCompletion::CompletionItem>{
                        "label": name,
                        "kind": kind,
                        "isCallable": callable,
                    };
                }
            }
        }

        # metadata fallback for builtin class static members / namespace members
        if (!result.size() && metaClassByName{target}) {
            hash<auto> ci = metaClassByName{target};
            foreach hash<auto> m in (ci.static_methods) {
                string name = m.name;
                if (!seen{name}
                        && QoreCodeCompletion::CompletionEngine::matchesPrefix(name, prefix)) {
                    seen{name} = True;
                    result += <QoreCodeCompletion::CompletionItem>{
                        "label": name,
                        "kind": "method",
                        "isCallable": True,
                    };
                }
            }
            foreach hash<auto> m in (ci.instance_methods) {
                string name = m.name;
                if (name == "constructor" || name == "destructor") {
                    continue;
                }
                if (!seen{name}
                        && QoreCodeCompletion::CompletionEngine::matchesPrefix(name, prefix)) {
                    seen{name} = True;
                    result += <QoreCodeCompletion::CompletionItem>{
                        "label": name,
                        "kind": "method",
                        "isCallable": True,
                    };
                }
            }
            foreach hash<auto> c in (ci.constants) {
                string name = c.name;
                if (!seen{name}
                        && QoreCodeCompletion::CompletionEngine::matchesPrefix(name, prefix)) {
                    seen{name} = True;
                    result += <QoreCodeCompletion::CompletionItem>{
                        "label": name,
                        "kind": "constant",
                        "isCallable": False,
                    };
                }
            }
        }

        return result;
    }
}

} # namespace QLS
