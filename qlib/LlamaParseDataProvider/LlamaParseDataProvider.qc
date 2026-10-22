# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LlamaParseDataProvider class definitions

/** LlamaParseDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace LlamaParseDataProvider {
public class LlamaParseDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "LlamaParse API key (Bearer)",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "LlamaParse API key.",
                "required": True,
                "sensitive": True,
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "Server URL",
                "short_desc": "LlamaParse REST API URL",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "URL of the LlamaParse REST API.",
                "default_value": "https://api.cloud.llamaindex.ai/api/parsing",
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("LlamaParseRestClient::LlamaParseRestClientIo"),
                    NOTHING, {DTT_ClientOnly: True}),
                "desc": "The LlamaParse REST client to use",
            },
        };
    }

    private {
        *LlamaParseRestClient::LlamaParseRestClientIo rest;
    }

    constructor(LlamaParseRestClient::LlamaParseRestClientIo rest) {
        self.rest = rest;
    }

    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        } else {
            if (!copts.apikey.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "missing required 'apikey' argument for LlamaParse API "
                    "communication";
            }
            rest = new LlamaParseRestClient::LlamaParseRestClientIo({
                "url": copts.url,
                "apikey": copts.apikey,
            });
        }
    }
}

#! The root of the LlamaParse application
/** Almost every action is generated from the vendored LlamaIndex Cloud API description, so the root is
    the action set itself; see
    @ref LlamaParseDataProvider::LlamaParseManifest "LlamaParseManifest" for what is exported and why.

    Two children are hand-written, because neither can be generated:

    - \c upload-file sends a \c multipart/form-data body, which
      @ref restschemaactionsintro "RestSchemaActions" does not support
    - \c parse-document parses a document and waits for the result, where the generated
      \c parse-file returns as soon as the job is started

    @since %LlamaParseDataProvider 1.0
*/
public class LlamaParseDataProvider inherits RestSchemaActions::RestSchemaActionSetDataProvider {
    public {
        #! The name of the hand-written synchronous parse action
        const ParseDocumentChildName = "parse-document";

        #! The name of the hand-written upload action
        const UploadFileChildName = "upload-file";

        const ProviderInfo = <DataProviderInfo>{
            "name": "llamaparse",
            "type": "LlamaParseDataProvider",
            "supports_children": True,
            "constructor_options": LlamaParseDataProviderCommon::ConstructorOptions,
            "children_can_support_apis": True,
        };
    }

    #! Creates the object from a REST client
    constructor(*RestClient::AbstractRestClient rest)
            : RestSchemaActionSetDataProvider(LlamaParseSchema::getActionSet(), rest) {
    }

    #! Creates the object from options
    constructor(*hash<auto> options)
            : RestSchemaActionSetDataProvider(LlamaParseSchema::getActionSet(),
                LlamaParseDataProvider::getClient(options)) {
    }

    string getName() { return ProviderInfo.name; }

    private *list<string> getChildProviderNamesImpl() {
        return (RestSchemaActionSetDataProvider::getChildProviderNamesImpl() ?? ())
            + (ParseDocumentChildName, UploadFileChildName);
    }

    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case ParseDocumentChildName:
                return new LlamaParseDocumentDataProvider(getIoClient());
            case UploadFileChildName:
                return new LlamaParseUploadFileDataProvider(getIoClient());
        }
        return RestSchemaActionSetDataProvider::getChildProviderImpl(name);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the async client the hand-written children need
    private LlamaParseRestClient::LlamaParseRestClientIo getIoClient() {
        if (rest instanceof LlamaParseRestClient::LlamaParseRestClientIo) {
            return cast<LlamaParseRestClient::LlamaParseRestClientIo>(rest);
        }
        throw "LLAMAPARSE-CLIENT-ERROR", sprintf("this action needs a LlamaParseRestClientIo, but the "
            "provider holds %s", rest ? sprintf("a %y", rest.className()) : "no client");
    }

    #! Returns a client built from constructor options
    private static LlamaParseRestClient::LlamaParseRestClientIo getClient(*hash<auto> options) {
        if (options.restclient) {
            return options.restclient;
        }
        return new LlamaParseRestClient::LlamaParseRestClientIo(options - "restclient");
    }
}

