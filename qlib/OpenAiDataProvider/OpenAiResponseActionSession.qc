# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAI response action session class definition

/** OpenAiResponseActionSession.qc Copyright 2026 Qore Technologies, s.r.o.

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

%modern

%try-module json
%define NoJson
%endtry

%try-module yaml
%define NoYaml
%endtry

#! contains all public definitions in the OpenAiDataProvider module
public namespace OpenAiDataProvider {

#! Action-based OpenAI responses session for Creator WebSocket integration
public class OpenAiResponseActionSession inherits DataProvider::Observer {
    private {
        #! Responses data provider root
        OpenAiResponsesDataProvider responses;

        #! Stream provider (when streaming)
        *OpenAiResponseStreamDataProvider stream;

        #! Optional conversation context store
        *OpenAiConversationContextStore context_store;

        #! Optional logger
        *LoggerInterface logger;

        #! Event emitter callback
        *code emit_cb;
    }

    #! Creates a new action session
    /** @param opts options accepted by OpenAiResponsesDataProvider plus:
        - \c conversation_context_store: optional context store
        - \c logger: optional LoggerInterface
        - \c emitter: optional event emitter callback
    */
    constructor(*hash<auto> opts) {
        *hash<auto> response_opts = opts;
        if (response_opts) {
            remove response_opts.logger;
            remove response_opts.emitter;
        }
        responses = new OpenAiResponsesDataProvider(response_opts);
        if (opts) {
            if (opts.conversation_context_store) {
                context_store = opts.conversation_context_store;
                responses.setConversationContextStore(context_store);
            }
            if (opts.logger) {
                logger = opts.logger;
                responses.setLogger(logger);
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

    #! Handles an OpenAI action
    hash<auto> handleAction(string action, *hash<auto> args) {
        switch (action) {
            case "openai-response-create":
                return handleCreate(args);

            case "openai-response-start":
                return handleStart(args);

            case "openai-response-stop":
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
            "event": "openai-response-event",
            "data": buildEventPayload(data_),
        });
    }

    private hash<auto> handleCreate(*hash<auto> args) {
        OpenAiResponseCreateDataProvider create
            = cast<OpenAiResponseCreateDataProvider>(responses.getChildProvider("create"));
        if (context_store) {
            create.setConversationContextStore(context_store);
        }
        if (logger) {
            create.setLogger(logger);
        }
        hash<auto> req = sanitizeRequest(args);
        hash<auto> resp = create.doRequest(req);
        return normalizeResponse(resp, req);
    }

    private hash<auto> handleStart(*hash<auto> args) {
        if (!stream) {
            stream = cast<OpenAiResponseStreamDataProvider>(responses.getChildProvider("stream"));
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

    private hash<auto> normalizeResponse(hash<auto> resp, hash<auto> req) {
        *string format_hint = getFormatHint(req, resp);
        list<hash<auto>> output = normalizeOutput(resp.output, format_hint, req.metadata);
        return {
            "response_id": resp.id,
            "output": output,
            "usage": resp.usage,
            "conversation_context": resp.conversation_context,
        };
    }

    private list<hash<auto>> normalizeOutput(*list<auto> output, *string format_hint, *hash<auto> metadata) {
        list<hash<auto>> items = ();
        if (!output) {
            return items;
        }
        foreach hash<auto> item in (output) {
            if (item.type == "message" || item.role) {
                items += {
                    "role": item.role ?? "assistant",
                    "content": normalizeContent(item.content, format_hint, metadata),
                    "metadata": item.metadata,
                };
                continue;
            }
            if (item.type == "tool_call" || item.type == "tool_result") {
                items += {
                    "role": "tool",
                    "content": (
                        {
                            "type": "json",
                            "data": item,
                        },
                    ),
                };
                continue;
            }
            if (item.text) {
                items += {
                    "role": "assistant",
                    "content": (
                        makeTextPart(item.text, format_hint, metadata),
                    ),
                };
            }
        }
        return items;
    }

    private list<hash<auto>> normalizeContent(*list<auto> content, *string format_hint, *hash<auto> metadata) {
        list<hash<auto>> parts = ();
        if (!content) {
            return parts;
        }
        foreach hash<auto> part in (content) {
            switch (part.type) {
                case "output_text":
                    parts += makeTextPart(part.text, format_hint, metadata, part.annotations);
                    break;

                case "output_image":
                    parts += makeImagePart(part);
                    break;

                case "output_audio":
                    parts += makeAudioPart(part);
                    break;

                default:
                    if (part.text) {
                        parts += makeTextPart(part.text, format_hint, metadata, part.annotations);
                    }
            }
        }
        return parts;
    }

    private hash<auto> makeTextPart(string text, *string format_hint, *hash<auto> metadata,
            *list<hash<auto>> annotations) {
        *string hint = format_hint ?? (metadata ? metadata.output_format : NOTHING);
        if (hint == "json") {
%ifdef NoJson
            return {
                "type": "json",
                "text": text,
                "annotations": annotations,
            };
%else
            try {
                return {
                    "type": "json",
                    "text": text,
                    "data": parse_json(text),
                    "annotations": annotations,
                };
            } catch (hash<ExceptionInfo> ex) {
            }
%endif
        }
        if (hint == "yaml") {
%ifdef NoYaml
            return {
                "type": "yaml",
                "text": text,
                "annotations": annotations,
            };
%else
            try {
                return {
                    "type": "yaml",
                    "text": text,
                    "data": parse_yaml(text),
                    "annotations": annotations,
                };
            } catch (hash<ExceptionInfo> ex) {
            }
%endif
        }
        if (hint == "markdown") {
            return {
                "type": "markdown",
                "text": text,
                "annotations": annotations,
            };
        }
        if (hint == "code") {
            return {
                "type": "code",
                "text": text,
                "language": metadata ? (metadata.code_language ?? metadata.language) : NOTHING,
                "annotations": annotations,
            };
        }
        return {
            "type": "text",
            "text": text,
            "annotations": annotations,
        };
    }

    private hash<auto> makeImagePart(hash<auto> part) {
        hash<auto> rv = {
            "type": "image",
            "mime_type": part.mime_type,
        };
        if (part.image_url) {
            rv.url = part.image_url;
        }
        if (part.file_id) {
            rv.data = {"file_id": part.file_id};
        }
        return rv;
    }

    private hash<auto> makeAudioPart(hash<auto> part) {
        hash<auto> rv = {
            "type": "file",
            "mime_type": part.mime_type,
        };
        if (part.file_id) {
            rv.data = {
                "file_id": part.file_id,
                "format": part.format,
            };
        }
        return rv;
    }

    private hash<auto> buildEventPayload(hash<auto> payload) {
        hash<auto> data = {
            "event_type": simplifyEventType(payload.type),
            "openai_type": payload.type,
            "id": payload.id,
            "event": payload.event,
        };
        list<hash<auto>> content = extractEventContent(payload);
        if (content) {
            data.content = content;
        }
        if (payload.raw_data) {
            data.raw_data = payload.raw_data;
        }
        if (payload.error) {
            data.error = payload.error;
        }
        return data;
    }

    private list<hash<auto>> extractEventContent(hash<auto> payload) {
        *hash<auto> resp = payload.response;
        if (resp && resp.output) {
            *string format_hint = getFormatHint(payload.metadata, resp);
            list<hash<auto>> output = normalizeOutput(resp.output, format_hint, payload.metadata);
            list<hash<auto>> parts = ();
            foreach hash<auto> item in (output) {
                if (item.content) {
                    parts += item.content;
                }
            }
            return parts;
        }
        if (payload.delta && payload.delta.text) {
            return (
                {
                    "type": "text",
                    "text": payload.delta.text,
                },
            );
        }
        return ();
    }

    private string simplifyEventType(*string openai_type) {
        if (!openai_type) {
            return "unknown";
        }
        switch (openai_type) {
            case "response.created": return "response-created";
            case "response.in_progress": return "response-in-progress";
            case "response.completed": return "response-completed";
            case "response.incomplete": return "response-incomplete";
            case "response.failed": return "response-failed";
            case "response.output_text.delta": return "text-delta";
            case "response.output_text.done": return "text-done";
            case "response.tool_call.delta": return "tool-delta";
            case "response.tool_call.done": return "tool-done";
        }
        if (openai_type =~ /^response\./) {
            string rv = replace(openai_type, "response.", "");
            rv = replace(rv, ".", "-");
            return rv;
        }
        return replace(openai_type, ".", "-");
    }

    private *string getFormatHint(*hash<auto> req, *hash<auto> resp) {
        if (req && req.output_format) {
            return req.output_format;
        }
        if (req && req.metadata && req.metadata.output_format) {
            return req.metadata.output_format;
        }
        if (resp && resp.text && resp.text.format && resp.text.format.type) {
            if (resp.text.format.type == "json_schema" || resp.text.format.type == "json_object") {
                return "json";
            }
        }
        return NOTHING;
    }
}

} # namespace OpenAiDataProvider
