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
    }

    private {
        #! The request type, built on first use; see requestType()
        static *MistralChatCompletionRequestDataType request_type;
        #! The ID of the request type's builder; see requestType()
        static int request_type_id = DataProviderActionCatalog::registerLazyValue(
            \MistralChatCompletionDataProvider::buildRequestType());
        #! The response type, built on first use; see responseType()
        static *MistralChatCompletionResponseDataType response_type;
        #! The ID of the response type's builder; see responseType()
        static int response_type_id = DataProviderActionCatalog::registerLazyValue(
            \MistralChatCompletionDataProvider::buildResponseType());
    }

    #! Returns the request type, which is built on first use
    /** Built from the vendored API description, so loading the module does not parse it
    */
    static MistralChatCompletionRequestDataType requestType() {
        return request_type ?? (request_type = DataProviderActionCatalog::getLazyValue(request_type_id));
    }

    #! Builds the request type; see requestType()
    private static MistralChatCompletionRequestDataType buildRequestType() {
        return new MistralChatCompletionRequestDataType();
    }

    #! Returns the response type, which is built on first use
    /** Built from the vendored API description, so loading the module does not parse it
    */
    static MistralChatCompletionResponseDataType responseType() {
        return response_type ?? (response_type = DataProviderActionCatalog::getLazyValue(response_type_id));
    }

    #! Builds the response type; see responseType()
    private static MistralChatCompletionResponseDataType buildResponseType() {
        return new MistralChatCompletionResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : MistralDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : MistralDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return requestType(); }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return responseType(); }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return getSchemaProvider("chat-completion").doRequest(req, request_options);
    }
}
}
