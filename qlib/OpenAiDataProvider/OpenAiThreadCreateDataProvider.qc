# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAiDataProvider module definition

/** OpenAiThreadCreateDataProvider.qc Copyright 2024 Qore Technologies, s.r.o.

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
public class OpenAiThreadCreateDataProvider inherits OpenAiDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "type": "OpenAiThreadCreateDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = OpenAiThreadCreateRequestType;

        #! Response type
        const ResponseType = OpenAiThreadCreateResponseDataType;
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
        hash<auto> rv = doRestCommand("POST", "threads", req, AssistantApiHdr).body;
        rv.created_at = localtime(rv.created_at);
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

#! The file upload request type
public class OpenAiThreadCreateRequestType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "messages": {
                "display_name": "Messages",
                "short_desc": "A list of messages to start the thread with",
                "desc": "A list of messages to start the thread with",
                "type": new ListDataType("Messages", OpenAiMessageDataType),
            },
            "tool_resources": {
                "display_name": "Tool Resources",
                "short_desc": "A set of resources that are used by the thread's tools",
                "desc": "A set of resources that are used by the thread's tools. The resources are specific to "
                    "the type of tool. For example, the code_interpreter` tool requires a list of file IDs, while "
                    "the `file_search` tool requires a list of vector store IDs",
                "type": OpenAiToolResourcesDataType.getOrNothingType(),
            },
            "metadata": {
                "display_name": "Metadata",
                "short_desc": "Set of 16 key-value pairs that can be attached to an object",
                "desc": "Set of 16 key-value pairs that can be attached to an object. This can be useful for "
                    "storing additional information about the object in a structured format. Keys can be a "
                    "maximum of 64 characters long and values can be a maxium of 512 characters long",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiThreadCreateRequest") {
        addQoreFields(Fields);
    }
}

#! File upload request type constant
public const OpenAiThreadCreateRequestType = new OpenAiThreadCreateRequestType();

#! Thread type
public class OpenAiThreadCreateResponseDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "id": {
                "display_name": "Thread ID",
                "short_desc": "Thread ID",
                "desc": "Thread ID",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "thread_ngyqsA9VoQGkhY9aD61b4AuX",
            },
            "object": {
                "display_name": "Object Type",
                "short_desc": "The type of object",
                "desc": "The type of object",
                "type": AbstractDataProviderTypeMap."string",
            },
            "created_at": {
                "display_name": "Creation Timestamp",
                "short_desc": "The date and time the thread was created",
                "desc": "The date and time the thread was created",
                "type": AbstractDataProviderTypeMap."date",
            },
        } + OpenAiThreadCreateRequestType::Fields{"metadata", "tool_resources"};
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiThreadCreateRequest") {
        addQoreFields(Fields);
    }
}

#! Thread type constant
public const OpenAiThreadCreateResponseDataType = new OpenAiThreadDataType();
}
