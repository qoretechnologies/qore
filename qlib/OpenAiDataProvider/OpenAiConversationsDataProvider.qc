# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAiDataProvider module definition

/** OpenAiConversationsDataProvider.qc Copyright 2024 - 2026 Qore Technologies, s.r.o.

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
#! The OpenAi conversations data provider class
/** Conversations are the Responses API replacement for the deprecated Assistants API threads; a conversation stores
    items (messages, tool calls, and tool call outputs) and can be attached to a response with the \c conversation
    request field.

    @see
    - @ref OpenAiDataProvider::OpenAiModelCreateResponseRequestDataType "OpenAiModelCreateResponseRequestDataType"
    - <a href="https://platform.openai.com/docs/api-reference/conversations">Conversations API</a>

    @since OpenAiDataProvider 1.6
*/
public class OpenAiConversationsDataProvider inherits OpenAiDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "conversations",
            "type": "OpenAiConversationsDataProvider",
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
            "create": Class::forName("OpenAiDataProvider::OpenAiConversationCreateDataProvider"),
            "delete": Class::forName("OpenAiDataProvider::OpenAiConversationDeleteDataProvider"),
            "get": Class::forName("OpenAiDataProvider::OpenAiConversationGetDataProvider"),
            "items": Class::forName("OpenAiDataProvider::OpenAiConversationItemsDataProvider"),
            "update": Class::forName("OpenAiDataProvider::OpenAiConversationUpdateDataProvider"),
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

#! Conversation metadata field definition shared by conversation request and response types
/** @since OpenAiDataProvider 1.6
*/
public const OpenAiConversationMetadataField = {
    "display_name": "Metadata",
    "short_desc": "Set of 16 key-value pairs that can be attached to the conversation",
    "desc": "Set of 16 key-value pairs that can be attached to an object. This can be useful for storing additional "
        "information about the object in a structured format, and querying for objects via API or the dashboard. "
        "Keys are strings with a maximum length of 64 characters; values are strings with a maximum length of 512 "
        "characters",
    "type": AbstractDataProviderTypeMap."*hash",
};

#! Conversation resource type
/** @since OpenAiDataProvider 1.6
*/
public class OpenAiConversationDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "id": {
                "display_name": "Conversation ID",
                "short_desc": "The unique ID of the conversation",
                "desc": "The unique ID of the conversation",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "conv_123",
            },
            "object": {
                "display_name": "Object Type",
                "short_desc": "The object type, which is always conversation",
                "desc": "The object type, which is always `conversation`",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "conversation",
            },
            "created_at": {
                "display_name": "Creation Timestamp",
                "short_desc": "The date and time the conversation was created",
                "desc": "The date and time the conversation was created",
                "type": AbstractDataProviderTypeMap."date",
            },
            "metadata": OpenAiConversationMetadataField,
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiConversation") {
        addQoreFields(Fields);
    }
}

#! Conversation resource type constant
/** @since OpenAiDataProvider 1.6
*/
public const OpenAiConversationDataType = new OpenAiConversationDataType();

#! Deleted conversation response type
/** @since OpenAiDataProvider 1.6
*/
public class OpenAiDeletedConversationDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "id": {
                "display_name": "Conversation ID",
                "short_desc": "The unique ID of the deleted conversation",
                "desc": "The unique ID of the deleted conversation",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "conv_123",
            },
            "object": {
                "display_name": "Object Type",
                "short_desc": "The object type, which is always conversation.deleted",
                "desc": "The object type, which is always `conversation.deleted`",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "conversation.deleted",
            },
            "deleted": {
                "display_name": "Deleted?",
                "short_desc": "Whether the conversation was deleted",
                "desc": "`True` if the conversation was deleted",
                "type": AbstractDataProviderTypeMap."bool",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiDeletedConversation") {
        addQoreFields(Fields);
    }
}

#! Deleted conversation response type constant
/** @since OpenAiDataProvider 1.6
*/
public const OpenAiDeletedConversationDataType = new OpenAiDeletedConversationDataType();

#! Conversation ID request type
/** @since OpenAiDataProvider 1.6
*/
public class OpenAiConversationIdRequestType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "conversation_id": {
                "display_name": "Conversation ID",
                "short_desc": "The ID of the conversation",
                "desc": "The ID of the conversation",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "conv_123",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiConversationIdRequest") {
        addQoreFields(Fields);
    }
}

#! Conversation ID request type constant
/** @since OpenAiDataProvider 1.6
*/
public const OpenAiConversationIdRequestType = new OpenAiConversationIdRequestType();

#! Conversation item type
/** Conversation items are a discriminated union keyed on \c type; messages, function calls, function call outputs,
    reasoning items, and built-in tool calls all appear here.  Only the fields common to every variant are declared;
    variant-specific fields are passed through unchanged.

    @since OpenAiDataProvider 1.6
*/
public class OpenAiConversationItemDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "id": {
                "display_name": "Item ID",
                "short_desc": "The unique ID of the item",
                "desc": "The unique ID of the item",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "msg_123",
            },
            "type": {
                "display_name": "Item Type",
                "short_desc": "The type of the item",
                "desc": "The type of the item; determines which additional fields are present (for example "
                    "`message`, `function_call`, `function_call_output`, `reasoning`, `web_search_call`)",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "message",
            },
            "role": {
                "display_name": "Role",
                "short_desc": "The role of the message item",
                "desc": "The role of the message item; only present for items with `type` = `message`",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "user",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "The status of the item",
                "desc": "The status of the item",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "The content of the message item",
                "desc": "The content of the message item; only present for items with `type` = `message`",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiConversationItem") {
        addQoreFields(Fields);
    }
}

#! Conversation item type constant
/** @since OpenAiDataProvider 1.6
*/
public const OpenAiConversationItemDataType = new OpenAiConversationItemDataType();

#! Conversation item list type
/** @since OpenAiDataProvider 1.6
*/
public class OpenAiConversationItemListDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "object": {
                "display_name": "Object Type",
                "short_desc": "The type of object returned, which is always list",
                "desc": "The type of object returned, which is always `list`",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "list",
            },
            "data": {
                "display_name": "Items",
                "short_desc": "The list of conversation items",
                "desc": "The list of conversation items",
                "type": new ListDataType("ConversationItemList", OpenAiConversationItemDataType),
            },
            "has_more": {
                "display_name": "Has More?",
                "short_desc": "Whether there are more items available",
                "desc": "Whether there are more items available",
                "type": AbstractDataProviderTypeMap."bool",
            },
            "first_id": {
                "display_name": "First ID",
                "short_desc": "The ID of the first item in the list",
                "desc": "The ID of the first item in the list",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "last_id": {
                "display_name": "Last ID",
                "short_desc": "The ID of the last item in the list",
                "desc": "The ID of the last item in the list",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiConversationItemList") {
        addQoreFields(Fields);
    }
}

#! Conversation item list type constant
/** @since OpenAiDataProvider 1.6
*/
public const OpenAiConversationItemListDataType = new OpenAiConversationItemListDataType();
}
