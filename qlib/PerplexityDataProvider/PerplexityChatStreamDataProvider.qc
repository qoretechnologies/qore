# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore PerplexityDataProvider module definition

/** PerplexityChatStreamDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the PerplexityDataProvider module
public namespace PerplexityDataProvider {
#! Chat stream event constant
public const EVENT_PERPLEXITY_CHAT_STREAM = "perplexity-chat-stream-event";

#! Chat stream event type
public class PerplexityChatStreamEventDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Event Type",
                "short_desc": "The stream event type",
                "desc": "The stream event type (e.g., `delta`, `done`, `error`)",
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
    constructor() : HashDataType("PerplexityChatStreamEvent") {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

public const PerplexityChatStreamEventDataType = new PerplexityChatStreamEventDataType();

#! Chat stream data provider
public class PerplexityChatStreamDataProvider inherits PerplexityDataProviderCommon, DataProvider::DelayedObservable {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "stream-chat",
            "type": "PerplexityChatStreamDataProvider",
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
        const RequestType = PerplexityChatCompletionRequestType;

        #! Response type
        const ResponseType = PerplexityChatStreamEventDataType;
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

    #! Creates the object from the arguments
    constructor(*RestClient rest) : PerplexityDataProviderCommon(rest) {
    }

    #! Creates the object from constructor options
    constructor(hash<auto> options) : PerplexityDataProviderCommon(options) {
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

    #! Returns the event types supported by this provider
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_PERPLEXITY_CHAT_STREAM: <DataProviderMessageInfo>{
                "desc": "Server-Sent Event from the Perplexity chat completion stream",
                "type": ResponseType,
            },
        };
    }

    #! Returns example event data for the given event
    private auto getExampleEventDataImpl(string event_id) {
        try {
            *RestClient::RestClient rest_client = getRestClient();
            if (rest_client) {
                hash<auto> resp = rest_client.doRequest("POST", "chat/completions", {
                    "model": "sonar",
                    "messages": ({"role": "user", "content": "Say hi"},),
                    "max_tokens": 16,
                }).body;
                if (resp) {
                    # Transform the non-streaming response into a stream delta shape
                    # so the example matches what observers actually receive
                    *hash<auto> choice = resp.choices[0];
                    return {
                        "id": resp.id,
                        "object": "chat.completion.chunk",
                        "created": resp.created,
                        "model": resp.model,
                        "choices": ({
                            "index": choice.index ?? 0,
                            "delta": choice.message,
                            "finish_reason": choice.finish_reason,
                        },),
                        "type": "delta",
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
            "id": "chatcmpl-example",
            "object": "chat.completion.chunk",
            "created": 1711000000,
            "model": "sonar",
            "choices": ({
                "index": 0,
                "delta": {
                    "role": "assistant",
                    "content": "Hello! How can I help you today?",
                },
                "finish_reason": NOTHING,
            },),
            "type": "delta",
        };
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
            hash<auto> req = stream_req + {"stream": True};
            *object rest = getRestClient();
            if (rest instanceof PerplexityRestClient::PerplexityRestClient) {
                stream = cast<PerplexityRestClient::PerplexityRestClient>(rest).openChatStream(req);
            } else if (rest instanceof PerplexityRestClient::PerplexityRestClientIo) {
                stream = cast<PerplexityRestClient::PerplexityRestClientIo>(rest).openChatStream(req);
            } else {
                throw "SSE-ERROR", "chat streaming requires PerplexityRestClient or PerplexityRestClientIo";
            }
            if (stop_flag) {
                stream.close();
                return;
            }

            while (!stop_flag) {
                *hash<SseMessageInfo> evt = stream.readEvent(1s);
                if (!evt) {
                    break;
                }
                if (evt."data" == "[DONE]") {
                    notifyObservers(EVENT_PERPLEXITY_CHAT_STREAM, {"type": "done"});
                    break;
                }
                hash<auto> payload = parseEventData(evt);
                if (payload) {
                    notifyObservers(EVENT_PERPLEXITY_CHAT_STREAM, payload);
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            try {
                notifyObservers(EVENT_PERPLEXITY_CHAT_STREAM, {
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
}
}
