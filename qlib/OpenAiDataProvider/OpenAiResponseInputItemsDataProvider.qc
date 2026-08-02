# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAiDataProvider module definition

/** OpenAiResponseInputItemsDataProvider.qc Copyright 2024 - 2026 Qore Technologies, s.r.o.

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
#! Lists the input items used to generate a response
/** This is the Responses API equivalent of listing the steps of an Assistants API run.

    @since OpenAiDataProvider 1.6
*/
public class OpenAiResponseInputItemsDataProvider inherits OpenAiDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "input-items",
            "type": "OpenAiResponseInputItemsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = OpenAiResponseInputItemsRequestType;

        #! Response type
        const ResponseType = OpenAiResponseItemListDataType;
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
        string path = OpenAiDataProviderCommon::responsePath(req.id) + "/input_items"
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

#! Response input item list request type
/** @since OpenAiDataProvider 1.6
*/
public class OpenAiResponseInputItemsRequestType inherits HashDataType {
    public {
        #! The minimum page size accepted by the API
        const MinLimit = 1;

        #! The maximum page size accepted by the API
        const MaxLimit = 100;

        #! fields
        const Fields = OpenAiModelGetResponseRequestDataType::Fields + {
            "limit": {
                "display_name": "Limit",
                "short_desc": "The number of input items to return",
                "desc": sprintf("The number of input items to return; must be between `%d` and `%d`", MinLimit,
                    MaxLimit),
                "type": AbstractDataProviderTypeMap."*int",
                "example_value": 20,
            },
            "order": OpenAiConversationItemListRequestType::Fields.order + {
                "short_desc": "The order to return the input items in",
                "desc": "The order to return the input items in",
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
    constructor() : HashDataType("OpenAiResponseInputItemsRequest") {
        addQoreFields(Fields);
    }
}

#! Response input item list request type constant
/** @since OpenAiDataProvider 1.6
*/
public const OpenAiResponseInputItemsRequestType = new OpenAiResponseInputItemsRequestType();

#! Response item list type
/** @since OpenAiDataProvider 1.6
*/
public class OpenAiResponseItemListDataType inherits HashDataType {
    public {
        #! fields
        const Fields = OpenAiConversationItemListDataType::Fields + {
            "data": {
                "display_name": "Items",
                "short_desc": "The list of items used to generate this response",
                "desc": "The list of items used to generate this response",
                "type": new ListDataType("ResponseItemList", OpenAiConversationItemDataType),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiResponseItemList") {
        addQoreFields(Fields);
    }
}

#! Response item list type constant
/** @since OpenAiDataProvider 1.6
*/
public const OpenAiResponseItemListDataType = new OpenAiResponseItemListDataType();
}
