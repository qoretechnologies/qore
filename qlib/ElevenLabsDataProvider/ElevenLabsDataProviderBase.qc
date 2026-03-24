# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ElevenLabsDataProvider module
public namespace ElevenLabsDataProvider {
#! The ElevenLabs data provider common base class
public class ElevenLabsDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "ElevenLabs API Key",
                "short_desc": "The API key required to communicate with ElevenLabs servers",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The API key required to communicate with ElevenLabs servers",
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
                "desc": "Options to the ElevenLabsRestClientIo constructor; only used if a REST client object is "
                    "created for a call",
            },
            "region": <DataProviderOptionInfo>{
                "display_name": "Region",
                "short_desc": "The API region to use",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The API region to use; `us` (default) uses the global endpoint, `eu` uses the EU data "
                    "residency endpoint",
                "default_value": "us",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "US (Global)",
                        "value": "us",
                        "desc": "Use the global API endpoint",
                    },
                    <AllowedValueInfo>{
                        "display_name": "EU",
                        "value": "eu",
                        "desc": "Use the EU data residency endpoint",
                    },
                ),
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
                throw "CONSTRUCTOR-ERROR", "missing 'apikey' argument for ElevenLabs API communication";
            }
            hash<auto> opts;
            opts += copts.restclient_options + {
                "apikey": copts.apikey,
                "region": copts.region,
            };
            rest = new ElevenLabsRestClient::ElevenLabsRestClientIo(opts);
        }
    }

    #! Returns the REST client object
    *RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "voices": True,
            "models": True,
            "dubbing": True,
            "history": True,
            "pronunciation_dictionaries": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "voices": return getReferenceVoices();
            case "models": return getReferenceModels();
            case "dubbing": return getReferenceDubbing();
            case "history": return getReferenceHistory();
            case "pronunciation_dictionaries": return getReferencePronunciationDictionaries();
        }
    }

    #! Returns available voices
    private *list<hash<AllowedValueInfo>> getReferenceVoices() {
        list<hash<AllowedValueInfo>> rv;
        foreach hash<auto> voice in (doRestCommand("GET", "v2/voices").body."voices") {
            string desc = voice.description ?? sprintf("Voice category: %s", voice.category ?? "unknown");
            rv += <AllowedValueInfo>{
                "display_name": voice.name,
                "short_desc": desc,
                "desc": desc,
                "value": string(voice.voice_id),
            };
        }
        return rv;
    }

    #! Returns available models
    private *list<hash<AllowedValueInfo>> getReferenceModels() {
        list<hash<AllowedValueInfo>> rv;
        foreach hash<auto> model in (doRestCommand("GET", "v1/models").body) {
            string desc = model.description ?? model.name;
            rv += <AllowedValueInfo>{
                "display_name": model.name,
                "short_desc": desc,
                "desc": desc,
                "value": string(model.model_id),
            };
        }
        return rv;
    }

    #! Returns available dubbing projects
    private *list<hash<AllowedValueInfo>> getReferenceDubbing() {
        list<hash<AllowedValueInfo>> rv;
        foreach hash<auto> dub in (doRestCommand("GET", "v1/dubbing").body."dubs") {
            string display = dub.name ?? dub.dubbing_id;
            string desc = sprintf("Status: %s", dub.status ?? "unknown");
            rv += <AllowedValueInfo>{
                "display_name": display,
                "short_desc": desc,
                "desc": desc,
                "value": string(dub.dubbing_id),
            };
        }
        return rv;
    }

    #! Returns available history items
    private *list<hash<AllowedValueInfo>> getReferenceHistory() {
        list<hash<AllowedValueInfo>> rv;
        foreach hash<auto> item in (doRestCommand("GET", "v1/history").body."history") {
            string display = item.text ?? item.history_item_id;
            if (display.size() > 60) {
                display = display.substr(0, 57) + "...";
            }
            string desc = sprintf("Voice: %s, State: %s", item.voice_name ?? "unknown",
                item.state ?? "unknown");
            rv += <AllowedValueInfo>{
                "display_name": display,
                "short_desc": desc,
                "desc": desc,
                "value": string(item.history_item_id),
            };
        }
        return rv;
    }

    #! Returns available pronunciation dictionaries
    private *list<hash<AllowedValueInfo>> getReferencePronunciationDictionaries() {
        list<hash<AllowedValueInfo>> rv;
        foreach hash<auto> dict in (
                doRestCommand("GET", "v1/pronunciation-dictionaries").body."pronunciation_dictionaries") {
            string desc = dict.description ?? dict.name;
            rv += <AllowedValueInfo>{
                "display_name": dict.name,
                "short_desc": desc,
                "desc": desc,
                "value": string(dict.id),
            };
        }
        return rv;
    }

    #! Processes a raw HTTP response; checks for errors and parses JSON bodies
    private hash<auto> processRawResponse(hash<auto> raw) {
        int status = raw.status_code;
        *string ct = raw.headers."content-type";
        auto body = raw.body;

        # Parse JSON bodies (binary or string)
        if (ct =~ /json/i || (body.typeCode() == NT_STRING && body =~ /^\s*[\[{]/)) {
            if (body.typeCode() == NT_BINARY) {
                body = parse_json(binary_to_string(body));
            } else if (body.typeCode() == NT_STRING) {
                body = parse_json(body);
            }
        }

        # Check for HTTP errors
        if (status >= 400) {
            string msg;
            if (body.typeCode() == NT_HASH) {
                if (body.detail.typeCode() == NT_HASH) {
                    msg = body.detail.message ?? sprintf("API error: %y", body.detail);
                } else if (body.detail.typeCode() == NT_STRING) {
                    msg = body.detail;
                } else {
                    msg = body.message ?? sprintf("API error %d", status);
                }
            } else {
                msg = sprintf("API error %d: %y", status, body);
            }
            throw "ELEVENLABS-API-ERROR", msg, {"status_code": status, "body": body};
        }

        return {
            "body": body,
            "content-type": ct,
            "status_code": status,
            "headers": raw.headers,
        };
    }

    #! Sends a multipart/form-data POST request for endpoints that accept binary file uploads
    /** @param path the API endpoint path
        @param fields a hash of field names to values; binary values are sent as file parts, all others as form fields
        @param filename the filename to use for binary file parts (default: \c "audio.mp3")

        @return a hash with \c body and other response info
    */
    private hash<auto> doMultipartPost(string path, hash<auto> fields, string filename = "audio.mp3") {
        string mpboundary = replace(make_base64_string(string(now_us())), "=", "");

        # Build multipart body
        binary msg = binary();
        foreach hash<auto> field in (fields.pairIterator()) {
            if (!exists field.value) {
                continue;
            }
            if (field.value.typeCode() == NT_HASH) {
                # Serialize hash fields as JSON
                msg += binary(sprintf("--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n"
                    "Content-Type: application/json\r\n\r\n", mpboundary, field.key));
                msg += binary(make_json(field.value));
                msg += binary("\r\n");
            } else if (field.value.typeCode() == NT_BINARY) {
                msg += binary(sprintf("--%s\r\nContent-Disposition: form-data; name=\"%s\"; "
                    "filename=\"%s\"\r\nContent-Type: application/octet-stream\r\n\r\n",
                    mpboundary, field.key, filename));
                msg += field.value;
                msg += binary("\r\n");
            } else if (field.value.typeCode() == NT_LIST) {
                # Send list of binary files
                foreach auto item in (field.value) {
                    if (item.typeCode() == NT_BINARY) {
                        msg += binary(sprintf("--%s\r\nContent-Disposition: form-data; name=\"%s\"; "
                            "filename=\"file%d\"\r\nContent-Type: application/octet-stream\r\n\r\n",
                            mpboundary, field.key, $#));
                        msg += item;
                        msg += binary("\r\n");
                    }
                }
            } else {
                msg += binary(sprintf("--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n",
                    mpboundary, field.key, string(field.value)));
            }
        }
        msg += binary(sprintf("--%s--\r\n", mpboundary));

        # Build headers from rest client defaults + multipart content type
        hash<auto> hdr = rest.getDefaultHeaders() + {
            "content-type": sprintf("%s; boundary=%s", MimeTypeMultipartFormData, mpboundary),
        };

        # Normalize path
        if (path[0] != "/") {
            path = "/" + path;
        }

        int retries = 0;
        while (True) {
            try {
                debug("REST multipart POST %y body size: %d", path, msg.size());
                hash<auto> raw = rest.getConnectionManager().request(rest.getUrl(), "POST", path,
                    hdr, msg);
                return processRawResponse(raw);
            } catch (hash<ExceptionInfo> ex) {
                if (retry(ex, \retries)) {
                    continue;
                }
                rethrow;
            }
        }
    }

    #! Sends a JSON POST and returns the raw binary response (for endpoints returning audio)
    /** @param path the API endpoint path
        @param body the request body (will be serialized as JSON)

        @return a hash with \c body (binary), \c content-type, \c status_code, and \c headers
    */
    private hash<auto> doRawPost(string path, auto body) {
        # Normalize path
        if (path[0] != "/") {
            path = "/" + path;
        }

        hash<auto> hdr = rest.getDefaultHeaders() + {
            "content-type": MimeTypeJson,
            "accept": "*/*",
        };

        binary json_body = binary(make_json(body));

        int retries = 0;
        while (True) {
            try {
                debug("REST raw POST %y body size: %d", path, json_body.size());
                hash<auto> raw = rest.getConnectionManager().request(rest.getUrl(), "POST", path,
                    hdr, json_body);
                return processRawResponse(raw);
            } catch (hash<ExceptionInfo> ex) {
                if (retry(ex, \retries)) {
                    continue;
                }
                rethrow;
            }
        }
    }

    #! Makes a REST call and returns the response; handles rate limit responses and retries
    private hash<auto> doRestCommand(string method, string path, auto body, *hash<auto> hdr) {
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
