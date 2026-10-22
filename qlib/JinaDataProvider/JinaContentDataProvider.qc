# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JinaContentDataProvider class definition

/** JinaContentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace JinaDataProvider {
#! Shared transport and validation for the documented APIs outside Jina's inference schema
/** Reader, Search, Segmenter, and DeepSearch use their published JSON contracts. Chat's upstream
    operation has no request body, and batch downloads declare JSON instead of their actual JSONL.
    These actions use explicit data types rather than inventing vendor schema fields.
*/
public class JinaContentDataProvider inherits JinaDataProviderCommon {
    public {
        #! Form options encoded as Reader and Search HTTP headers
        const HeaderOptions = {
            "return_format": "X-Return-Format",
            "engine": "X-Engine",
            "with_links_summary": "X-With-Links-Summary",
            "with_images_summary": "X-With-Images-Summary",
            "no_cache": "X-No-Cache",
            "with_generated_alt": "X-With-Generated-Alt",
            "retain_images": "X-Retain-Images",
            "locale": "X-Locale",
            "set_cookie": "X-Set-Cookie",
            "proxy_url": "X-Proxy-Url",
            "timeout": "X-Timeout",
            "token_budget": "X-Token-Budget",
            "site": "X-Site",
            "respond_with": "X-Respond-With",
            "with_favicons": "X-With-Favicons",
            "preset": "X-Preset",
            "retain_links": "X-Retain-Links",
            "retain_media": "X-Retain-Media",
            "respond_timing": "X-Respond-Timing",
            "base": "X-Base",
            "markdown_chunking": "X-Markdown-Chunking",
            "target_selector": "X-Target-Selector",
            "wait_for_selector": "X-Wait-For-Selector",
            "remove_selector": "X-Remove-Selector",
            "proxy": "X-Proxy",
            "user_agent": "X-User-Agent",
            "referer": "X-Referer",
            "robots_txt": "X-Robots-Txt",
            "with_iframe": "X-With-Iframe",
            "with_shadow_dom": "X-With-Shadow-Dom",
            "keep_img_data_url": "X-Keep-Img-Data-Url",
            "remove_overlay": "X-Remove-Overlay",
            "detach_invisibles": "X-Detach-Invisibles",
            "cache_tolerance": "X-Cache-Tolerance",
            "no_gfm": "X-No-Gfm",
            "md_heading_style": "X-Md-Heading-Style",
            "md_hr": "X-Md-Hr",
            "md_bullet_list_marker": "X-Md-Bullet-List-Marker",
            "md_em_delimiter": "X-Md-Em-Delimiter",
            "md_strong_delimiter": "X-Md-Strong-Delimiter",
            "md_link_style": "X-Md-Link-Style",
            "md_link_reference_style": "X-Md-Link-Reference-Style",
            "do_not_track": "DNT",
        };
    }

    #! Copies an authenticated transport for this provider
    /** @param rest the transport to copy */
    constructor(RestClientIo::RestClientIo rest) : JinaDataProviderCommon(rest) {
    }

    #! Creates the provider from constructor options
    /** @param options the ConstructorOptions values
        @throw CONSTRUCTOR-ERROR authentication is missing or an option is invalid
    */
    constructor(*hash<auto> options) : JinaDataProviderCommon(options) {
    }

