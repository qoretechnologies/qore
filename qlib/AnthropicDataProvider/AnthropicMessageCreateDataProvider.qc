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
        hash<auto> rv = doRestCommand("POST", "messages", req).body;
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
        # extract and remove the conversation_key (not part of the API)
        *string ctx_key = remove req.conversation_key;
        ctx_info.ctx_key = ctx_key;
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
