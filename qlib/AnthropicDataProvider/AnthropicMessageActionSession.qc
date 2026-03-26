# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Anthropic message action session class definition

/** AnthropicMessageActionSession.qc Copyright 2026 Qore Technologies, s.r.o.

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

%try-module json
%define NoJson
%endtry

#! contains all public definitions in the AnthropicDataProvider module
public namespace AnthropicDataProvider {

#! Action-based Anthropic messages session for Creator WebSocket integration
/** Provides a transport-agnostic interface for message creation and streaming with
    normalized rich content output.

    Actions:
    - \c anthropic-message-create: create a message (non-streaming)
    - \c anthropic-message-start: start streaming a message
    - \c anthropic-message-stop: stop an active stream

    @par Example:
    @code{.py}
AnthropicMessageActionSession session({
    "restclient": rest,
    "emitter": sub (hash<auto> event) { ws.send(event); },
});
hash<auto> result = session.handleAction("anthropic-message-create", {
    "model": "claude-sonnet-4-6-20250514",
    "max_tokens": 1024,
    "messages": ({"role": "user", "content": "Hello!"},),
});
    @endcode
*/
public class AnthropicMessageActionSession inherits DataProvider::Observer {
    private {
        #! Messages data provider root
        AnthropicMessagesDataProvider messages;

        #! Stream provider (when streaming)
        *AnthropicMessageStreamDataProvider stream;

        #! Optional conversation context store
        *AnthropicConversationContextStore context_store;

        #! Optional logger
        *LoggerInterface logger;

        #! Event emitter callback
        *code emit_cb;
    }

    #! Creates a new action session
    /** @param opts options accepted by AnthropicMessagesDataProvider plus:
        - \c conversation_context_store: optional context store
        - \c logger: optional LoggerInterface
        - \c emitter: optional event emitter callback
    */
    constructor(*hash<auto> opts) {
        *hash<auto> msg_opts = opts;
        if (msg_opts) {
            remove msg_opts.logger;
            remove msg_opts.emitter;
        }
        messages = new AnthropicMessagesDataProvider(msg_opts);
        if (opts) {
            if (opts.conversation_context_store) {
                context_store = opts.conversation_context_store;
                messages.setConversationContextStore(context_store);
            }
            if (opts.logger) {
                logger = opts.logger;
                messages.setLogger(logger);
            }
            if (opts.emitter) {
                emit_cb = opts.emitter;
            }
        }
    }

    destructor() {
        stopStream();
    }

    #! Sets the event emitter callback
    setEmitter(*code cb) {
        emit_cb = cb;
    }

    #! Handles an Anthropic action
    hash<auto> handleAction(string action, *hash<auto> args) {
        switch (action) {
            case "anthropic-message-create":
                return handleCreate(args);

            case "anthropic-message-start":
                return handleStart(args);

            case "anthropic-message-stop":
                return handleStop();
        }

        throw "INVALID-ACTION", sprintf("Unsupported action %y", action);
    }

    #! Observer update handler for stream events
    update(string event_id, hash<auto> data_) {
        if (!emit_cb) {
            return;
        }
        emit_cb({
            "event": "anthropic-message-event",
            "data": buildEventPayload(data_),
        });
    }

    private hash<auto> handleCreate(*hash<auto> args) {
        AnthropicMessageCreateDataProvider create
            = cast<AnthropicMessageCreateDataProvider>(messages.getChildProvider("create"));
        if (context_store) {
            create.setConversationContextStore(context_store);
        }
        if (logger) {
            create.setLogger(logger);
        }
        hash<auto> req = sanitizeRequest(args);
        hash<auto> resp = create.doRequest(req);
        return normalizeResponse(resp);
    }

    private hash<auto> handleStart(*hash<auto> args) {
        if (!stream) {
            stream = cast<AnthropicMessageStreamDataProvider>(messages.getChildProvider("stream"));
            if (context_store) {
                stream.setConversationContextStore(context_store);
            }
            if (logger) {
                stream.setLogger(logger);
            }
            stream.registerObserver(self);
            stream.observersReady();
        }
        hash<auto> req = sanitizeRequest(args);
        return stream.doRequest(req);
    }

    private hash<auto> handleStop() {
        stopStream();
        return {
            "status": "stopped",
        };
    }

    private stopStream() {
        if (stream) {
            try {
                stream.stopEvents();
            } catch (hash<ExceptionInfo> ex) {
            }
            stream.deregisterObserver(self);
            delete stream;
        }
    }

    private hash<auto> sanitizeRequest(*hash<auto> args) {
        hash<auto> req = args ?? {};
        remove req.action;
        remove req.request_id;
        remove req.id;
        return req;
    }

    private hash<auto> normalizeResponse(hash<auto> resp) {
        return {
            "message_id": resp.id,
            "role": resp.role,
            "content": normalizeContent(resp.content),
            "model": resp.model,
            "stop_reason": resp.stop_reason,
            "usage": resp.usage,
            "output_text": resp.output_text,
            "tool_calls": resp.tool_calls,
        };
    }

    private list<hash<auto>> normalizeContent(*list<auto> content) {
        list<hash<auto>> parts = ();
        if (!content) {
            return parts;
        }
        foreach hash<auto> block in (content) {
            switch (block.type) {
                case "text":
                    parts += {
                        "type": "text",
                        "text": block.text,
                    };
                    break;

                case "tool_use":
                    parts += {
                        "type": "tool_use",
                        "id": block.id,
                        "name": block.name,
                        "input": block.input,
                    };
                    break;

                case "thinking":
                    parts += {
                        "type": "thinking",
                        "thinking": block.thinking,
                    };
                    break;

                default:
                    parts += block;
            }
        }
        return parts;
    }

    #! Builds event payload from stream data for the emitter
    private hash<auto> buildEventPayload(hash<auto> data_) {
        hash<auto> payload = {
            "type": data_.type,
        };
        switch (data_.type) {
            case "message_start":
                payload.message = data_.message;
                break;

            case "content_block_start":
                payload.index = data_.index;
                payload.content_block = data_.content_block;
                break;

            case "content_block_delta":
                payload.index = data_.index;
                payload.delta = data_.delta;
                # normalize text deltas for easier consumption
                if (data_.delta.type == "text_delta") {
                    payload.normalized_type = "text-delta";
                    payload.text = data_.delta.text;
                } else if (data_.delta.type == "input_json_delta") {
                    payload.normalized_type = "tool-input-delta";
                    payload.partial_json = data_.delta.partial_json;
                } else if (data_.delta.type == "thinking_delta") {
                    payload.normalized_type = "thinking-delta";
                    payload.thinking = data_.delta.thinking;
                }
                break;

            case "content_block_stop":
                payload.index = data_.index;
                break;

            case "message_delta":
                payload.delta = data_.delta;
                payload.usage = data_.usage;
                break;

            case "message_stop":
                payload.normalized_type = "message-completed";
                break;

            case "error":
                payload.error = data_.error;
                break;

            default:
                payload += data_;
        }
        return payload;
    }
}
}