    #! Rejects empty inputs and invalid numeric controls before contacting Jina
    private validateRequest(hash<auto> req) {
        if (getName() == "read-url") {
            if (!req.url.val() && !req.html.val() && !req.file) {
                throw "JINA-REQUEST-ERROR", "provide a URL, HTML, or a document file";
            }
            if (exists req.html && exists req.file) {
                throw "JINA-REQUEST-ERROR", "HTML and file input cannot be supplied together";
            }
        }
        if (getName() == "segment-text" && exists req.head && exists req.tail) {
            throw "JINA-REQUEST-ERROR", "head and tail cannot be supplied together";
        }
        foreach string key in (("url", "html", "q", "content", "batch_id")) {
            if (exists req{key} && !req{key}.val()) {
                throw "JINA-REQUEST-ERROR", sprintf("%y must not be empty", key);
            }
        }
        foreach string key in (("timeout", "token_budget", "num", "max_chunk_length", "head", "tail",
                "max_tokens", "budget_tokens", "max_attempts", "team_size", "max_returned_urls")) {
            if (exists req{key} && req{key} <= 0) {
                throw "JINA-REQUEST-ERROR", sprintf("%y must be greater than zero", key);
            }
        }
        foreach string key in (("page", "cache_tolerance")) {
            if (exists req{key} && req{key} < 0) {
                throw "JINA-REQUEST-ERROR", sprintf("%y must not be negative", key);
            }
        }
        if (exists req.messages && !req.messages) {
            throw "JINA-REQUEST-ERROR", "messages must not be empty";
        }
        string expected_role = "user";
        foreach hash<auto> message in (req.messages ?? ()) {
            if (getName() == "chat") {
                if (message.role != expected_role) {
                    throw "JINA-REQUEST-ERROR", "vision chat requires alternating user and assistant messages";
                }
                expected_role = expected_role == "user" ? "assistant" : "user";
            }
            if (!message.content.val()) {
                throw "JINA-REQUEST-ERROR", "message content must not be empty";
            }
            if (message.content.typeCode() == NT_LIST) {
                foreach hash<auto> part in (message.content) {
                    if ((part.type == "text" && (!part.text.val() || exists part.image_url))
                            || (part.type == "image_url" && (!part.image_url.url.val() || exists part.text))) {
                        throw "JINA-REQUEST-ERROR", "message part must contain the selected text or image URL";
                    }
                }
            }
        }
        if (exists req.viewport && (!req.viewport.width || !req.viewport.height
                || req.viewport.width <= 0 || req.viewport.height <= 0)) {
            throw "JINA-REQUEST-ERROR", "viewport requires positive width and height";
        }
    }

    #! Sends a Reader or Search request through a copy of the connection's transport
    private hash<auto> readContent(string service, hash<auto> req) {
        validateRequest(req);
        if (service == "r" && exists req.page && req.page < 1) {
            throw "JINA-REQUEST-ERROR", "document page numbers start at 1";
        }
        string region = remove req.region;
        string url = sprintf("https://%s%s.jina.ai", region == "eu" ? "eu." : "", service);
        RestClient::AbstractRestClient client = rest.copyWithUrl(url);
        client.clearConnectionPath();
        hash<auto> hdr = {"Accept": MimeTypeJson};
        foreach hash<auto> item in (HeaderOptions.pairIterator()) {
            if (exists req{item.key}) {
                auto value = remove req{item.key};
                hdr{item.value} = value.typeCode() == NT_BOOLEAN
                    ? (value ? (item.key == "do_not_track" ? "1" : "true") : "false")
                    : value.toString();
            }
        }
        if (exists req.file) {
            hash<auto> file = remove req.file;
            if (!file.content.val()) {
                throw "JINA-REQUEST-ERROR", "file content must not be empty";
            }
            # FileDataType has already normalized binary or data-URI content to base64.
            req.file = file.content;
        }
        hash<auto> response = client.restPost("/", req, hdr).body;
        if (response.code != 200 || !exists response.data) {
            throw "JINA-RESPONSE-ERROR", "Jina did not return successful content results";
        }
        return response;
    }

    #! Sends a completed-response chat request to the vision or research service
    private hash<auto> chat(hash<auto> req, bool deepsearch) {
        validateRequest(req);
        RestClient::AbstractRestClient client = rest;
        if (deepsearch) {
            client = rest.copyWithUrl("https://deepsearch.jina.ai");
            client.clearConnectionPath();
        }
        req.stream = False;
        hash<auto> response = client.restDoRequestTarget("POST", <RestClient::RestRequestTarget>{
            "mode": RestClient::RestPathMode::OriginAbsolute, "path": "/v1/chat/completions"},
            req, {"Accept": MimeTypeJson}).body;
        if (exists response.created) {
            response.created = gmtime(response.created);
        }
        return response;
    }

