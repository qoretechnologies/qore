# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MistralChatCompletionDataProvider class definition

/** MistralChatCompletionDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace MistralDataProvider {
public class MistralChatCompletionDataProvider inherits MistralDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "chat-completion",
            "type": "MistralChatCompletionDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        static MistralChatCompletionRequestDataType RequestType =
            new MistralChatCompletionRequestDataType();
        static MistralChatCompletionResponseDataType ResponseType =
            new MistralChatCompletionResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : MistralDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : MistralDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = doRestCommand("POST", "chat/completions", req).body;
        if (body.created.typeCode() == NT_INT) {
            body.created = 1970-01-01Z + seconds(body.created);
        }
        return body;
    }
}
}