public class LlamaParseDataProviderFactory inherits AbstractDataProviderFactory {
    private {
        static Class cls = new Class("LlamaParseDataProvider");
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "llamaparse",
            "desc": "LlamaParse document-extraction data provider factory",
            "children_can_support_apis": True,
        };
    }
    private hash<DataProviderFactoryInfo> getInfoImpl() { return FactoryInfo; }
    private hash<DataProviderInfo> getProviderInfoImpl() { return LlamaParseDataProvider::ProviderInfo; }
    private Class getClassImpl() { return cls; }
}

public class LlamaParseRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "url": {
                "display_name": "Document URL",
                "short_desc": "Public URL of the document to parse",
                "desc": "Public URL pointing at the document to parse.  LlamaParse "
                    "fetches the document from this URL (no client-side upload).",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "https://example.com/document.pdf",
            },
            "result_type": {
                "display_name": "Result Type",
                "short_desc": "Output format: markdown, text, or json",
                "desc": "Result format requested from LlamaParse.",
                "type": AbstractDataProviderTypeMap."*softstring",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "markdown", "display_name": "Markdown"},
                    <AllowedValueInfo>{"value": "text", "display_name": "Plain Text"},
                    <AllowedValueInfo>{"value": "json", "display_name": "JSON"},
                ),
            },
            "parsing_instruction": {
                "display_name": "Parsing Instruction",
                "short_desc": "Natural-language hint guiding extraction",
                "desc": "Optional natural-language hint (e.g. \"focus on tables\").",
                "type": AbstractDataProviderTypeMap."*softstring",
            },
            "language": {
                "display_name": "Language",
                "short_desc": "Document language hint (ISO code)",
                "desc": "Document language hint.",
                "type": AbstractDataProviderTypeMap."*softstring",
            },
            "poll_interval_ms": {
                "display_name": "Poll Interval (ms)",
                "short_desc": "Time between job status polls",
                "desc": "Polling interval in milliseconds while waiting for the "
                    "extraction job to complete.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "job_timeout": {
                "display_name": "Job Timeout",
                "short_desc": "Overall job timeout",
                "desc": "Overall job timeout — if extraction takes longer than this, "
                    "the call errors out.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class LlamaParseResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "job_id": {"display_name": "Job ID",
                "desc": "LlamaParse-assigned job ID.",
                "type": AbstractDataProviderTypeMap."*string"},
            "status": {"display_name": "Status",
                "desc": "Final job status (SUCCESS / ERROR / CANCELLED).",
                "type": AbstractDataProviderTypeMap."*string"},
            "result_type": {"display_name": "Result Type",
                "desc": "Result type (markdown / text / json).",
                "type": AbstractDataProviderTypeMap."*string"},
            "content": {"display_name": "Content",
                "desc": "Extracted content — string for markdown/text, list of pages "
                    "for json.",
                "type": AbstractDataProviderTypeMap."any"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class LlamaParseDocumentDataProvider inherits LlamaParseDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "parse-document",
            "type": "LlamaParseDocumentDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        static LlamaParseRequestDataType RequestType = new LlamaParseRequestDataType();
        static LlamaParseResponseDataType ResponseType = new LlamaParseResponseDataType();
    }

    constructor(LlamaParseRestClient::LlamaParseRestClientIo rest)
            : LlamaParseDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : LlamaParseDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string url = req.url;
        hash<auto!> opts;
        if (req.result_type.val()) {
            opts.result_type = req.result_type;
        }
        if (req.parsing_instruction.val()) {
            opts.parsing_instruction = req.parsing_instruction;
        }
        if (req.language.val()) {
            opts.language = req.language;
        }
        if (exists req.poll_interval_ms) {
            opts.poll_interval_ms = req.poll_interval_ms;
        }
        if (exists req.job_timeout) {
            opts.job_timeout = req.job_timeout;
        }
        return rest.parseDocument(url, opts);
    }
}
}