    #! Parses a batch's JSONL download, reporting the line number of a malformed record
    private hash<auto> downloadBatch(string suffix, hash<auto> req) {
        validateRequest(req);
        string path = sprintf("/v1/batch/%s/%s", encode_url(req.batch_id, True), suffix);
        auto body = rest.restDoRawRequest("GET", path, NOTHING,
            {"Accept": "application/x-ndjson"}).body;
        if (body.typeCode() == NT_BINARY) {
            body = body.toString();
        }
        if (body.typeCode() != NT_STRING) {
            throw "JINA-RESPONSE-ERROR", "Jina batch download did not return JSONL text";
        }
        list<hash<auto>> records = ();
        int line_number = 0;
        foreach string line in (split("\n", body)) {
            ++line_number;
            if (line =~ /^\s*$/) {
                continue;
            }
            auto record;
            try {
                record = parse_json(line);
            } catch (hash<ExceptionInfo> ex) {
                throw "JINA-RESPONSE-ERROR", sprintf("Invalid JSON in batch download at line %d", line_number);
            }
            if (record.typeCode() != NT_HASH) {
                throw "JINA-RESPONSE-ERROR", sprintf("Expected a JSON object at batch download line %d", line_number);
            }
            records += record;
        }
        return {"records": records};
    }

    #! Lists recent batches for download actions
    /** @return supported reference list names */
    *hash<string, bool> getSupportedReferenceData() {
        if (getName() == "download-batch-output" || getName() == "download-batch-errors") {
            return {"batches": True};
        }
    }

    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> opts) {
        return getSchemaProvider("get-batch").getReferenceData(type, opts);
    }
}

#! Executes the read-url action
public class JinaReaderDataProvider inherits JinaContentDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{"name": "read-url", "type": "JinaReaderDataProvider",
            "supports_request": True, "constructor_options": ConstructorOptions};
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
    }

    #! Uses an existing authenticated REST client
    /** @param rest the client to copy */
    constructor(RestClientIo::RestClientIo rest) : JinaContentDataProvider(rest) {
    }

    #! Creates the provider from connection options
    /** @param options the ConstructorOptions values
        @throw CONSTRUCTOR-ERROR authentication is missing or an option is invalid
    */
    constructor(*hash<auto> options) : JinaContentDataProvider(options) {
    }

    #! Returns the stable action name
    /** @return the provider name */
    string getName() {
        return ProviderInfo.name;
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return JinaReaderRequestDataType;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return JinaReaderResponseDataType;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return readContent("r", req);
    }
}

#! Executes the search-web action
public class JinaSearchDataProvider inherits JinaContentDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{"name": "search-web", "type": "JinaSearchDataProvider",
            "supports_request": True, "constructor_options": ConstructorOptions};
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
    }

    #! Uses an existing authenticated REST client
    /** @param rest the client to copy */
    constructor(RestClientIo::RestClientIo rest) : JinaContentDataProvider(rest) {
    }

    #! Creates the provider from connection options
    /** @param options the ConstructorOptions values
        @throw CONSTRUCTOR-ERROR authentication is missing or an option is invalid
    */
    constructor(*hash<auto> options) : JinaContentDataProvider(options) {
    }

    #! Returns the stable action name
    /** @return the provider name */
    string getName() {
        return ProviderInfo.name;
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return JinaSearchRequestDataType;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return JinaSearchResponseDataType;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return readContent("s", req);
    }
}

#! Executes the segment-text action
public class JinaSegmentDataProvider inherits JinaContentDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{"name": "segment-text", "type": "JinaSegmentDataProvider",
            "supports_request": True, "constructor_options": ConstructorOptions};
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
    }

    #! Uses an existing authenticated REST client
    /** @param rest the client to copy */
    constructor(RestClientIo::RestClientIo rest) : JinaContentDataProvider(rest) {
    }

    #! Creates the provider from connection options
    /** @param options the ConstructorOptions values
        @throw CONSTRUCTOR-ERROR authentication is missing or an option is invalid
    */
    constructor(*hash<auto> options) : JinaContentDataProvider(options) {
    }

    #! Returns the stable action name
    /** @return the provider name */
    string getName() {
        return ProviderInfo.name;
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return JinaSegmentRequestDataType;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return JinaSegmentResponseDataType;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        validateRequest(req);
        return rest.restDoRequestTarget("POST", <RestClient::RestRequestTarget>{
            "mode": RestClient::RestPathMode::OriginAbsolute, "path": "/v1/segment"}, req, {"Accept": MimeTypeJson}).body;
    }
}

