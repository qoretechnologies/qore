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
        };
    }


    #=================
    # Main logic
    #=================

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
