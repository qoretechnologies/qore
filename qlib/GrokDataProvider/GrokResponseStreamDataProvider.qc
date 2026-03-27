# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GrokDataProvider module definition

/** GrokResponseStreamDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

# for parsing response stream events
%try-module json
%define NoJson
%endtry

#! contains all public definitions in the GrokDataProvider module
public namespace GrokDataProvider {
#! Response stream event constant
public const EVENT_GROK_RESPONSE_STREAM = "grok-response-stream-event";

#! Response stream event type
public class GrokResponseStreamEventDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Event Type",
                "short_desc": "The response stream event type",
                "desc": "The response stream event type",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "id": {
                "display_name": "Event ID",
                "short_desc": "SSE event ID",
                "desc": "SSE event ID",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "event": {
                "display_name": "Event Name",
                "short_desc": "SSE event name",
                "desc": "SSE event name",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "raw_data": {
                "display_name": "Raw Data",
                "short_desc": "Raw event data",
                "desc": "Raw event data when JSON parsing is not possible",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "error": {
                "display_name": "Error",
                "short_desc": "Error payload for stream failures",
                "desc": "Exception info when a stream error occurs",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

public const GrokResponseStreamEventDataType = new GrokResponseStreamEventDataType();

#! Response stream data provider - SSE streaming for the Responses API
public class GrokResponseStreamDataProvider inherits GrokDataProviderCommon, DataProvider::DelayedObservable {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "stream",
            "type": "GrokResponseStreamDataProvider",
            "supports_observable": True,
            "supports_connections": True,
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = GrokResponseCreateRequestType;

        #! Response type
        const ResponseType = GrokResponseStreamEventDataType;
    }

    private {
        #! Stream request
        *hash<auto> stream_req;

        #! Stream connection
        *object stream;

        #! Model info
        *hash<auto> model;

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

    #! Creates the object from the arguments
    constructor(*RestClientIo::RestClientIo rest) : GrokDataProviderCommon(rest) {
    }

    #! Creates the object with model info
    constructor(*RestClientIo::RestClientIo rest, hash<auto> model) : GrokDataProviderCommon(rest) {
        self.model = model;
    }

    #! Creates the object from constructor options
    constructor(hash<auto> options) : GrokDataProviderCommon(options) {
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

    #! Returns event type info
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_GROK_RESPONSE_STREAM: <DataProviderMessageInfo>{
                "desc": "Fires for each SSE event from the Grok responses stream",
                "type": GrokResponseStreamEventDataType,
            },
        };
    }

    #! Returns example event data
    private auto getExampleEventDataImpl(string event_id) {
        try {
            *RestClientIo::RestClientIo rest_client = getRestClient();
            if (rest_client) {
                hash<auto> result = doRestCommand("POST", "responses", {
                    "model": model.id ?? "grok-3",
                    "input": "Reply with exactly one word: hello",
                    "max_output_tokens": 50,
                }).body;
                if (result) {
                    return {
                        "type": "response.completed",
                        "response": result,
                    };
                }
            }
        } catch () {
            # fall through to fake data
        }
        return getFakeExampleData();
    }

    #! Returns fake example data for when real API is unavailable
    private hash<auto> getFakeExampleData() {
        return {
            "type": "response.output_text.delta",
            "response_id": "resp_abc123",
            "item_id": "msg_001",
            "output_index": 0,
            "content_index": 0,
            "delta": {
                "type": "output_text_delta",
                "text": "Hello",
            },
        };
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
        {
            m.lock();
            on_exit m.unlock();
            if (!started) {
                return;
            }
            stop_flag = True;
            if (stream) {
                stream.close();
            }
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
            *GrokConversationContextStore ctx_store = getConversationContextStore();
            *hash<auto> req = GrokModelCreateResponseDataProvider::prepareRequest(stream_req, model,
                ctx_store, \ctx_info);
            req.stream = True;
            *RestClientIo::RestClientIo rest_client = getRestClient();
            if (!(rest_client instanceof GrokRestClient::GrokRestClientIo)) {
                throw "SSE-ERROR", "response streaming requires GrokRestClientIo";
            }
            stream = cast<GrokRestClient::GrokRestClientIo>(rest_client).openResponsesStream(req);

            while (!stop_flag) {
                *hash<SseMessageInfo> evt = stream.readEvent(1s);
                if (!evt) {
                    if (stream.isDone()) {
                        break;
                    }
                    continue;
                }
                if (evt."data" == "[DONE]") {
                    notifyObservers(EVENT_GROK_RESPONSE_STREAM, {"type": "done"});
                    break;
                }
                hash<auto> payload = parseEventData(evt);
                if (payload) {
                    notifyObservers(EVENT_GROK_RESPONSE_STREAM, payload);
                    handleContextSave(payload, ctx_info, ctx_store, req.metadata);
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            try {
                notifyObservers(EVENT_GROK_RESPONSE_STREAM, {
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

    private handleContextSave(hash<auto> payload, hash<auto> ctx_info, *GrokConversationContextStore ctx_store,
            *hash<auto> metadata) {
        if (!ctx_store || !ctx_info.ctx_key) {
            return;
        }
        if (payload.type != "response.completed") {
            return;
        }
        if (!payload.response || !payload.response.id) {
            return;
        }
        hash<auto> ctx = ctx_info.ctx ?? {};
        *hash<auto> effective_metadata = ctx.metadata ?? metadata;
        hash<auto> ctx_data = {
            "previous_response_id": payload.response.id,
            "conversation_id": payload.response.conversation_id ?? ctx.conversation_id,
        };
        if (effective_metadata) {
            ctx_data.metadata = effective_metadata;
        }
        ctx_store.saveContext(ctx_info.ctx_key, ctx_data);
    }
}
}