#! Executes the chat action
public class JinaChatDataProvider inherits JinaContentDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{"name": "chat", "type": "JinaChatDataProvider",
            "supports_request": True, "constructor_options": ConstructorOptions};
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
    }

    #! Uses an existing authenticated REST client
    /** @param rest the client to copy */
    constructor(RestClientIo::RestClientIo rest) : JinaContentDataProvider(rest) {
    }

    #! Creates the provider from connection options
    /** @param options the ConstructorOptions values
        @throw CONSTRUCTOR-ERROR authentication is missing or an option is invalid
    */
    constructor(*hash<auto> options) : JinaContentDataProvider(options) {
    }

    #! Returns the stable action name
    /** @return the provider name */
    string getName() {
        return ProviderInfo.name;
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return JinaChatRequestDataType;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return JinaChatResponseDataType;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return chat(req, False);
    }
}

#! Executes the deep-search action
public class JinaDeepSearchDataProvider inherits JinaContentDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{"name": "deep-search", "type": "JinaDeepSearchDataProvider",
            "supports_request": True, "constructor_options": ConstructorOptions};
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
    }

    #! Uses an existing authenticated REST client
    /** @param rest the client to copy */
    constructor(RestClientIo::RestClientIo rest) : JinaContentDataProvider(rest) {
    }

    #! Creates the provider from connection options
    /** @param options the ConstructorOptions values
        @throw CONSTRUCTOR-ERROR authentication is missing or an option is invalid
    */
    constructor(*hash<auto> options) : JinaContentDataProvider(options) {
    }

    #! Returns the stable action name
    /** @return the provider name */
    string getName() {
        return ProviderInfo.name;
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return JinaDeepSearchRequestDataType;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return JinaChatResponseDataType;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return chat(req, True);
    }
}

#! Executes the download-batch-output action
public class JinaBatchOutputDataProvider inherits JinaContentDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{"name": "download-batch-output", "type": "JinaBatchOutputDataProvider",
            "supports_request": True, "constructor_options": ConstructorOptions};
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
    }

    #! Uses an existing authenticated REST client
    /** @param rest the client to copy */
    constructor(RestClientIo::RestClientIo rest) : JinaContentDataProvider(rest) {
    }

    #! Creates the provider from connection options
    /** @param options the ConstructorOptions values
        @throw CONSTRUCTOR-ERROR authentication is missing or an option is invalid
    */
    constructor(*hash<auto> options) : JinaContentDataProvider(options) {
    }

    #! Returns the stable action name
    /** @return the provider name */
    string getName() {
        return ProviderInfo.name;
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return JinaBatchDownloadRequestDataType;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return JinaBatchDownloadResponseDataType;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return downloadBatch("output", req);
    }
}

#! Executes the download-batch-errors action
public class JinaBatchErrorsDataProvider inherits JinaContentDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{"name": "download-batch-errors", "type": "JinaBatchErrorsDataProvider",
            "supports_request": True, "constructor_options": ConstructorOptions};
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
    }

    #! Uses an existing authenticated REST client
    /** @param rest the client to copy */
    constructor(RestClientIo::RestClientIo rest) : JinaContentDataProvider(rest) {
    }

    #! Creates the provider from connection options
    /** @param options the ConstructorOptions values
        @throw CONSTRUCTOR-ERROR authentication is missing or an option is invalid
    */
    constructor(*hash<auto> options) : JinaContentDataProvider(options) {
    }

    #! Returns the stable action name
    /** @return the provider name */
    string getName() {
        return ProviderInfo.name;
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return JinaBatchDownloadRequestDataType;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return JinaBatchDownloadResponseDataType;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return downloadBatch("errors", req);
    }
}

}
