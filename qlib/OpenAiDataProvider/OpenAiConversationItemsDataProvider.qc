# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAiDataProvider module definition

/** OpenAiConversationItemsDataProvider.qc Copyright 2024 - 2026 Qore Technologies, s.r.o.

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
#! The OpenAi conversation items data provider class
/** @since OpenAiDataProvider 1.6
*/
public class OpenAiConversationItemsDataProvider inherits OpenAiDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "items",
            "type": "OpenAiConversationItemsDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! Child data providers
        const ChildMap = {
            "create": Class::forName("OpenAiDataProvider::OpenAiConversationItemCreateDataProvider"),
            "delete": Class::forName("OpenAiDataProvider::OpenAiConversationItemDeleteDataProvider"),
            "get": Class::forName("OpenAiDataProvider::OpenAiConversationItemGetDataProvider"),
            "list": Class::forName("OpenAiDataProvider::OpenAiConversationItemListDataProvider"),
        };
    }

    #! Creates the object from the arguments
    constructor(*RestClientIo::RestClientIo rest) : OpenAiDataProviderCommon(rest) {
    }

    #! Creates the object from constructor options
    constructor(hash<auto> options) : OpenAiDataProviderCommon(options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Additional item data to return in the response, shared by conversation item requests
/** @since OpenAiDataProvider 1.6
*/
public const OpenAiConversationItemIncludeField = {
    "display_name": "Include",
    "short_desc": "Additional fields to include in the response",
    "desc": "Additional fields to include in the response.\n\n"
        "See the [include parameter](https://platform.openai.com/docs/api-reference/responses/create#responses_create-include) "
        "documentation for the supported values",
    "type": AbstractDataProviderTypeMap."*softlist",
    "example_value": ("message.input_image.image_url",),
};

#! Conversation item ID request type
/** @since OpenAiDataProvider 1.6
*/
public class OpenAiConversationItemIdRequestType inherits HashDataType {
    public {
        #! fields
        const Fields = OpenAiConversationIdRequestType::Fields + {
            "item_id": {
                "display_name": "Item ID",
                "short_desc": "The ID of the item within the conversation",
                "desc": "The ID of the item within the conversation",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "msg_abc",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiConversationItemIdRequest") {
        addQoreFields(Fields);
    }
}

#! Conversation item ID request type constant
/** @since OpenAiDataProvider 1.6
*/
public const OpenAiConversationItemIdRequestType = new OpenAiConversationItemIdRequestType();

#! Action options for actions addressing a single conversation item
/** These are declared inline rather than derived with
    @ref DataProvider::DataProviderActionCatalog::getActionOptionFromFields() "getActionOptionFromFields()"
    because that helper does not propagate the cascading-dropdown metadata (\c has_dependents, \c depends_on,
    and \c on_change) needed to filter the item list by the selected conversation.

    @note \c conversation_id has no \c ref_data because OpenAI exposes no endpoint that lists conversations; it
    must be entered manually or supplied from an upstream step.

    @since OpenAiDataProvider 1.6
*/
public const OpenAiConversationItemActionOptions = {
    "conversation_id": <ActionOptionInfo>{
        "display_name": "Conversation ID",
        "short_desc": "The ID of the conversation holding the item",
        "desc": "The ID of the conversation holding the item",
        "type": AbstractDataProviderTypeMap."string",
        "required": True,
        "example_value": "conv_123",
        "has_dependents": True,
        "on_change": ("refetch",),
    },
    "item_id": <ActionOptionInfo>{
        "display_name": "Item",
        "short_desc": "The item within the conversation",
        "desc": "The item within the conversation; the list is populated from the selected conversation",
        "type": AbstractDataProviderTypeMap."string",
        "required": True,
        "example_value": "msg_abc",
        "depends_on": ("conversation_id",),
        "ref_data": "conversation_items",
    },
};
}
