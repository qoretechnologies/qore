# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore PerplexityDataProvider module definition

/** PerplexitySimpleChatCompletionDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the PerplexityDataProvider module
public namespace PerplexityDataProvider {
#! The Perplexity simple chat completion data provider class
public class PerplexitySimpleChatCompletionDataProvider inherits PerplexityDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "simple-chat",
            "type": "PerplexitySimpleChatCompletionDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = PerplexitySimpleChatCompletionRequestType;

        #! Response type
        const ResponseType = PerplexitySimpleChatCompletionResponseType;
    }

    #! Creates the object from the arguments
    constructor(*RestClientIo::RestClientIo rest) : PerplexityDataProviderCommon(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req.messages = ({
            "role": remove req.role,
            "content": remove req.content,
        },);
        hash<auto> rv = doRestCommand("POST", "chat/completions", req).body;
        rv.choice = rv.choices[0];
        delete rv.choices;
        return rv;
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! The simple chat completion request type
public class PerplexitySimpleChatCompletionRequestType inherits HashDataType {
    public {
        #! fields
        const Fields = PerplexityMessageDataType::Fields{"role", "content"} +
            PerplexityChatCompletionRequestType::Fields - "messages";
    }

    #! Creates the type
    constructor() : HashDataType("PerplexitySimpleChatCompletionRequest") {
        addQoreFields(Fields);
    }
}

#! The simple chat completion request type constant
public const PerplexitySimpleChatCompletionRequestType = new PerplexitySimpleChatCompletionRequestType();

#! The simple chat completion response type
public class PerplexitySimpleChatCompletionResponseType inherits HashDataType {
    public {
        #! fields
        const Fields = PerplexityChatCompletionResponseType::Fields - "choices" + {
            "choice": {
                "display_name": "Choice",
                "short_desc": "The single chat completion choice returned by the server",
                "desc": "The single chat completion choice returned by the server",
                "type": PerplexityChoiceDataType,
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("PerplexitySimpleChatCompletionResponse") {
        addQoreFields(Fields);
    }
}

#! The simple chat completion response type constant
public const PerplexitySimpleChatCompletionResponseType = new PerplexitySimpleChatCompletionResponseType();
}
