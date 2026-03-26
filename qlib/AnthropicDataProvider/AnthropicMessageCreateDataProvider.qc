# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AnthropicDataProvider module definition

/** AnthropicMessageCreateDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Data provider for creating Anthropic messages (non-streaming)
public class AnthropicMessageCreateDataProvider inherits AnthropicDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "type": "AnthropicMessageCreateDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = AnthropicMessageRequestDataType;

        #! Response type
        const ResponseType = AnthropicMessageResponseDataType;
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
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

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> ctx_info = {};
        *AnthropicConversationContextStore ctx_store = getConversationContextStore();
        req = prepareRequest(req, ctx_store, \ctx_info);
        # ensure stream is False for non-streaming requests
        delete req.stream;
        hash<auto> rv = doRestCommand("POST", "messages", req, ctx_info.extra_headers).body;
        # add convenience fields
        if (rv.content) {
            rv.output_text = getOutputText(rv.content);
            rv.tool_calls = getToolCalls(rv.content);
        }
        # save conversation context if applicable
        if (rv.id && ctx_info.ctx_key && ctx_store) {
            list<auto> saved_messages = ctx_info.messages ?? ();
            saved_messages += req.messages[req.messages.size() - 1];
            saved_messages += {"role": "assistant", "content": rv.content};
            hash<auto> new_ctx = {
                "messages": saved_messages,
            };
            if (req.system) {
                new_ctx.system = req.system;
            }
            ctx_store.saveContext(ctx_info.ctx_key, new_ctx);
        }
        return rv;
    }

    #! Prepares a request, loading conversation context if configured
    static hash<auto> prepareRequest(hash<auto> req, *AnthropicConversationContextStore ctx_store,
            reference<hash<auto>> ctx_info) {
        # transform flattened action options into nested API format
        *auto output_schema = remove req.output_schema;
        *string effort = remove req.effort;
        if (output_schema || effort) {
            req.output_config = {};
            if (output_schema) {
                # normalize schema: convert simple string property values to {"type": "..."}
                if (output_schema.properties) {
                    hash<auto> props = output_schema.properties;
                    foreach hash<auto> i in (props.pairIterator()) {
                        if (i.value.typeCode() == NT_STRING) {
                            props{i.key} = {"type": i.value};
                        }
                    }
                    output_schema.properties = props;
                }
                # auto-set additionalProperties: false for object types (required by the API)
                if (output_schema.type == "object" && !exists output_schema.additionalProperties) {
                    output_schema.additionalProperties = False;
                }
                req.output_config.format = {
                    "type": "json_schema",
                    "schema": output_schema,
                };
            }
            if (effort) {
                req.output_config.effort = effort;
            }
        }
        *string thinking_mode = remove req.thinking_mode;
        *int thinking_budget = remove req.thinking_budget_tokens;
        if (thinking_mode) {
            req.thinking = {
                "type": thinking_mode,
            };
            if (thinking_budget) {
                req.thinking.budget_tokens = thinking_budget;
            }
        }
        # transform skill string into container.skills + inject code execution tool
        *string skill_val = remove req.skill;
        if (skill_val.val()) {
            # parse "{type}/{skill_id}" format; default to "custom" if no prefix
            string skill_type;
            string skill_id;
            int slash = skill_val.find("/");
            if (slash > 0) {
                skill_type = skill_val.substr(0, slash);
                skill_id = skill_val.substr(slash + 1);
            } else {
                skill_type = "custom";
                skill_id = skill_val;
            }
            req.container = {"skills": ({"type": skill_type, "skill_id": skill_id, "version": "latest"},)};
            # inject code execution tool (required for skills)
            if (!req.tools) {
                req.tools = ();
            }
            req.tools += {"type": "code_execution_20250825", "name": "code_execution"};
            # store beta headers for the REST call
            ctx_info.extra_headers = {
                "anthropic-beta": "code-execution-2025-08-25,skills-2025-10-02",
            };
        }
        # extract and remove the conversation_key (not part of the API)
        *string ctx_key = remove req.conversation_key;
        ctx_info.ctx_key = ctx_key;
        # convert message content + files into API content blocks
        req.messages = convertMessages(req.messages);
        # load conversation context if available
        if (ctx_key && ctx_store) {
            *hash<auto> ctx = ctx_store.loadContext(ctx_key);
            if (ctx) {
                ctx_info.ctx = ctx;
                # prepend previous messages
                if (ctx.messages) {
                    req.messages = ctx.messages + req.messages;
                    ctx_info.messages = ctx.messages;
                }
                # use stored system prompt if not provided
                if (!req.system && ctx.system) {
                    req.system = ctx.system;
                }
            }
        }
        return req;
    }

    #! Converts messages with split content/files fields into API format
    /** Transforms the user-friendly message format (separate \c content string and \c files list)
        into the Anthropic API content block format.

        Messages that already have \c content as a list (from conversation context) are passed through unchanged.
    */
    static list<auto> convertMessages(list<auto> messages) {
        list<auto> result = ();
        foreach hash<auto> msg in (messages) {
            # skip messages that already have content as a list (from context store)
            if (msg.content.typeCode() == NT_LIST) {
                result += msg;
                continue;
            }
            # extract and remove file (not part of the API)
            *hash<auto> file = remove msg.file;
            if (!file) {
                # simple text message — pass through as string (API accepts this)
                result += msg;
                continue;
            }
            # build content blocks for multi-part message
            list<auto> blocks = ();
            if (msg.content.val()) {
                blocks += {"type": "text", "text": msg.content};
            }
            blocks += convertFileToContentBlock(file);
            msg.content = blocks;
            result += msg;
        }
        return result;
    }

    #! Converts a FileDataType value to an Anthropic content block
    /** Maps file MIME types to the appropriate Anthropic content block type:
        - \c image/* -> \c image content block with base64 source
        - \c application/pdf -> \c document content block with base64 source
    */
    public {
        #! Supported image MIME types for the Anthropic API
        const SupportedImageTypes = {
            "image/jpeg": True,
            "image/png": True,
            "image/gif": True,
            "image/webp": True,
        };
    }

    static hash<auto> convertFileToContentBlock(hash<auto> file) {
        string mime_type = file.mime_type ?? "application/octet-stream";
        string content = file.content;
        if (mime_type =~ /^image\//) {
            if (!SupportedImageTypes{mime_type}) {
                throw "ANTHROPIC-FILE-ERROR", sprintf("Unsupported image type %y; supported types: %y",
                    mime_type, keys SupportedImageTypes);
            }
            return {
                "type": "image",
                "source": {
                    "type": "base64",
                    "media_type": mime_type,
                    "data": content,
                },
            };
        }
        if (mime_type == "application/pdf") {
            return {
                "type": "document",
                "source": {
                    "type": "base64",
                    "media_type": mime_type,
                    "data": content,
                },
            };
        }
        # fallback: treat as document
        return {
            "type": "document",
            "source": {
                "type": "base64",
                "media_type": mime_type,
                "data": content,
            },
        };
    }

    #! Extracts text from content blocks
    static *string getOutputText(list<auto> content) {
        list<string> texts;
        foreach hash<auto> block in (content) {
            if (block.type == "text" && block.text.val()) {
                texts += block.text;
            }
        }
        if (texts) {
            return texts.join("");
        }
    }

    #! Extracts tool_use blocks from content
    static *list<hash<auto>> getToolCalls(list<auto> content) {
        list<hash<auto>> calls;
        foreach hash<auto> block in (content) {
            if (block.type == "tool_use") {
                calls += block;
            }
        }
        return calls ?? NOTHING;
    }
}
}
