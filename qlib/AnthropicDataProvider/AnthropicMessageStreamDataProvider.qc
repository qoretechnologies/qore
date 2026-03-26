# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AnthropicDataProvider module definition

/** AnthropicMessageStreamDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

# for parsing stream events
%try-module json
%define NoJson
%endtry

#! contains all public definitions in the AnthropicDataProvider module
public namespace AnthropicDataProvider {
#! Anthropic message stream data provider using Server-Sent Events
/** This provider uses the observer pattern for event delivery. It emits SSE events
    as they arrive from the Anthropic Messages API with \c stream: \c True.

    @par Usage:
    @code{.py}
AnthropicMessageStreamDataProvider stream(rest);
stream.registerObserver(myObserver);
stream.observersReady();
stream.doRequest({
    "model": "claude-sonnet-4-6-20250514",
    "max_tokens": 1024,
    "messages": ({"role": "user", "content": "Hello!"},),
});
    @endcode
*/
public class AnthropicMessageStreamDataProvider inherits AnthropicDataProviderCommon,
        DataProvider::DelayedObservable {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "stream",
            "type": "AnthropicMessageStreamDataProvider",
            "supports_observable": True,
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = AnthropicMessageRequestDataType;

        #! Response type
        const ResponseType = AnthropicStreamEventDataType;
    }

    private {
        #! Stream request
        *hash<auto> stream_req;

        #! Stream connection
        *object stream;

        #! Stop flag
        bool stop_flag;

        #! Start flag
        bool started;

        #! Observers ready flag
        bool ready;

        #! Thread WaitGroup
        WaitGroup cnt();

        #! Thread lock
        Mutex m();
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {
    }

    #! Destroys the object
    destructor() {
        stop();
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Called when all observers have been added to the object
    observersReady() {
        ready = True;
        if (stream_req && !started) {
            start();
        }
    }

    #! Called to stop generating events
    stopEvents() {
        stop();
    }

    #! Returns the description of a successful request message, if any
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the event types supported by this provider
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_ANTHROPIC_MESSAGE_STREAM: <DataProviderMessageInfo>{
                "desc": "Fires for each SSE event from the Anthropic Messages streaming API",
                "type": AnthropicStreamEventDataType,
            },
        };
    }

    #! Returns example event data for the given event
    private auto getExampleEventDataImpl(string event_id) {
        try {
            # try a minimal real API call to get representative data
            *RestClientIo::RestClientIo rest_client = getRestClient();
            if (rest_client) {
                hash<auto> resp = rest_client.restPost("messages", {
                    "model": "claude-haiku-4-5-20241022",
                    "max_tokens": 16,
                    "messages": ({"role": "user", "content": "Say hi"},),
                });
                if (resp.body) {
                    return resp.body + {
                        "event": "message_start",
                    };
                }
            }
        } catch () {
            # fall through to fake data
        }
        return getFakeExampleData();
    }

    #! Returns fake example data for stream events
    private hash<auto> getFakeExampleData() {
        return {
            "type": "content_block_delta",
            "index": 0,
            "delta": {
                "type": "text_delta",
                "text": "Hello, world!",
            },
            "event": "content_block_delta",
        };
    }

    #! Starts streaming with the given request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        stream_req = req;
        if (ready && !started) {
            start();
        }
        return {
            "status": "streaming",
        };
    }

    #! Starts the SSE thread
    private start() {
        if (started) {
            return;
        }
        started = True;
        cnt.add();
        on_error cnt.done();
        background eventLoop();
    }

    #! Stops the SSE thread
    private stop() {
        m.lock();
        on_exit m.unlock();
        if (!started) {
            return;
        }
        stop_flag = True;
        if (stream) {
            stream.close();
        }
        cnt.wait();
    }

    #! SSE event loop
    private eventLoop() {
        on_exit {
            cnt.done();
        }
        try {
            hash<auto> ctx_info = {};
            *AnthropicConversationContextStore ctx_store = getConversationContextStore();
            *hash<auto> req = AnthropicMessageCreateDataProvider::prepareRequest(stream_req, ctx_store, \ctx_info);
            req.stream = True;

            # remove conversation_key if still present (not part of API)
            delete req.conversation_key;

            *RestClientIo::RestClientIo rest_client = getRestClient();
            if (!rest_client) {
                throw "SSE-ERROR", "no REST client available for streaming";
            }
            stream = rest_client.openMessagesStream(req, ctx_info.extra_headers);

            # accumulated message for context saving
            *hash<auto> full_message;

            while (!stop_flag) {
                *hash<SseMessageInfo> evt = stream.readEvent(1s);
                if (!evt) {
                    if (stream.isDone()) {
                        break;
                    }
                    continue;
                }
                hash<auto> payload = parseEventData(evt);
                if (payload) {
                    notifyObservers(EVENT_ANTHROPIC_MESSAGE_STREAM, payload);
                    # track message for context persistence
                    if (payload.type == "message_start" && payload.message) {
                        full_message = payload.message;
                    }
                    if (payload.type == "message_stop") {
                        handleContextSave(full_message, ctx_info, ctx_store, req);
                        break;
                    }
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            try {
                notifyObservers(EVENT_ANTHROPIC_MESSAGE_STREAM, {
                    "type": "error",
                    "error": ex,
                });
            } catch (hash<ExceptionInfo> ex2) {
            }
        }
    }

    private hash<auto> parseEventData(hash<SseMessageInfo> evt) {
        if (!evt."data") {
            return {"event": evt.event, "id": evt.id};
        }
%ifdef NoJson
        return {
            "event": evt.event,
            "id": evt.id,
            "raw_data": evt."data",
        };
%else
        hash<auto> payload = parse_json(evt."data");
        if (evt.event) {
            payload.event = evt.event;
        }
        if (evt.id) {
            payload.id = evt.id;
        }
        return payload;
%endif
    }

    private handleContextSave(*hash<auto> full_message, hash<auto> ctx_info,
            *AnthropicConversationContextStore ctx_store, hash<auto> req) {
        if (!ctx_store || !ctx_info.ctx_key) {
            return;
        }
        # build context from messages + assistant response
        hash<auto> new_ctx = {
            "messages": (ctx_info.messages ?? ()) + req.messages + (
                {"role": "assistant", "content": full_message.content ?? ()},
            ),
        };
        if (req.system) {
            new_ctx.system = req.system;
        }
        ctx_store.saveContext(ctx_info.ctx_key, new_ctx);
    }
}
}
