# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GrokDataProvider module definition

/** GrokDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GrokDataProvider module
public namespace GrokDataProvider {
#! Conversation context store for persisting response context
/** Implementations must return and accept a hash compatible with:
        - \c previous_response_id (string, required)
        - \c conversation_id (string, optional)
        - \c metadata (hash, optional)
*/
public class GrokConversationContextStore {
    #! Loads conversation context for the given key
    abstract *hash<auto> loadContext(string key);

    #! Saves conversation context for the given key
    abstract nothing saveContext(string key, hash<auto> ctxt);
}

#! Conversation context store with callbacks
public class GrokConversationContextStoreCallbacks inherits GrokConversationContextStore {
    private {
        *code<*hash<auto>(string)> load_cb;
        *code<nothing(string, hash<auto>)> save_cb;
    }

    #! Creates the callback store
    constructor(code<*hash<auto>(string)> load_cb, code<nothing(string, hash<auto>)> save_cb) {
        self.load_cb = load_cb;
        self.save_cb = save_cb;
    }

    #! Loads conversation context for the given key
    *hash<auto> loadContext(string key) {
        if (load_cb) {
            return load_cb(key);
        }
        return;
    }

    #! Saves conversation context for the given key
    nothing saveContext(string key, hash<auto> ctxt) {
        if (save_cb) {
            save_cb(key, ctxt);
        }
    }
}

#! The Grok data provider common base class
public class GrokDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        #! The default Grok URL
        const DefaultGrokUrl = "https://api.x.ai/";

        #! The default Grok API version
        const DefaultGrokApiVersion = "v1";

        #! Constructor options
        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "The xAI Grok API key for authentication",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The xAI Grok API key used for Bearer authentication.\n\n"
                    "Get your API key from the [xAI Console](https://console.x.ai/)",
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(new Type("RestClientIo::RestClientIo"), NOTHING, {
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
                "desc": "Options to the RestClient constructor; only used if a RestClient object is created for a "
                    "call",
            },
            "api": <DataProviderOptionInfo>{
                "display_name": "API Version",
                "short_desc": "The API version to use",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The API version to use",
                "default_value": DefaultGrokApiVersion,
            },
            "conversation_context_store": <DataProviderOptionInfo>{
                "display_name": "Conversation Context Store",
                "short_desc": "Optional store for persisting response conversation context",
                "type": AbstractDataProviderType::get(new Type("GrokConversationContextStore"), NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Optional conversation context store used to load and save conversation state",
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
        #! The REST client object for API calls (RestClientIo for async support)
        *RestClientIo::RestClientIo rest;

        #! Optional conversation context store
        *GrokConversationContextStore conversation_context_store;
    }

    #! Creates the object
    constructor(*RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        }
        if (copts.conversation_context_store) {
            conversation_context_store = copts.conversation_context_store;
        }

        if (!rest) {
            if (!copts.apikey) {
                throw "CONSTRUCTOR-ERROR", "missing 'apikey' argument for Grok API communication";
            }
            hash<auto> opts;
            opts += copts.restclient_options + {
                "api": copts.api,
                "apikey": copts.apikey,
            };
            rest = new GrokRestClient::GrokRestClientIo(opts);
        }
    }

    #! Sets the conversation context store
    setConversationContextStore(*GrokConversationContextStore store) {
        conversation_context_store = store;
    }

    #! Returns the conversation context store
    *GrokConversationContextStore getConversationContextStore() {
        return conversation_context_store;
    }

    #! Returns the REST client object
    *RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        # RestClientIo does not support setLogger(); logger set via constructor option
        LoggerWrapper::setLogger(logger);
    }

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "models": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "models": return getReferenceModels();
        }
    }

    #! Returns available models
    private *list<hash<AllowedValueInfo>> getReferenceModels() {
        list<hash<AllowedValueInfo>> rv;
        foreach hash<auto> model in (doRestCommand("GET", "models").body."data") {
            string desc = sprintf("xAI Grok model: %s", model.id);
            rv += <AllowedValueInfo>{
                "display_name": model.id,
                "short_desc": desc,
                "desc": desc,
                "value": model.id,
            };
        }
        return rv;
    }

    #! Makes a REST call and returns the response; handles rate limit responses
    private hash<auto> doRestCommand(string method, string path, auto body, *hash<auto> hdr) {
        int retries = 0;
        while (True) {
            try {
                debug("REST %y %y (%y) body: %y", method, rest.getSafeUrl(), path, body);
                if (hdr) {
                    return rest.restDoRequest(method, path, body, hdr);
                }
                switch (method) {
                    case "GET": return rest.restGet(path);
                    case "POST": return rest.restPost(path, body);
                    case "PUT": return rest.restPut(path, body);
                    case "PATCH": return rest.restPatch(path, body);
                    case "DELETE": return rest.restDel(path);
                    default: return rest.restDoRequest(method, path, body);
                }
            } catch (hash<ExceptionInfo> ex) {
                if (retry(ex, \retries)) {
                    continue;
                }
                rethrow ex.err, sprintf("%s (HTTP response code: %y)", ex.desc, ex.arg.status_code);
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
        return False;
    }
}
}
