# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAiDataProvider module definition

/** OpenAiConversationItemListDataProvider.qc Copyright 2024 - 2026 Qore Technologies, s.r.o.

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
#! Lists items in a conversation
/** @since OpenAiDataProvider 1.6
*/
public class OpenAiConversationItemListDataProvider inherits OpenAiDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "list",
            "type": "OpenAiConversationItemListDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = OpenAiConversationItemListRequestType;

        #! Response type
        const ResponseType = OpenAiConversationItemListDataType;
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
        string path = OpenAiDataProviderCommon::conversationPath(req.conversation_id) + "/items"
            + OpenAiDataProviderCommon::makeQueryString(req{"after", "include", "limit", "order"});
        return doRestCommand("GET", path).body;
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

#! Conversation item list request type
/** @since OpenAiDataProvider 1.6
*/
public class OpenAiConversationItemListRequestType inherits HashDataType {
    public {
        #! The default page size used by the API when \c limit is not given
        const DefaultLimit = 20;

        #! fields
        const Fields = OpenAiConversationIdRequestType::Fields + {
            "limit": {
                "display_name": "Limit",
                "short_desc": "The number of items to return",
                "desc": sprintf("The number of items to return; defaults to `%d` server-side", DefaultLimit),
                "type": AbstractDataProviderTypeMap."*int",
                "example_value": DefaultLimit,
            },
            "order": {
                "display_name": "Order",
                "short_desc": "The sort order of the returned items by creation timestamp",
                "desc": "The sort order of the returned items by creation timestamp",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Ascending",
                        "short_desc": "Oldest items first",
                        "desc": "Return items in ascending order by creation timestamp (oldest first)",
                        "value": "asc",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Descending",
                        "short_desc": "Newest items first",
                        "desc": "Return items in descending order by creation timestamp (newest first)",
                        "value": "desc",
                    },
                ),
            },
            "after": {
                "display_name": "After",
                "short_desc": "An item ID to list items after, used in pagination",
                "desc": "An item ID to list items after, used in pagination",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "include": OpenAiConversationItemIncludeField,
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiConversationItemListRequest") {
        addQoreFields(Fields);
    }
}

#! Conversation item list request type constant
/** @since OpenAiDataProvider 1.6
*/
public const OpenAiConversationItemListRequestType = new OpenAiConversationItemListRequestType();
}
