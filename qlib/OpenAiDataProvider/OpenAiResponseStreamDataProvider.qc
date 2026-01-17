# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAiDataProvider module definition

/** OpenAiResponseStreamDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the OpenAiDataProvider module
public namespace OpenAiDataProvider {
#! Response stream event constant
public const EVENT_OPENAI_RESPONSE_STREAM = "openai-response-stream-event";

#! Response stream event type
public class OpenAiResponseStreamEventDataType inherits HashDataType {
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
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

public const OpenAiResponseStreamEventDataType = new OpenAiResponseStreamEventDataType();

#! Response stream data provider
public class OpenAiResponseStreamDataProvider inherits OpenAiDataProviderCommon, DataProvider::DelayedObservable {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "stream",
            "type": "OpenAiResponseStreamDataProvider",
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
        const RequestType = OpenAiModelCreateResponseRequestDataType;

        #! Response type
        const ResponseType = OpenAiResponseStreamEventDataType;
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

        #! Thread counter
        Counter cnt();

        #! Thread lock
        Mutex m();
    }

    #! Creates the object from the arguments
    constructor(*RestClient rest) : OpenAiDataProviderCommon(rest) {
    }

    #! Creates the object from constructor options
    constructor(hash<auto> options) : OpenAiDataProviderCommon(options) {
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
        cnt.inc();
        on_error cnt.dec();
%ifdef PO_NO_THREAD_CONTROL
        background eventLoop();
%else
        background eventLoop();
%endif
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
        cnt.waitForZero();
    }

    #! SSE event loop
    private eventLoop() {
        on_exit {
            cnt.dec();
        }
        hash<auto> ctx_info = {};
        *OpenAiConversationContextStore ctx_store = getConversationContextStore();
        *hash<auto> req = OpenAiModelCreateResponseDataProvider::prepareRequest(stream_req, NOTHING,
            ctx_store, \ctx_info);
        req.stream = True;
        *OpenAiRestClient::OpenAiRestClient rest = getRestClient();
        if (!(rest instanceof OpenAiRestClient::OpenAiRestClient)) {
            throw "SSE-ERROR", "response streaming requires OpenAiRestClient";
        }
        stream = rest.openResponsesStream(req);

        while (!stop_flag) {
            *hash<SseMessageInfo> evt = stream.readEvent(1s);
            if (!evt) {
                break;
            }
            if (evt.data == "[DONE]") {
                notifyObservers(EVENT_OPENAI_RESPONSE_STREAM, {"type": "done"});
                break;
            }
            hash<auto> payload = parseEventData(evt);
            if (payload) {
                notifyObservers(EVENT_OPENAI_RESPONSE_STREAM, payload);
                handleContextSave(payload, ctx_info, ctx_store, req.metadata);
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

    private handleContextSave(hash<auto> payload, hash<auto> ctx_info, *OpenAiConversationContextStore ctx_store,
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
        hash<auto> ctx_data = {
            "previous_response_id": payload.response.id,
            "conversation_id": payload.response.conversation_id ?? ctx.conversation_id,
            "metadata": ctx.metadata ?? metadata,
        };
        ctx_store.saveContext(ctx_info.ctx_key, ctx_data);
    }
}
}
