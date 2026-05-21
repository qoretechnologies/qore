# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DeepSeekChatCompletionDataProvider class definition

/** DeepSeekChatCompletionDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DeepSeekDataProvider module
public namespace DeepSeekDataProvider {
#! Data provider for the DeepSeek `/v1/chat/completions` endpoint
public class DeepSeekChatCompletionDataProvider inherits DeepSeekDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "chat-completion",
            "type": "DeepSeekChatCompletionDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        static DeepSeekChatCompletionRequestDataType RequestType =
            new DeepSeekChatCompletionRequestDataType();

        #! Response type
        static DeepSeekChatCompletionResponseDataType ResponseType =
            new DeepSeekChatCompletionResponseDataType();
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest) : DeepSeekDataProviderCommon(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : DeepSeekDataProviderCommon(options) {
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
    /** Converts DeepSeek's Unix-seconds `created` field on the response into
        a Qore date to match the typed `*date` field in the response schema.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = doRestCommand("POST", "chat/completions", req).body;
        if (body.created.typeCode() == NT_INT) {
            body.created = 1970-01-01Z + seconds(body.created);
        }
        return body;
    }
}
}
