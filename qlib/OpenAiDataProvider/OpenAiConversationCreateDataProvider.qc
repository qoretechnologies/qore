# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAiDataProvider module definition

/** OpenAiConversationCreateDataProvider.qc Copyright 2024 - 2026 Qore Technologies, s.r.o.

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
#! Creates a conversation
/** @since OpenAiDataProvider 1.6
*/
public class OpenAiConversationCreateDataProvider inherits OpenAiDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "type": "OpenAiConversationCreateDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = OpenAiConversationCreateRequestType;

        #! Response type
        const ResponseType = OpenAiConversationDataType;
    }

    #! Creates the object from the arguments
    constructor(*RestClientIo::RestClientIo rest) : OpenAiDataProviderCommon(rest) {
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
        return OpenAiDataProviderCommon::fixConversationTimestamps(
            doRestCommand("POST", "conversations", req).body
        );
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

#! Conversation create request type
/** @since OpenAiDataProvider 1.6
*/
public class OpenAiConversationCreateRequestType inherits HashDataType {
    public {
        #! The maximum number of initial items accepted by the API in a single create call
        const MaxInitialItems = 20;

        #! fields
        const Fields = {
            "items": {
                "display_name": "Items",
                "short_desc": "Initial items to include in the conversation context",
                "desc": sprintf("Initial items to include in the conversation context; a maximum of %d items may "
                    "be added at a time", MaxInitialItems),
                "type": new ListDataType("ConversationInitialItemList", OpenAiModelInputItemDataType, True),
            },
            "metadata": OpenAiConversationMetadataField,
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiConversationCreateRequest") {
        addQoreFields(Fields);
    }
}

#! Conversation create request type constant
/** @since OpenAiDataProvider 1.6
*/
public const OpenAiConversationCreateRequestType = new OpenAiConversationCreateRequestType();
}
