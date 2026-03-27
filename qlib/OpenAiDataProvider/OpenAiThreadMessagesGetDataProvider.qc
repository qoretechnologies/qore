# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAiDataProvider module definition

/** OpenAiThreadMessagesGetDataProvider.qc Copyright 2024 Qore Technologies, s.r.o.

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
public class OpenAiThreadMessagesGetDataProvider inherits OpenAiDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get",
            "type": "OpenAiThreadMessagesGetDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = OpenAiThreadIdRequestType;

        #! Response type
        const ResponseType = new ListDataType("*ThreadMessages", OpenAiThreadMessageDataType, True);
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
        list<auto> rv = doRestCommand("GET", "threads/" + req.thread_id + "/messages", NOTHING, AssistantApiHdr).body."data";
        foreach hash<auto> msg in (\rv) {
            msg.created_at = localtime(msg.created_at);
            if (msg.completed_at) {
                msg.created_at = localtime(msg.created_at);
            }
            if (msg.incomplete_at) {
                msg.incomplete_at = localtime(msg.incomplete_at);
            }
        }
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

#! Image file data type
public class OpenAiImageFileDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "file_id": {
                "display_name": "File ID",
                "short_desc": "The File ID of the image in the message content",
                "desc": "The File ID of the image in the message content. Set `purpose=\"vision\"` when uploading "
                    "the File if you need to later display the file content",
                "type": AbstractDataProviderTypeMap."string",
            },
            "detail": {
                "display_name": "Detail",
                "short_desc": "Specifies the detail level of the image",
                "desc": "Specifies the detail level of the image if specified by the user. `low` uses fewer tokens, "
                    "you can opt in to high resolution using `high`",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Low",
                        "short_desc": "Low detail",
                        "desc": "Low detail",
                        "value": "low",
                    },
                    <AllowedValueInfo>{
                        "display_name": "High",
                        "short_desc": "High detail",
                        "desc": "High detail",
                        "value": "high",
                    },
                ),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiImageFile") {
        addQoreFields(Fields);
    }
}

#! Image file data type constant
public const OpenAiImageFileDataType = new OpenAiImageFileDataType();

#! Image URL data type
public class OpenAiImageUrlDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "url": {
                "display_name": "URL",
                "short_desc": "The external URL of the image",
                "desc": "The external URL of the image, must be a supported image types: jpeg, jpg, png, gif, webp",
                "type": AbstractDataProviderTypeMap."string",
            },
            "detail": {
                "display_name": "Detail",
                "short_desc": "Specifies the detail level of the image",
                "desc": "Specifies the detail level of the image. `low` uses fewer tokens, you can opt in to high "
                    "resolution using `high`. Default value is `auto`",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Low",
                        "short_desc": "Low detail",
                        "desc": "Low detail",
                        "value": "low",
                    },
                    <AllowedValueInfo>{
                        "display_name": "High",
                        "short_desc": "High detail",
                        "desc": "High detail",
                        "value": "high",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Auto",
                        "short_desc": "Auto detail",
                        "desc": "Auto detail",
                        "value": "auto",
                    },
                ),
                "default_value": "auto",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiImageUrl") {
        addQoreFields(Fields);
    }
}

#! Image URL data type constant
public const OpenAiImageUrlDataType = new OpenAiImageUrlDataType();

#! Message text data type
public class OpenAiTextDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "value": {
                "display_name": "Value",
                "short_desc": "The text of the message",
                "desc": "The text of the message",
                "type": AbstractDataProviderTypeMap."string",
            },
            "annotations": {
                "display_name": "Annotations",
                "short_desc": "Annotations for the message",
                "desc": "Annotations for the message",
                "type": AbstractDataProviderTypeMap."*list",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiImageFile") {
        addQoreFields(Fields);
    }
}

#! Message text data type constant
public const OpenAiTextDataType = new OpenAiTextDataType();

#! Message content data type
public class OpenAiMessageContentDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "type": {
                "display_name": "ID",
                "short_desc": "The ID of the message",
                "desc": "The ID of the message",
                "type": AbstractDataProviderTypeMap."string",
            },
            "image_file": {
                "display_name": "Image File",
                "short_desc": "Image file information",
                "desc": "Image file information",
                "type": OpenAiImageFileDataType.getOrNothingType(),
            },
            "image_url": {
                "display_name": "Image URL",
                "short_desc": "Image URL information",
                "desc": "Image URL information",
                "type": OpenAiImageUrlDataType.getOrNothingType(),
            },
            "text": {
                "display_name": "Text",
                "short_desc": "Text information",
                "desc": "Text information",
                "type": OpenAiTextDataType.getOrNothingType(),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiTMessageContent") {
        addQoreFields(Fields);
    }
}

#! Message content data type
public const OpenAiMessageContentDataType = new OpenAiMessageContentDataType();

#! Open AI thread message data type
public class OpenAiThreadMessageDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "The ID of the message",
                "desc": "The ID of the message",
                "type": AbstractDataProviderTypeMap."string",
            },
            "object": {
                "display_name": "Object Type",
                "short_desc": "The object type",
                "desc": "The object type",
                "type": AbstractDataProviderTypeMap."string",
                "default_value": "thread.message",
            },
            "created_at": {
                "display_name": "Creation Timestamp",
                "short_desc": "The date and time the message was created",
                "desc": "The date and time the message was created",
                "type": AbstractDataProviderTypeMap."date",
            },
            "thread_id": {
                "display_name": "Thread ID",
                "short_desc": "The thread ID that this message belongs to",
                "desc": "The thread ID that this message belongs to",
                "type": AbstractDataProviderTypeMap."string",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "The status of the message",
                "desc": "The status of the message`",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "In Progress",
                        "short_desc": "The message is in progress",
                        "desc": "The message is in progress",
                        "value": "in_progress",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Incomplete",
                        "short_desc": "The message is incomplete",
                        "desc": "The message is incomplete",
                        "value": "incomplete",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Completed",
                        "short_desc": "The message is complete",
                        "desc": "The message is complete",
                        "value": "completed",
                    },
                ),
            },
            "incomplete_details": {
                "display_name": "Incomplete Details",
                "short_desc": "Details on why the message is incomplete",
                "desc": "Details on why the message is incomplete. Will be `null` if the message is not incomplete",
                "type": OpenAiIncompleteDetails.getOrNothingType(),
            },
            "completed_at": {
                "display_name": "Completion Timestamp",
                "short_desc": "The date and time the message was completed",
                "desc": "The date and time the message was completed",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "incomplete_at": {
                "display_name": "Incomplete Timestamp",
                "short_desc": "The date and time the message was marked as incomplete",
                "desc": "The date and time the message was marked as incomplete",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "role": {
                "display_name": "Role",
                "short_desc": "The role of the author of this message",
                "desc": "The role of the author of this message",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "User",
                        "short_desc": "The author of the message is the user",
                        "desc": "The author of the message is the user",
                        "value": "user",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Assistant",
                        "short_desc": "The author of the message is an assistant",
                        "desc": "The author of the message is an assistant",
                        "value": "assistant",
                    },
                ),
            },
            "content": {
                "display_name": "Content",
                "short_desc": "The content of the message in array of text and/or images",
                "desc": "The content of the message in array of text and/or images",
                "type": new ListDataType("MessageContentList", OpenAiMessageContentDataType),
            },
            "assistant_id": OpenAiRunCreateRequestType::Fields.assistant_id,
            "run_id": {
                "display_name": "Run ID",
                "short_desc": "The ID of the run associated with the creation of this message",
                "desc": "The ID of the run associated with the creation of this message. Value is `null` when "
                    "messages are created manually using the create message or create thread endpoints",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "attachments": {
                "display_name": "Attachments",
                "short_desc": "A list of files attached to the message, and the tools they were added to",
                "desc": "A list of files attached to the message, and the tools they were added to",
                "type": AbstractDataProviderTypeMap."*list",
            },
            "metadata": OpenAiAssistantCreateRequestType::Fields.metadata,
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiThreadMessage") {
        addQoreFields(Fields);
    }
}

#! Open AI thread message data type constant
public const OpenAiThreadMessageDataType = new OpenAiThreadMessageDataType();
}
