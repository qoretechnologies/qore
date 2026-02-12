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

#! QLS LSP ServerCapabilities
public const ServerCapabilities = {
    /**
     * Defines how text documents are synced. Is either a detailed structure defining each notification or
     * for backwards compatibility the TextDocumentSyncKind number.
     */
    "textDocumentSync": TextDocumentSyncKind.Full,

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
        "resolveProvider": False,
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
        "full": True,
        "range": True,
    },

    /**
     * The server provides code action support.
     */
    "codeActionProvider": {
        "codeActionKinds": ("source.sortImports", "source"),
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

        #! Qore Documents in the current workspace
        hash<auto> workspaceDocs;

        #! Standard Qore modules
        hash<auto> stdModuleDocs;

        #! URIs of ignored documents
        hash<auto> ignoredUris;
    }

    private:internal {
        FileOutputStream fos;
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
                    d.create(0755);
                } catch (hash<ExceptionInfo> ex) {
                    return;
                }
            }

            # check if the log file can be opened and truncate it if appending is turned off
            try {
                fos = new FileOutputStream(logFile, appendToLog);
            } catch (hash<ExceptionInfo> ex) {
            }
        }
    }

    private:internal log(int verbosity, string fmt) {
        if (fos && verbosity <= logVerbosity) {
            string str = sprintf("%s: ", format_date("YYYY-MM-DD HH:mm:SS", now()));
            string msg = vsprintf(str + fmt + "\n", argv);
            fos.write(binary(msg));
        }
    }

    private:internal log(int verbosity, string fmt, softlist<auto> l) {
        if (fos && verbosity <= logVerbosity) {
            string str = sprintf("%s: ", format_date("YYYY-MM-DD HH:mm:SS", now()));
            string msg = vsprintf(str + fmt + "\n", l);
            fos.write(binary(msg));
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
        };
    }


    #=================
    # Main logic
    #=================

    #! Runs the main JSON-RPC server loop, reading requests from stdin and writing responses to stdout.
    /** @return the exit code (0 for normal shutdown, 1 for error)
    */
    public int main() {
        while (running) {
            # read JSON-RPC request
            hash<auto> received = Messenger::receive();
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
            if (responses) {
                map Messenger::send($1), responses, $1.typeCode() == NT_STRING;
            }

            # send additional messages
            if (initialized && clientInitialized) {
                while (messagesToSend.size()) {
                    list<auto> msg = extract messagesToSend, 0, 1;
                    foreach auto m in (msg) {
                        Messenger::send(m);
                    }
                }
            }
        }

        return exitCode;
    }

    #! Handles a single JSON-RPC request message and returns the response(s).
    /** @param msg the raw JSON-RPC message string
        @return a list of JSON-RPC response strings to send back (may be empty for notifications)
    */
    softlist<auto> handleRequest(string msg) {
        # parse the request
        auto request = parse_json(msg);

        # validate request
        if (request.typeCode() != NT_HASH) {
            return ErrorResponse::invalidRequest({"received": msg, "jsonrpc": jsonRpcVer},
                "Invalid JSON-RPC request");
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

        # call appropriate method
        try {
            softlist<auto> responses = methodMap{request.method}(request);
            return responses;
        } catch (hash<ExceptionInfo> ex) {
            log(0, "unhandled exception in method '%s': %s: %s", request.method, ex.err, ex.desc);
            if (request.id) {
                return ErrorResponse::internalError(jsonRpcVer,
                    sprintf("%s: %s", ex.err, ex.desc));
            }
            return NOTHING;
        }
    }


    #=================
    # Parsing
    #=================

    private:internal parseFilesInWorkspace() {
        if (!rootPath) {
            log(0, "WARNING: root path not set - workspace files will not be parsed");
            return;
        }

        # find all Qore files in the workspace
        list<string> qoreFiles;
        try {
            qoreFiles = Files::find_qore_files(rootPath);
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
        }

        # create a list of file URIs
        int rootPathSize = rootPath.size();
        qoreFiles = map rootUri + $1.substr(rootPathSize), qoreFiles;
        log(1, "qore files in workspace: %N", qoreFiles);

        # measure start time
        date start = now_us();

        # parse everything
        map workspaceDocs{$1} = new Document($1), qoreFiles;

        # measure end time
        date end = now_us();
        log(0, "parsing of %d workspace files took: %y", qoreFiles.size(), end - start);
    }

    private:internal parseStdModules() {
        # find standard Qore modules
        list<string> moduleFiles;
        try {
            moduleFiles = Files::find_std_modules();
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

        # create a list of file URIs
        moduleFiles = map Document::getFileUri($1), moduleFiles;

        # parse everything
        map stdModuleDocs{$1} = new Document($1), moduleFiles;
    }


    #=================
    # General methods
    #=================

    #! "initialize" method handler
    private *string meth_initialize(hash<auto> request) {
        log(0, "initialize request received: %N", request);
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
        if (request.params.rootUri) {
            rootUri = request.params.rootUri;
        }
        if (request.params.rootPath) {
            rootPath = request.params.rootPath;
        }
        if (!rootPath && rootUri) {
            rootPath = parse_url(rootUri).path;
        }
        if (!rootUri && rootPath) {
            rootUri = Document::getFileUri(rootPath);
        }

        try {
            # parse all Qore files in the current workspace
            parseFilesInWorkspace();
        } catch (hash<ExceptionInfo> ex) {
            return ErrorResponse::invalidParams(request, sprintf("%s: %s: %N", ex.err, ex.desc, ex));
        }

        try {
            # parse standard Qore modules
            parseStdModules();
        } catch (hash<ExceptionInfo> ex) {
            return ErrorResponse::internalError(jsonRpcVer, sprintf("%s: %s: %N", ex.err, ex.desc, ex));
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
        # ignore for now
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
        log(0, "changed configuration received: %N", clientConfig);

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
        prepareLogFile(logFile);

        if (logging && !fos) {
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
            switch (change.type) {
                case FileChangeType.Created:
                case FileChangeType.Changed:
                    workspaceDocs{change.uri} = new Document(change.uri);
                    break;
                case FileChangeType.Deleted:
                    remove workspaceDocs{change.uri};
                    break;
                default:
                    break;
            }
        }
        return NOTHING;
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
        list<auto> symbols = ();
        map symbols += $1.findMatchingSymbols(query), documents.iterator();
        map symbols += $1.findMatchingSymbols(query), workspaceDocs.iterator();
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
            if (workspaceDocs{textDoc.uri}) {
                doc = remove workspaceDocs{textDoc.uri};
                # Update with editor text which may differ from the file on disk
                doc.didChange({"text": textDoc.text});
            } else {
                # ignore non-local files
                if (textDoc.uri =~ /^file:\//) {
                    doc = new Document(textDoc.uri, textDoc.text, textDoc.languageId, textDoc.version);
                } else {
                    ignoredUris{textDoc.uri} = True;
                    log(1, "added URI to ignored: '%s'", textDoc.uri);
                }
            }
            if (doc != NOTHING) {
                documents{textDoc.uri} = doc;
                log(1, "opened text document: '%s'", textDoc.uri);

                if (doc.getParseErrorCount() > 0) {
                    *list<auto> diagnostics = doc.getDiagnostics();
                    responses += Notification::diagnostics(jsonRpcVer, textDoc.uri, diagnostics);
                    continue;
                }
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

        if (doc.getParseErrorCount() > 0) {
            *list<auto> diagnostics = doc.getDiagnostics();
            return Notification::diagnostics(jsonRpcVer, textDoc.uri, diagnostics);
        }
        return Notification::diagnostics(jsonRpcVer, textDoc.uri);
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
        # ignore for now
        return NOTHING;
    }

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
        workspaceDocs{uri} = remove documents{uri};
        log(1, "closed text document: '%s'", uri);
        return NOTHING;
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

        # find matching symbols
        list<auto> symbols = ();
        map symbols += $1.findMatchingSymbols(query, True), documents.iterator();
        map symbols += $1.findMatchingSymbols(query, True), workspaceDocs.iterator();
        map symbols += $1.findMatchingSymbols(query, True), stdModuleDocs.iterator();

        for (int i = symbols.size() - 1; i >= 0; i--) {
            if (SymbolUsageFastMap{string(symbolInfo.usage)}) {
                if (!SymbolUsageFastMap{string(symbolInfo.usage)}.contains(symbols[i].kind)) {
                    splice symbols, i, 1;
                }
            }
        }

        # prepare results
        hash<auto> result = {"range": symbolInfo.range, "contents": ()};
        foreach hash<auto> symbol in (symbols) {
            *hash<auto> description;
            if (documents{symbol.location.uri}) {
                description = documents{symbol.location.uri}.hoverInfo(symbol.kind, symbol.location.range.start);
            } else if (workspaceDocs{symbol.location.uri}) {
                description = workspaceDocs{symbol.location.uri}.hoverInfo(symbol.kind,
                    symbol.location.range.start);
            } else if (stdModuleDocs{symbol.location.uri}) {
                description = stdModuleDocs{symbol.location.uri}.hoverInfo(symbol.kind,
                    symbol.location.range.start);
            }

            if (description) {
                result.contents += replace(description.description, "*", "\\*");
            }
        }

        return make_jsonrpc_response(jsonRpcVer, request.id, result);
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

        # find references
        Document doc = documents{request.params.textDocument.uri};
        *list<auto> references = doc.findReferences(request.params.position,
            request.params.context.includeDeclaration);
        return make_jsonrpc_response(jsonRpcVer, request.id, references ?? ());
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
                default:
                    ret = doc.findSymbols();
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
        CSTCompletionProvider provider(doc, request.params.position);
        QoreCodeCompletion::CompletionEngine engine(provider);

        # Get completions
        list<hash<QoreCodeCompletion::CompletionItem>> items = engine.complete(input);

        # Convert to LSP format
        list<hash<auto>> lspItems = ();
        foreach hash<QoreCodeCompletion::CompletionItem> item in (items) {
            string insertText = item.label;
            if (item.isCallable) {
                insertText += "(";
            } else if (item.kind == "namespace") {
                insertText += "::";
            }
            lspItems += {
                "label": item.label,
                "kind": LspCompletionItemKind{item.kind} ?? 1,
                "detail": item.detail,
                "documentation": item.documentation,
                "insertText": insertText,
            };
        }

        return make_jsonrpc_response(jsonRpcVer, request.id, {
            "isIncomplete": False,
            "items": lspItems,
        });
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
                    if (sym.name == funcName && sym.params) {
                        params = sym.params;
                        returnType = sym.returnType;
                        break;
                    }
                }
            }
        } else if (isScopeAccess && target) {
            *list<auto> members = doc.findTypeMembers(target, True);
            if (members) {
                foreach hash<auto> sym in (members) {
                    if (sym.name == funcName && sym.params) {
                        params = sym.params;
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
                    if (sym.name == funcName && sym.params) {
                        params = sym.params;
                        returnType = sym.returnType;
                        break;
                    }
                }
            }
        }

        if (!params) {
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
            paramInfos += {"label": plabel};
        }

        string sigLabel = funcName + "(" + paramLabels.join(", ") + ")";
        if (returnType) {
            sigLabel += " returns " + returnType;
        }

        hash<auto> signature = {
            "label": sigLabel,
            "parameters": paramInfos,
        };

        return make_jsonrpc_response(jsonRpcVer, request.id, {
            "signatures": (signature,),
            "activeSignature": 0,
            "activeParameter": commaCount < paramInfos.size() ? commaCount : paramInfos.size() - 1,
        });
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

        # Search for type declaration across all documents
        list<auto> symbols = ();
        map symbols += $1.findMatchingSymbols(typeName, True), documents.iterator();
        map symbols += $1.findMatchingSymbols(typeName, True), workspaceDocs.iterator();
        map symbols += $1.findMatchingSymbols(typeName, True), stdModuleDocs.iterator();

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
                        "position": {
                            "line": sym.location.range.start.line,
                            "character": sym.location.range.start.character,
                        },
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

        Document doc = documents{uri};
        *list<auto> refs = doc.findReferences(lens.data.position, False);
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
            # Search across all docs
            list<auto> symbols = ();
            map symbols += $1.findMatchingSymbols(parentName, True), documents.iterator();
            map symbols += $1.findMatchingSymbols(parentName, True), workspaceDocs.iterator();
            map symbols += $1.findMatchingSymbols(parentName, True), stdModuleDocs.iterator();

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

        code scanDoc = sub (Document doc, string docUri) {
            *list<auto> symbols = doc.findSymbols(True, True);
            if (!symbols) {
                return;
            }
            foreach hash<auto> sym in (symbols) {
                if (sym.kind != ASTSymbolKind.Class) {
                    continue;
                }
                # Check if this class inherits from targetName
                *list<string> parents = doc.getSuperclassNames(sym.location.range.start);
                if (parents && parents.contains(targetName)) {
                    result += {
                        "name": sym.name,
                        "kind": LspSymbolKind.Class,
                        "uri": docUri,
                        "range": sym.location.range,
                        "selectionRange": sym.location.range,
                    };
                }
            }
        };

        map scanDoc(documents{$1}, $1), keys documents;
        map scanDoc(workspaceDocs{$1}, $1), keys workspaceDocs;
        map scanDoc(stdModuleDocs{$1}, $1), keys stdModuleDocs;

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
        *hash<auto> otherTrees = buildOtherTrees(uri);

        *list<auto> refs = doc.findReferencesCrossDoc(request.params.position, True, otherTrees);
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
            code scanDoc = sub (Document d, string docUri) {
                *list<auto> symbols = d.findSymbols(True, True);
                if (!symbols) {
                    return;
                }
                foreach hash<auto> sym in (symbols) {
                    if (sym.kind != ASTSymbolKind.Class) {
                        continue;
                    }
                    *list<string> parents = d.getSuperclassNames(sym.location.range.start);
                    if (parents && parents.contains(symbolInfo.name)) {
                        result += sym.location;
                    }
                }
            };

            map scanDoc(documents{$1}, $1), keys documents;
            map scanDoc(workspaceDocs{$1}, $1), keys workspaceDocs;
            map scanDoc(stdModuleDocs{$1}, $1), keys stdModuleDocs;
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

                code scanDoc = sub (Document d, string docUri) {
                    *list<auto> symbols = d.findSymbols(True, True);
                    if (!symbols) {
                        return;
                    }
                    foreach hash<auto> sym in (symbols) {
                        if (sym.kind != ASTSymbolKind.Class) {
                            continue;
                        }
                        *list<string> parents = d.getSuperclassNames(sym.location.range.start);
                        if (!parents || !parents.contains(enclosingClass)) {
                            continue;
                        }
                        # Check if this class has the method
                        *list<auto> members = d.findTypeMembers(sym.name, False);
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
                };

                map scanDoc(documents{$1}, $1), keys documents;
                map scanDoc(workspaceDocs{$1}, $1), keys workspaceDocs;
                map scanDoc(stdModuleDocs{$1}, $1), keys stdModuleDocs;
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
        *hash<auto> otherTrees = buildOtherTrees(uri);
        *list<auto> refs = doc.findReferencesCrossDoc(item.range.start, False, otherTrees);
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
            *Document refDoc;
            if (documents{refUri}) {
                refDoc = documents{refUri};
            } else if (workspaceDocs{refUri}) {
                refDoc = workspaceDocs{refUri};
            } else if (stdModuleDocs{refUri}) {
                refDoc = stdModuleDocs{refUri};
            }
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
                *hash<auto> otherTrees = buildOtherTrees(uri);
                defs = doc.resolveDefinitionCrossDoc(target.position, otherTrees);
            }

            if (defs && defs.size() > 0) {
                hash<auto> def = defs[0];
                string key = def.uri + ":" + target.name + ":" + string(def.range.start.line);
                if (!outgoing{key}) {
                    # Resolve the target's symbol kind
                    int targetKind = LspSymbolKind.Function;
                    *Document targetDoc = documents{def.uri}
                        ?? workspaceDocs{def.uri}
                        ?? stdModuleDocs{def.uri};
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
        *list<auto> tokens = doc.semanticTokens();

        list<int> data = encodeSemanticTokens(tokens);
        return make_jsonrpc_response(jsonRpcVer, request.id, {"data": data});
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

        list<auto> actions = ();

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

    #! Build a hash of URI → AstTree for all open documents except the given one
    private *hash<auto> buildOtherTrees(string excludeUri) {
        hash<auto> otherTrees;
        foreach string docUri in (keys documents) {
            if (docUri == excludeUri) {
                continue;
            }
            *astparser::AstTree t = documents{docUri}.getTree();
            if (t) {
                otherTrees{docUri} = t;
            }
        }
        foreach string docUri in (keys workspaceDocs) {
            if (docUri == excludeUri) {
                continue;
            }
            *astparser::AstTree t = workspaceDocs{docUri}.getTree();
            if (t) {
                otherTrees{docUri} = t;
            }
        }
        foreach string docUri in (keys stdModuleDocs) {
            if (docUri == excludeUri) {
                continue;
            }
            *astparser::AstTree t = stdModuleDocs{docUri}.getTree();
            if (t) {
                otherTrees{docUri} = t;
            }
        }
        return otherTrees;
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
        *hash<auto> otherTrees = buildOtherTrees(request.params.textDocument.uri);
        if (otherTrees) {
            resolved = doc.resolveDefinitionCrossDoc(request.params.position, otherTrees);
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

        # find matching symbols
        list<auto> symbols = ();
        map symbols += $1.findMatchingSymbols(query, True), documents.iterator();
        map symbols += $1.findMatchingSymbols(query, True), workspaceDocs.iterator();
        map symbols += $1.findMatchingSymbols(query, True), stdModuleDocs.iterator();

        for (int i = symbols.size() - 1; i >= 0; i--) {
            if (SymbolUsageFastMap{string(symbolInfo.usage)}) {
                if (!SymbolUsageFastMap{string(symbolInfo.usage)}.contains(symbols[i].kind)) {
                    splice symbols, i, 1;
                }
            }
        }

        # prepare results
        list<auto> result = map $1.location, symbols;
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
};

#! Completion provider using CSTSearcher for static analysis
/** This provider resolves symbols from the parsed syntax tree of the current document.
*/
class CSTCompletionProvider inherits QoreCodeCompletion::AbstractCompletionProvider {
    private {
        Document doc;
        hash<auto> position;
    }

    constructor(Document doc, hash<auto> position) {
        self.doc = doc;
        self.position = position;
    }

    int getParseOptions() {
        return doc.getParseDirectiveOptions();
    }

    list<hash<QoreCodeCompletion::CompletionItem>> getVisibleSymbols(string prefix) {
        *list<auto> symbols = doc.findScopeSymbolsDetailed(position);
        if (!symbols) {
            return ();
        }

        list<hash<QoreCodeCompletion::CompletionItem>> result = ();
        hash<string, bool> seen;
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
        return result;
    }

    list<hash<QoreCodeCompletion::CompletionItem>> getClassNames(string prefix) {
        *list<auto> symbols = doc.findScopeSymbolsDetailed(position);
        if (!symbols) {
            return ();
        }

        list<hash<QoreCodeCompletion::CompletionItem>> result = ();
        hash<string, bool> seen;
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
        return result;
    }

    list<hash<QoreCodeCompletion::CompletionItem>> getInstanceMembers(string target, string prefix) {
        # Resolve the type of the target variable
        *string typeName = doc.getSymbolType({
            "line": position.line,
            "character": position.character > 0 ? position.character - 1 : 0,
        });
        if (!typeName) {
            # Fall back: try the target name as a type directly
            typeName = target;
        }

        *list<auto> members = doc.findTypeMembers(typeName, True);
        if (!members) {
            return ();
        }

        list<hash<QoreCodeCompletion::CompletionItem>> result = ();
        hash<string, bool> seen;
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
        return result;
    }

    list<hash<QoreCodeCompletion::CompletionItem>> getScopeMembers(string target, string prefix) {
        *list<auto> members = doc.findTypeMembers(target, True);
        if (!members) {
            return ();
        }

        list<hash<QoreCodeCompletion::CompletionItem>> result = ();
        hash<string, bool> seen;
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
        return result;
    }
}

} # namespace QLS
