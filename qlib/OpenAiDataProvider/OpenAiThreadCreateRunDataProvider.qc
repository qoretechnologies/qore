# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAiDataProvider module definition

/** OpenAiThreadCreateRunDataProvider.qc Copyright 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the OpenAiDataProvider module
public namespace OpenAiDataProvider {
#! The OpenAi data provider class
public class OpenAiThreadCreateRunDataProvider inherits OpenAiDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-and-run",
            "type": "OpenAiThreadCreateRunDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = OpenAiThreadCreateRunRequestType;

        #! Response type
        const ResponseType = OpenAiRunDataType;
    }

    private {
        #! The assistant
        hash<auto> assistant;
    }

    #! Creates the object from the arguments
    constructor(*RestClientIo::RestClientIo rest, hash<auto> assistant) : OpenAiDataProviderCommon(rest) {
        self.assistant = assistant;
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
        hash<auto> rv = doRestCommand("POST", "threads/runs", {"assistant_id": assistant.id} + req,
            AssistantApiHdr).body;
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

#! The thread data type
public class OpenAiThreadDataType inherits HashDataType {
    public {
        #! fields
        const Fields = OpenAiThreadCreateRequestType::Fields;
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiThread") {
        addQoreFields(Fields);
    }
}

#! The thread data type constant
public const OpenAiThreadDataType = new OpenAiThreadDataType();

#! The file upload request type
public class OpenAiThreadCreateRunRequestType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "thread": {
                "display_name": "Thread",
                "short_desc": "Information about the thread to create",
                "desc": "Information about the thread to create",
                "type": OpenAiThreadDataType,
            },
        } + OpenAiRunCreateRequestType::Fields{"model", "instructions", "tools"}
        + OpenAiThreadCreateRequestType::Fields{"tool_resources", "metadata"}
        + OpenAiChatCompletionRequestType::Fields{"temperature", "top_p", "stream", }
        + OpenAiRunCreateRequestType::Fields{"max_prompt_tokens", "max_completion_tokens", "truncation_strategy", }
        + OpenAiChatCompletionRequestType::Fields{"tool_choice", "response_format"};
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiThreadCreateRequest") {
        addQoreFields(Fields);
    }
}

#! File upload request type constant
public const OpenAiThreadCreateRunRequestType = new OpenAiThreadCreateRunRequestType();
}
