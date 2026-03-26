# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GeminiDataProvider module definition

/** GeminiStreamContentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GeminiDataProvider module
public namespace GeminiDataProvider {
#! Stream event constant
public const EVENT_GEMINI_STREAM = "gemini-stream-event";

#! Stream content data provider (SSE streaming via DelayedObservable)
public class GeminiStreamContentDataProvider inherits GeminiDataProviderBase, DataProvider::DelayedObservable {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "stream-content",
            "type": "GeminiStreamContentDataProvider",
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
        const RequestType = new GeminiGenerateContentRequestDataType();

        #! Response type
        const ResponseType = GeminiStreamEventDataTypeInst;
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

    #! Creates the object from the REST client
    constructor(RestClientIo::RestClientIo rest) : GeminiDataProviderBase(rest) {
    }

    #! Creates the object from constructor options
    constructor(hash<auto> options) : GeminiDataProviderBase(options) {
    }

    #! Destroys the object
    destructor() {
        stop();
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns example event data for the given event
    private auto getExampleEventDataImpl(string event_id) {
        try {
            # Try to get real data from the API using a simple prompt
            hash<auto> req = {
                "contents": ({
                    "role": "user",
                    "parts": ({"text": "Say hello"},),
                },),
            };
            hash<auto> resp = doRestCommand("POST",
                GeminiDataProviderBase::buildModelPath("gemini-2.5-flash", "generateContent"), req).body;
            if (resp.candidates) {
                return resp;
            }
        } catch () {
            # Fall through to fake data
        }
        return getFakeExampleData();
    }

    #! Returns fake example data for the stream event
    private hash<auto> getFakeExampleData() {
        return {
            "candidates": ({
                "content": {
                    "parts": ({"text": "Hello! How can I help you today?"},),
                    "role": "model",
                },
                "finishReason": "STOP",
                "index": 0,
            },),
            "usageMetadata": {
                "promptTokenCount": 5,
                "candidatesTokenCount": 10,
                "totalTokenCount": 15,
            },
            "modelVersion": "gemini-2.5-flash",
        };
    }

    #! Returns supported event types
    hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_GEMINI_STREAM: <DataProviderMessageInfo>{
                "desc": "Gemini streaming content generation event",
                "type": ResponseType,
            },
        };
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
        # Handle prompt shortcut: wrap into contents structure
        if (req.prompt) {
            string prompt = remove req.prompt;
            req.contents = ({
                "role": "user",
                "parts": ({"text": prompt},),
            },);
        } else if (!req.contents) {
            throw "STREAM-CONTENT-ERROR", "either 'prompt' or 'contents' must be provided";
        }
        # Auto-fill missing role on content items (default to "user")
        req.contents = map $1 + ($1.role ? {} : {"role": "user"}), req.contents;
        # Wrap systemInstruction string into Content format
        if (req.systemInstruction.typeCode() == NT_STRING) {
            req.systemInstruction = {"parts": ({"text": req.systemInstruction},)};
        } else if (req.systemInstruction) {
            delete req.systemInstruction.role;
        }
        # Map fileSearchStore to tools structure
        if (req.fileSearchStore) {
            string store = remove req.fileSearchStore;
            if (store !~ /^fileSearchStores\//) {
                store = "fileSearchStores/" + store;
            }
            req.tools = ({"file_search": {"file_search_store_names": (store,)}},);
        }
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
        bool do_wait;
        {
            m.lock();
            on_exit m.unlock();
            if (!started || stop_flag) {
                return;
            }
            stop_flag = True;
            if (stream) {
                stream.close();
            }
            do_wait = True;
        }
        if (do_wait) {
            cnt.wait();
        }
    }

    #! SSE event loop
    private eventLoop() {
        on_exit {
            cnt.done();
        }
        try {
            hash<auto> req = stream_req;
            string model = remove req.model;

            *RestClientIo::RestClientIo rest_io = cast<*RestClientIo::RestClientIo>(getRestClient());
            if (!rest_io) {
                throw "SSE-ERROR", "streaming requires a RestClientIo-based REST client";
            }
            # Check if the rest client supports openStreamingConnection
            *GeminiRestClient::GeminiRestClientIo gemini_rest = cast<*GeminiRestClient::GeminiRestClientIo>(rest_io);
            if (!gemini_rest) {
                throw "SSE-ERROR", "streaming requires GeminiRestClientIo";
            }
            stream = gemini_rest.openStreamingConnection(model, req);

            while (!stop_flag) {
                *hash<SseMessageInfo> evt = stream.readEvent(1s);
                if (!evt) {
                    if (stream.isDone()) {
                        # Stream ended (no [DONE] sentinel in Gemini API)
                        notifyObservers(EVENT_GEMINI_STREAM, {"type": "done"});
                        break;
                    }
                    # Timeout — server is still generating, keep polling
                    continue;
                }
                hash<auto> payload = parseEventData(evt);
                if (payload) {
                    notifyObservers(EVENT_GEMINI_STREAM, payload);
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            try {
                notifyObservers(EVENT_GEMINI_STREAM, {
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
