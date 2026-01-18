# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAiDataProvider module definition

/** OpenAiDataProviderCommon.qc Copyright 2024 - 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the OpenAiDataProvider module
public namespace OpenAiDataProvider {
#! Conversation context store for persisting response context
/** Implementations must return and accept a hash compatible with:
        - \c previous_response_id (string, required)
        - \c conversation_id (string, optional)
        - \c metadata (hash, optional)
    The returned hash is used to set \c previous_response_id and to persist state for follow-up requests.

    @par Example: subclass implementation
    @code{.py}
class MyContextStore inherits OpenAiConversationContextStore {
    *hash<auto> loadContext(string key) {
        return get_from_store(key);
    }

    nothing saveContext(string key, hash<auto> ctxt) {
        save_to_store(key, ctxt);
    }
}
    @endcode

    @par Example: callback-based store
    @code{.py}
OpenAiConversationContextStoreCallbacks store(
    *hash<auto> sub (string key) { return get_from_store(key); },
    nothing sub (string key, hash<auto> ctxt) { save_to_store(key, ctxt); }
);
    @endcode
*/
public class OpenAiConversationContextStore {
    #! Loads conversation context for the given key
    /** @param key an opaque identifier for the conversation (for example user id, session id, or workflow id)

        @return a context hash or @ref nothing if no context is available

        @note the returned hash should include at least \c previous_response_id
    */
    abstract *hash<auto> loadContext(string key);

    #! Saves conversation context for the given key
    /** @param key an opaque identifier for the conversation
        @param ctxt the context hash to persist (see class description for supported keys)

        @return nothing
    */
    abstract nothing saveContext(string key, hash<auto> ctxt);
}

#! Conversation context store with callbacks
public class OpenAiConversationContextStoreCallbacks inherits OpenAiConversationContextStore {
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

#! The OpenAi data provider common base class
public class OpenAiDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        #! The default OpenAi URL
        const DefaultOpenAiUrl = "https://api.openai.com/";

        #! The default OpenAi API version
        const DefaultOpenAiApiVersion = "v1";

        #! Constructor options
        const ConstructorOptions = {
            "api_key": <DataProviderOptionInfo>{
                "display_name": "OpenAi API Key",
                "short_desc": "The API key required to communicate with OpenAi servers",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The API key required to communicate with OpenAi servers",
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(new Type("RestClient"), NOTHING, {
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
                "default_value": DefaultOpenAiApiVersion,
            },
            "conversation_context_store": <DataProviderOptionInfo>{
                "display_name": "Conversation Context Store",
                "short_desc": "Optional store for persisting response conversation context",
                "type": AbstractDataProviderType::get(new Type("OpenAiConversationContextStore"), NOTHING, {
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

        #! Headers required for the OpenAI assistant API
        const AssistantApiHdr = {
            "OpenAI-Beta": "assistants=v2",
        };
    }

    private {
        #! The REST client object for API calls
        *RestClient::RestClient rest;

        #! Optional conversation context store
        *OpenAiConversationContextStore conversation_context_store;

        #! first time initialization
        static bool init;

        #! lock for first time initialization
        static Mutex m();
    }

    #! Creates the object
    constructor(RestClient::RestClient rest) {
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
            if (!copts.api_key) {
                throw "CONSTRUCTOR-ERROR", "missing 'api_key' argument for OpenAI API communication";
            }
            hash<auto> opts;
            opts += copts.restclient_options + {
                "api": copts.api_version,
                "token": copts.api_key,
            };
            rest = new OpenAiRestClient(opts);
        }
    }

    #! Sets the conversation context store
    setConversationContextStore(*OpenAiConversationContextStore store) {
        conversation_context_store = store;
    }

    #! Returns the conversation context store
    *OpenAiConversationContextStore getConversationContextStore() {
        return conversation_context_store;
    }

    #! Returns the REST client object
    *RestClient::RestClient getRestClient() {
        return rest;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        if (rest) {
            rest.setLogger(logger);
        }
        LoggerWrapper::setLogger(logger);
    }

    #! Returns information on supported reference data
    /** @return a hash of supported reference data; keys in the hash returned are supported in calls to
        @ref getReferenceData()

        @since DataProvider 3.0
    */
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "models": True,
            "assistants": True,
        };
    }

    #! Returns reference data of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data

        @since DataProvider 3.0
    */
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "models": return getReferenceModels();
            case "assistants": return getReferenceAssistants();
        }
    }

    #! Returns available models
    private *list<hash<AllowedValueInfo>> getReferenceModels() {
        list<hash<AllowedValueInfo>> rv;
        foreach hash<auto> model in (doRestCommand("GET", "models").body."data") {
            string desc = sprintf("Model created on %y, owned by %y", localtime(model.created), model.owned_by);
            rv += <AllowedValueInfo>{
                "display_name": model.id,
                "short_desc": desc,
                "desc": desc,
                "value": model.id,
            };
        }
        return rv;
    }

    #! Returns available assistants
    private *list<hash<AllowedValueInfo>> getReferenceAssistants() {
        list<hash<AllowedValueInfo>> rv;
        foreach hash<auto> assistant in (doRestCommand("GET", "assistants", NOTHING, AssistantApiHdr).body."data") {
            string desc = assistant.desc ?? "no description available";
            rv += <AllowedValueInfo>{
                "display_name": assistant.id,
                "short_desc": desc,
                "desc": desc,
                "value": assistant.id,
            };
        }
        return rv;
    }

    #! Makes a REST call and returns the response; handles rate limit responses
    private hash<auto> doRestCommand(string method, string path, auto body, *hash<auto> hdr,
            *reference<hash<auto>> info) {
        int retries = 0;
        while (True) {
            try {
                debug("REST %y %y (%y) body: %y", method, rest.getURL(), path, body);
                return rest.doRequest(method, path, body, \info, NOTHING, hdr);
            } catch (hash<ExceptionInfo> ex) {
                if (retry(ex, \retries)) {
                    continue;
                }
                rethrow ex.err, sprintf("%s (HTTP response code: %y)", ex.desc, info."response-headers".status_code);
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
