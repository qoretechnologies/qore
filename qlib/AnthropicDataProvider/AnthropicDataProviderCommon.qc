# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AnthropicDataProvider module definition

/** AnthropicDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AnthropicDataProvider module
public namespace AnthropicDataProvider {
#! Conversation context store for persisting message context
/** Implementations must return and accept a hash compatible with:
        - \c messages (list, required) - message history
        - \c system (string, optional) - system prompt
        - \c metadata (hash, optional)
    The returned hash is used to prepend previous messages for multi-turn conversations.

    @par Example: subclass implementation
    @code{.py}
class MyContextStore inherits AnthropicConversationContextStore {
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
AnthropicConversationContextStoreCallbacks store(
    *hash<auto> sub (string key) { return get_from_store(key); },
    nothing sub (string key, hash<auto> ctxt) { save_to_store(key, ctxt); }
);
    @endcode
*/
public class AnthropicConversationContextStore {
    #! Loads conversation context for the given key
    /** @param key an opaque identifier for the conversation (for example user id, session id, or workflow id)

        @return a context hash or @ref nothing if no context is available

        @note the returned hash should include at least \c messages
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
public class AnthropicConversationContextStoreCallbacks inherits AnthropicConversationContextStore {
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

#! The Anthropic data provider common base class
/** Uses RestClientIo for async-capable multiplexed HTTP connections.
*/
public class AnthropicDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        #! The default Anthropic URL
        const DefaultAnthropicUrl = "https://api.anthropic.com/";

        #! The default Anthropic API version
        const DefaultAnthropicApiVersion = "v1";

        #! Constructor options
        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "Anthropic API Key",
                "short_desc": "The API key required to communicate with Anthropic servers",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Anthropic API key used for authentication; sent as the `x-api-key` header",
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
                "desc": "Options to the RestClientIo constructor; only used if a RestClientIo object is created for "
                    "a call",
            },
            "api": <DataProviderOptionInfo>{
                "display_name": "API Version",
                "short_desc": "The API version to use",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The API version to use",
                "default_value": DefaultAnthropicApiVersion,
            },
            "conversation_context_store": <DataProviderOptionInfo>{
                "display_name": "Conversation Context Store",
                "short_desc": "Optional store for persisting message conversation context",
                "type": AbstractDataProviderType::get(
                    new Type("AnthropicConversationContextStore"), NOTHING, {
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
        #! The REST client object for API calls (async-capable)
        *RestClientIo::RestClientIo rest;

        #! Optional conversation context store
        *AnthropicConversationContextStore conversation_context_store;
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest) {
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
                throw "CONSTRUCTOR-ERROR", "missing 'apikey' argument for Anthropic API communication";
            }
            hash<auto> opts;
            opts += copts.restclient_options + {
                "api": copts.api,
                "apikey": copts.apikey,
            };
            rest = new AnthropicRestClient::AnthropicRestClientIo(opts);
        }
    }

    #! Sets the conversation context store
    setConversationContextStore(*AnthropicConversationContextStore store) {
        conversation_context_store = store;
    }

    #! Returns the conversation context store
    *AnthropicConversationContextStore getConversationContextStore() {
        return conversation_context_store;
    }

    #! Returns the REST client object
    *RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        # RestClientIo does not support setLogger() - logger set via constructor option only
        LoggerWrapper::setLogger(logger);
    }

    #! Returns information on supported reference data
    /** @return a hash of supported reference data; keys in the hash returned are supported in calls to
        \c getReferenceData()

        @since DataProvider 3.0
    */
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "models": True,
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
        }
    }

    #! Returns available models
    private *list<hash<AllowedValueInfo>> getReferenceModels() {
        list<hash<AllowedValueInfo>> rv;
        foreach hash<auto> model in (doRestCommand("GET", "models").body."data") {
            string desc = model.display_name ?? model.id;
            rv += <AllowedValueInfo>{
                "display_name": model.display_name ?? model.id,
                "short_desc": desc,
                "desc": desc,
                "value": model.id,
            };
        }
        return rv;
    }

    #! Makes a REST call and returns the response; handles rate limit and I/O errors
    private hash<auto> doRestCommand(string method, string path, auto body, *hash<auto> hdr) {
        int retries = 0;
        while (True) {
            try {
                debug("REST %y %y body: %y", method, path, body);
                switch (method.upr()) {
                    case "GET":
                        return rest.restGet(path);
                    case "POST":
                        return rest.restPost(path, body);
                    case "PUT":
                        return rest.restPut(path, body);
                    case "PATCH":
                        return rest.restPatch(path, body);
                    case "DELETE":
                        return rest.restDel(path);
                    default:
                        return rest.restDoRequest(method, path, body);
                }
            } catch (hash<ExceptionInfo> ex) {
                if (retry(ex, \retries)) {
                    continue;
                }
                rethrow ex.err, sprintf("%s (request path: %y)", ex.desc,
                    ex.arg.request_path ?? path), ex.arg;
            }
        }
    }

    #! Returns True if the error indicates that the operation should be retried
    private bool retry(hash<ExceptionInfo> ex, reference<int> retries) {
        # retry on I/O errors
        if (RetrySet{ex.err}) {
            if (++retries == MaxIoRetries) {
                error("Maximum retries exceeded (%d)", retries);
                return False;
            }
            warn("Technical error making REST call: %s: %s; retrying", ex.err, ex.desc);
            return True;
        }
        # retry on rate limit (429)
        if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.status_code == 429) {
            if (++retries == MaxIoRetries) {
                error("Maximum retries exceeded (%d)", retries);
                return False;
            }
            # use retry-after header if available
            *string retry_after = ex.arg.headers."retry-after";
            int wait_secs = retry_after.val() ? retry_after.toInt() : (1 << retries);
            warn("Rate limited (429); waiting %d second(s) before retry %d", wait_secs, retries);
            sleep(wait_secs);
            return True;
        }
        return False;
    }
}
}
