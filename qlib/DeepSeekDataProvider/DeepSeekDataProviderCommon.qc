# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DeepSeekDataProviderCommon class definition

/** DeepSeekDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DeepSeekDataProvider module
public namespace DeepSeekDataProvider {
#! The DeepSeek data provider common base class
/** Uses RestClientIo for async-capable multiplexed HTTP connections.
*/
public class DeepSeekDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        #! The default DeepSeek REST API URL
        const DefaultDeepSeekUrl = "https://api.deepseek.com";

        #! The default DeepSeek API version
        const DefaultDeepSeekApiVersion = "v1";

        #! Constructor options
        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "DeepSeek API Key",
                "short_desc": "DeepSeek API key for Bearer authentication",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The DeepSeek API key used for Bearer authentication.  Get your API key "
                    "from the [DeepSeek Platform](https://platform.deepseek.com/api_keys).",
                "sensitive": True,
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "Server URL",
                "short_desc": "The URL of the DeepSeek API",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The URL of the DeepSeek REST API.",
                "default_value": DefaultDeepSeekUrl,
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo"), NOTHING, {
                        DTT_ClientOnly: True,
                    }),
                "desc": "The REST client object to use",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "The options to use when creating a REST client",
                "type": AbstractDataProviderType::get(AutoHashType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Options to the RestClientIo constructor; only used if a RestClientIo "
                    "object is created for a call",
            },
            "api": <DataProviderOptionInfo>{
                "display_name": "API Version",
                "short_desc": "DeepSeek API version segment",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The default API path for chat requests; beta features select their endpoint automatically.",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "v1", "display_name": "Stable API"},
                    <AllowedValueInfo>{"value": "beta", "display_name": "Beta API"},
                ),
                "default_value": DefaultDeepSeekApiVersion,
            },
        };

    }

    private {
        #! The REST client object for API calls (async-capable)
        RestClientIo::RestClientIo rest;

        #! Immutable chat endpoint selected at construction
        string chat_path;
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest) {
        self.rest = cast<RestClientIo::RestClientIo>(rest.copyWithUrl(rest.getUrl().toString()));
        chat_path = (self.rest.getConnectionPath() ?? "/v1") + "/chat/completions";
        self.rest.clearConnectionPath();
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = cast<RestClientIo::RestClientIo>(
                copts.restclient.copyWithUrl(copts.restclient.getUrl().toString()));
        }
        if (!copts.restclient) {
            if (!copts.apikey.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "missing required 'apikey' argument for DeepSeek API communication";
            }
            hash<auto> opts = (copts.restclient_options ?? {}) + {
                "url": copts.url,
                "api": copts.api,
                "apikey": copts.apikey,
            };
            rest = new DeepSeekRestClient::DeepSeekRestClientIo(opts);
        }
        chat_path = (self.rest.getConnectionPath() ?? "/v1") + "/chat/completions";
        rest.clearConnectionPath();
    }

    #! Returns supported dropdown names
    /** @return model and uploaded-file reference data names */
    *hash<string, bool> getSupportedReferenceData() {
        return {"models": True, "fim-models": True, "files": True};
    }

    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        list<hash<AllowedValueInfo>> rv = ();
        if (type == "models" || type == "fim-models") {
            foreach hash<auto> model in (doRestCommand("GET", "/v1/models").body.data) {
                if (model.id && (type != "fim-models" || model.id == "deepseek-v4-pro")) {
                    push rv, <AllowedValueInfo>{"value": model.id, "display_name": model.id};
                }
            }
            return rv;
        }
        if (type != "files") {
            return;
        }
        string path = "/v1/files?limit=1000";
        hash<string, bool> cursors;
        while (True) {
            hash<auto> page = doRestCommand("GET", path).body;
            foreach hash<auto> file in (page.data) {
                if (file.id) {
                    push rv, <AllowedValueInfo>{"value": file.id,
                        "display_name": (file.filename ?? file.id) + " (" + file.id + ")"};
                }
            }
            if (!page.has_more) {
                return rv;
            }
            if (!page.last_id || cursors{page.last_id}) {
                throw "DEEPSEEK-RESPONSE-ERROR", "file pagination did not advance";
            }
            cursors{page.last_id} = True;
            path = "/v1/files?limit=1000&after=" + encode_url(page.last_id, True);
        }
    }

    #! Makes one API request; errors propagate without replaying billable requests
    private hash<RestResponseInfo> doRestCommand(string method, string path, auto body, *hash<auto> hdr) {
        return rest.restDoRequest(method, path, body, hdr);
    }

    #! Uploads a named image using the API multipart contract
    private hash<auto> uploadFile(hash<auto> req) {
        hash<auto> file = req.file;
        if (!trim(file.name) || file.name =~ /[\r\n]/) {
            throw "DEEPSEEK-REQUEST-ERROR", "file name must be non-empty without line breaks";
        }
        binary content = file.content.typeCode() == NT_BINARY
            ? file.content : parse_base64_string(file.content);
        if (!content.size() || content.size() > 67108864) {
            throw "DEEPSEEK-REQUEST-ERROR", "image content must be between 1 byte and 64 MiB";
        }
        *string mime = file.mime_type;
        if (!mime || mime == "auto") {
            mime = get_mime_type_from_ext(file.name) ?? "application/octet-stream";
        }
        if (mime =~ /[\r\n]/) {
            throw "DEEPSEEK-REQUEST-ERROR", "file MIME type must not contain line breaks";
        }
        Mime::MultiPartFormDataMessage msg();
        msg.addPart(content, "file", file.name, mime);
        msg.addPart(req.purpose, "purpose");
        if (exists req.expires_after_seconds) {
            if (req.expires_after_seconds < 3600 || req.expires_after_seconds > 2592000) {
                throw "DEEPSEEK-REQUEST-ERROR", "file lifetime must be between 3600 and 2592000 seconds";
            }
            msg.addPart("created_at", "expires_after[anchor]");
            msg.addPart(req.expires_after_seconds.toString(), "expires_after[seconds]");
        }
        hash<MessageInfo> info = msg.getMsgAndHeaders();
        return rest.restDoRawRequestTarget("POST", <RestClient::RestRequestTarget>{
            "mode": RestClient::RestPathMode::OriginAbsolute, "path": "/v1/files"}, info.body, info.hdr).body;
    }
}
}
