# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GroqChatCompletionDataProvider class definition

/** GroqChatCompletionDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GroqDataProvider module
public namespace GroqDataProvider {
#! Data provider for the Groq <tt>/v1/chat/completions</tt> endpoint
public class GroqChatCompletionDataProvider inherits GroqDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "chat-completion",
            "type": "GroqChatCompletionDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        static GroqChatCompletionRequestDataType RequestType =
            new GroqChatCompletionRequestDataType();

        #! Response type
        static GroqChatCompletionResponseDataType ResponseType =
            new GroqChatCompletionResponseDataType();
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest) : GroqDataProviderCommon(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : GroqDataProviderCommon(options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Makes a chat-completion request and returns the response
    /** Converts Groq's Unix-seconds <tt>created</tt> field on the response into a
        Qore date to match the typed <tt>*date</tt> field in the response schema.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        GroqApiDataProvider::validateGeneration(req);
        if (!req.messages || !trim(req.model)) {
            throw "GROQ-REQUEST-ERROR", "chat requires a model and at least one message";
        }
        if ((exists req.max_tokens && exists req.max_completion_tokens)
            || (exists req.reasoning_format && exists req.include_reasoning)) {
            throw "GROQ-REQUEST-ERROR", "conflicting token limits or reasoning controls";
        }
        if (req.stop.typeCode() == NT_LIST && req.stop.size() > 4) {
            throw "GROQ-REQUEST-ERROR", "at most four stop sequences are supported";
        }
        foreach hash<auto> tool in (req.tools) {
            if (tool.type == "function" && !tool.function) {
                throw "GROQ-REQUEST-ERROR", "function tools require a function definition";
            }
        }
        hash<auto> body = doRestCommand("POST", "chat/completions", req).body;
        if (body.created.typeCode() == NT_INT) {
            body.created = 1970-01-01Z + seconds(body.created);
        }
        return body;
    }
}
}
