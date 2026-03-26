# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GeminiDataProvider module definition

/** GeminiDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GeminiDataProvider module
public namespace GeminiDataProvider {
#! The Gemini data provider common base class
public class GeminiDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "Gemini API Key",
                "short_desc": "The API key required to communicate with Google Gemini servers",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The API key required to communicate with Google Gemini servers",
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(new Type("RestClientIo::RestClientIo"), NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "The RestClientIo object to use for API calls",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "The options to use when creating a REST client",
                "type": AbstractDataProviderType::get(AutoHashType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Options to the GeminiRestClientIo constructor; only used if a REST client object is "
                    "created for a call",
            },
        };

        #! IO errors for REST retries
        const RetrySet = {
            "SOCKET-SSL-ERROR": True,
            "SOCKET-CLOSED": True,
        };

        #! Max retries
        const MaxIoRetries = 5;
    }

    private {
        #! The REST client object for API calls
        *RestClientIo::RestClientIo rest;
    }

    #! Creates the object from the REST client
    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        }

        if (!rest) {
            if (!copts.apikey) {
                throw "CONSTRUCTOR-ERROR", "missing 'apikey' argument for Gemini API communication";
            }
            hash<auto> opts;
            opts += copts.restclient_options + {
                "apikey": copts.apikey,
            };
            rest = new GeminiRestClient::GeminiRestClientIo(opts);
        }
    }

    #! Returns the REST client object
    *RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "models": True,
            "files": True,
            "cached_contents": True,
            "file_search_stores": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "models": return getReferenceModels();
            case "files": return getReferenceFiles();
            case "cached_contents": return getReferenceCachedContents();
            case "file_search_stores": return getReferenceFileSearchStores();
        }
    }

    #! Returns available models
    private *list<hash<AllowedValueInfo>> getReferenceModels() {
        list<hash<AllowedValueInfo>> rv;
        foreach hash<auto> model in (doRestCommand("GET", "models").body."models") {
            string desc = model.description ?? model.displayName ?? model.name;
            rv += <AllowedValueInfo>{
                "display_name": model.displayName ?? model.name,
                "short_desc": desc,
                "desc": desc,
                "value": model.name =~ /^models\// ? model.name.substr(7) : model.name,
            };
        }
        return rv;
    }

    #! Returns available files
    private *list<hash<AllowedValueInfo>> getReferenceFiles() {
        list<hash<AllowedValueInfo>> rv;
        *list<auto> files = doRestCommand("GET", "files").body."files";
        foreach hash<auto> file in (files) {
            string name = file.name;
            string display = file.displayName ?? name;
            rv += <AllowedValueInfo>{
                "display_name": display,
                "short_desc": file.mimeType ?? display,
                "value": string(name),
            };
        }
        return rv;
    }

    #! Returns available cached contents
    private *list<hash<AllowedValueInfo>> getReferenceCachedContents() {
        list<hash<AllowedValueInfo>> rv;
        *list<auto> items = doRestCommand("GET", "cachedContents").body."cachedContents";
        foreach hash<auto> item in (items) {
            string name = item.name;
            string display = item.displayName ?? name;
            rv += <AllowedValueInfo>{
                "display_name": display,
                "short_desc": item.model ?? display,
                "value": string(name),
            };
        }
        return rv;
    }

    #! Returns available file search stores
    private *list<hash<AllowedValueInfo>> getReferenceFileSearchStores() {
        list<hash<AllowedValueInfo>> rv;
        *list<auto> stores = doRestCommand("GET", "fileSearchStores").body."fileSearchStores";
        foreach hash<auto> store in (stores) {
            string name = store.name;
            string short_name = name =~ /^fileSearchStores\// ? name.substr(17) : name;
            string display = store.displayName ?? short_name;
            rv += <AllowedValueInfo>{
                "display_name": display,
                "short_desc": display,
                "value": string(name),
            };
        }
        return rv;
    }

    #! Uploads a file using the Gemini resumable upload protocol and returns the file info
    /** @param display_name the display name for the file
        @param mime_type the MIME type of the file
        @param base64_content the file content as a base64-encoded string

        @return the file info hash from the Gemini API
    */
    private hash<auto> uploadFile(string display_name, string mime_type, string base64_content) {
        binary file_bytes = parse_base64_string(base64_content);
        int num_bytes = file_bytes.size();

        # Get default headers (includes API key)
        hash<auto> default_hdr = rest.getDefaultHeaders();

        HttpClientIo::HttpClientConnectionManager mgr = rest.getConnectionManager();

        # Phase 1: Initiate resumable upload
        hash<auto> phase1_hdr = default_hdr + {
            "content-type": "application/json",
            "x-goog-upload-protocol": "resumable",
            "x-goog-upload-command": "start",
            "x-goog-upload-header-content-length": string(num_bytes),
            "x-goog-upload-header-content-type": mime_type,
        };
        string metadata = make_json({"file": {"display_name": display_name}});
        HttpClientIo::HttpClientStreamHandle stream1 = mgr.acquireStream(rest.getUrl());
        hash<auto> phase1_resp = stream1.request("POST", "/upload/v1beta/files", phase1_hdr, metadata);
        if (phase1_resp.status_code != 200) {
            throw "UPLOAD-ERROR", sprintf("upload initiation failed with HTTP %d: %s",
                phase1_resp.status_code, phase1_resp.body);
        }

        # Get upload URL from response headers
        *string upload_url = phase1_resp.headers."x-goog-upload-url";
        if (!upload_url) {
            throw "UPLOAD-ERROR", "no X-Goog-Upload-URL in upload initiation response";
        }

        # Phase 2: Upload file bytes to the upload URL
        hash<auto> url_parts = parse_url(upload_url);
        string upload_path = url_parts.path;
        if (url_parts.query) {
            upload_path += "?" + url_parts.query;
        }
        hash<auto> phase2_hdr = default_hdr + {
            "content-length": string(num_bytes),
            "x-goog-upload-offset": "0",
            "x-goog-upload-command": "upload, finalize",
            "content-type": mime_type,
        };
        HttpClientIo::HttpClientStreamHandle stream2 = mgr.acquireStream(rest.getUrl());
        hash<auto> phase2_resp = stream2.request("PUT", upload_path, phase2_hdr, file_bytes);
        if (phase2_resp.status_code != 200) {
            throw "UPLOAD-ERROR", sprintf("file upload failed with HTTP %d: %s",
                phase2_resp.status_code, phase2_resp.body);
        }

        # Parse JSON response
        hash<auto> result = parse_json(phase2_resp.body);
        return result."file" ?? result;
    }

    #! Builds a model-qualified API path
    /** @param model the model name (e.g., "gemini-2.5-flash")
        @param action the action name (e.g., "generateContent")

        @return the full path (e.g., "models/gemini-2.5-flash:generateContent")
    */
    static string buildModelPath(string model, string action) {
        return "models/" + model + ":" + action;
    }

    #! Makes a REST call and returns the response; handles rate limit responses and retries
    private hash<auto> doRestCommand(string method, string path, auto body = NOTHING, *hash<auto> hdr) {
        int retries = 0;
        while (True) {
            try {
                debug("REST %y %y body: %y", method, path, body);
                switch (method) {
                    case "GET": return rest.restGet(path, hdr);
                    case "POST": return rest.restPost(path, body, hdr);
                    case "PUT": return rest.restPut(path, body, hdr);
                    case "PATCH": return rest.restPatch(path, body, hdr);
                    case "DELETE": return rest.restDel(path, body, hdr);
                    default: return rest.restDoRequest(method, path, body, hdr);
                }
            } catch (hash<ExceptionInfo> ex) {
                if (retry(ex, \retries)) {
                    continue;
                }
                rethrow;
            }
        }
    }

    #! Returns True if the error indicates that the operation should be retried
    private bool retry(hash<ExceptionInfo> ex, reference<int> retries) {
        if (RetrySet{ex.err}) {
            if (++retries == MaxIoRetries) {
                error("Maximum retries exceeded (%d)", retries);
                return False;
            }
            warn("Technical error making REST call: %s: %s; retrying", ex.err, ex.desc);
            return True;
        }
        # Handle rate limit (429)
        if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.status_code == 429) {
            if (++retries == MaxIoRetries) {
                error("Maximum retries exceeded (%d)", retries);
                return False;
            }
            # Wait before retrying on rate limit
            int wait_ms = 1000 * retries;
            warn("Rate limit exceeded; retrying in %d ms", wait_ms);
            usleep(wait_ms * 1000);
            return True;
        }
        return False;
    }
}
}
