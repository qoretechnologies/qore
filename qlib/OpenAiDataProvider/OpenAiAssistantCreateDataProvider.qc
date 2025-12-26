# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAiDataProvider module definition

/** OpenAiAssistantCreateDataProvider.qc Copyright 2024 Qore Technologies, s.r.o.

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
public class OpenAiAssistantCreateDataProvider inherits OpenAiDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-assistant",
            "type": "OpenAiAssistantCreateDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = OpenAiAssistantCreateRequestType;

        #! Response type
        const ResponseType = OpenAiAssistantDataType;
    }

    #! Creates the object from the arguments
    constructor(*RestClient rest) : OpenAiDataProviderCommon(rest) {
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
        hash<auto> rv = doRestCommand("POST", "assistants", req, AssistantApiHdr).body;
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

#! The assistant tool data type
public class OpenAiAssistantToolDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "type": {
                "display_name": "Type",
                "short_desc": "The type of the tool",
                "desc": "The type of tool being defined",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Code Interpreter",
                        "short_desc": "For a code interpreter tool",
                        "desc": "For a code interpreter tool",
                        "value": "code_interpreter",
                    },
                    <AllowedValueInfo>{
                        "display_name": "File Search",
                        "short_desc": "For a file search tool",
                        "desc": "For a file search tool",
                        "value": "file_search",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Function",
                        "short_desc": "For a function",
                        "desc": "For a function",
                        "value": "function",
                    },
                ),
            },
            "function": OpenAiToolDataType::Fields.function + {
                "type": OpenAiFunctionDataType.getOrNothingType(),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiAssistantTool") {
        addQoreFields(Fields);
    }
}

#! Constant for the assistant tool data type
public const OpenAiAssistantToolDataType = new OpenAiAssistantToolDataType();

#! The code interpreter type
public class OpenAiCodeInterpreterDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "file_ids": {
                "display_name": "File IDs",
                "short_desc": "File ID resources for code interpreters",
                "desc": "A list of file IDs made available to the code_interpreter tool. There can be a maximum of "
                    "20 files associated with the tool",
                "type": new ListDataType("*FileIdList", AbstractDataProviderTypeMap."string", True),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiCodeInterpreter") {
        addQoreFields(Fields);
    }
}

#! Constant for the code interpreter data type
public const OpenAiCodeInterpreterDataType = new OpenAiCodeInterpreterDataType();

#! The vector store definition type
public class OpenAiVectorStoreDefinitionDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "file_ids": {
                "display_name": "File IDs",
                "short_desc": "A list of file IDs to add to the vector store",
                "desc": "A list of file IDs to add to the vector store. There can be a maximum of 10000 files in a "
                    "vector store",
                "type": new ListDataType("*FileIdList", AbstractDataProviderTypeMap."string", True),
            },
            "metadata": {
                "display_name": "Metadata",
                "short_desc": "Set of 16 key-value pairs that can be attached to a vector store",
                "desc": "Set of 16 key-value pairs that can be attached to a vector store. This can be useful for "
                    "storing additional information about the vector store in a structured format. Keys can be a "
                    "maximum of 64 characters long and values can be a maxium of 512 characters long",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiVectorStoreDefinition") {
        addQoreFields(Fields);
    }
}

#! Constant for the vector store definition type
public const OpenAiVectorStoreDefinitionDataType = new OpenAiVectorStoreDefinitionDataType();

#! The vector store type
public class OpenAiVectorStoreDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "vector_store_ids": {
                "display_name": "Vector Store IDs",
                "short_desc": "Vector store IDs attached to this assistant",
                "desc": "The vector store attached to this assistant. There can be a maximum of 1 vector store "
                    "attached to the assistant",
                "type": new ListDataType("*VectorStoreIdList", AbstractDataProviderTypeMap."string", True),
            },
            "vector_stores": {
                "display_name": "Vector Stores",
                "short_desc": "A helper to create a vector store with file_ids and attach it to this assistant",
                "desc": "A helper to create a vector store with file_ids and attach it to this assistant. There can "
                    "be a maximum of 1 vector store attached to the assistant",
                "type": new ListDataType("*VectorStoreList", OpenAiVectorStoreDefinitionDataType, True),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiVectorStore
    ") {
        addQoreFields(Fields);
    }
}

#! Constant for the vector store type
public const OpenAiVectorStoreDataType = new OpenAiVectorStoreDataType();

#! The tool resources data type
public class OpenAiToolResourcesDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "code_interpreter": {
                "display_name": "Code Interpreter",
                "short_desc": "File ID resources for code interpreters",
                "desc": "File ID resources for code interpreters",
                "type": new ListDataType("*CodeInterpreterList", OpenAiCodeInterpreterDataType, True),
            },
            "file_search": {
                "display_name": "File Search",
                "short_desc": "Vector store information attached to the assistant",
                "desc": "Vector store information attached to the assistant",
                "type": new ListDataType("*VectorStoreList", OpenAiVectorStoreDataType, True),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiToolResources") {
        addQoreFields(Fields);
    }
}

#! Constant for the tool resources data type
public const OpenAiToolResourcesDataType = new OpenAiToolResourcesDataType();

#! The Assistant create request type
public class OpenAiAssistantCreateRequestType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "model": {
                "display_name": "Model ID",
                "short_desc": "Model for the assistant",
                "desc": "Model for the assistant",
                "type": AbstractDataProviderTypeMap."string",
                # for dynamic lookups of allowed values
                "attr": {
                    "ref_data": "models",
                },
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The name of the assistant",
                "desc": "The name of the assistant. The maximum length is 256 characters",
                "type": AbstractDataProviderTypeMap."*string",
                "attr": {
                    "preselected": True,
                },
            },
            "description": {
                "display_name": "Description",
                "short_desc": "The description of the assistant",
                "desc": "The description of the assistant. The maximum length is 512 characters",
                "type": AbstractDataProviderTypeMap."*string",
                "attr": {
                    "preselected": True,
                },
            },
            "instructions": {
                "display_name": "Instructions",
                "short_desc": "The system instructions that the assistant uses",
                "desc": "The system instructions that the assistant uses. The maximum length is 256,000 characters",
                "type": AbstractDataProviderTypeMap."*string",
                "attr": {
                    "preselected": True,
                },
            },
            "tools": {
                "display_name": "Tools",
                "short_desc": "A list of tools enabled on the assistant",
                "desc": "A list of tools enabled on the assistant. There can be a maximum of 128 tools per assistant. "
                    "Tools can be of types `code_interpreter`, `file_search`, or `function`",
                "type": new ListDataType("*AssistantToolList", OpenAiAssistantToolDataType, True),
            },
            "tool_resources": {
                "display_name": "Tool Resources",
                "short_desc": "A set of resources that are used by the assistant's tools",
                "desc": "A set of resources that are used by the assistant's tools. The resources are specific to "
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
            }
        } + OpenAiChatCompletionRequestType::Fields{"temperature", "top_p", "response_format"};
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiAssistantCreateRequest") {
        addQoreFields(Fields);
    }
}

#! Assistant create request type constant
public const OpenAiAssistantCreateRequestType = new OpenAiAssistantCreateRequestType();

#! Assistant type
public class OpenAiAssistantDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "id": {
                "display_name": "Assistant ID",
                "short_desc": "Assistant ID",
                "desc": "Assistant ID",
                "type": AbstractDataProviderTypeMap."string",
            },
            "object": {
                "display_name": "Object Type",
                "short_desc": "The type of object",
                "desc": "The type of object",
                "type": AbstractDataProviderTypeMap."string",
            },
            "created_at": {
                "display_name": "Creation Timestamp",
                "short_desc": "The date and time the assistant was created",
                "desc": "The date and time the assistant was created",
                "type": AbstractDataProviderTypeMap."date",
            },
        } + OpenAiAssistantCreateRequestType::Fields{"model", "name", "description", "instructions", "tools",
            "tool_resources", "metadata", "temperature", "top_p", "response_format"};
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiAssistantCreateRequest") {
        addQoreFields(Fields);
    }
}

#! Assistant type constant
public const OpenAiAssistantDataType = new OpenAiAssistantDataType();
}
