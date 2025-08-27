# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAiDataProvider module definition

/** OpenAiModelCreateResponseDataProvider.qc Copyright 2024 Qore Technologies, s.r.o.

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
public class OpenAiModelCreateResponseDataProvider inherits OpenAiDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-response",
            "type": "OpenAiModelCreateResponseDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = OpenAiModelCreateResponseRequestDataType;

        #! Response type
        const ResponseType = OpenAiModelCreateResponseResponseDataType;
    }

    private {
        #! Model info
        hash<auto> model;
    }

    #! Creates the object from the arguments
    constructor(*RestClient rest) : OpenAiDataProviderCommon(rest) {
    }

    #! Creates the object from the arguments
    constructor(*RestClient rest, hash<auto> model) : OpenAiDataProviderCommon(rest) {
        self.model = model;
    }

    #! Returns the data provider name
    string getName() {
        return "create-response";
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # process request input to the OpenAI type
        if (req.input) {
            req.input = map getInput($1), req.input;
            if (req.input.lsize() == 1 && (req.input[0].typeCode() == NT_STRING)) {
                req.input = req.input[0];
            }
        }
        if (req.tools) {
            req.tools = map getTool($1), req.tools;
        }
        if (model) {
            req.model = model.id;
        }
        hash<auto> rv = doRestCommand("POST", "responses", req).body;
        if (rv.output) {
            rv.output_text = getOutputText(rv.output);
        }
        return rv;
    }

    #! Processes input in a request
    private static auto getInput(hash<auto> input) {
        auto rv = input.firstValue();
        if (rv.content) {
            rv.content = map getContent($1), rv.content;
            if (rv.content.lsize() == 1 && (rv.content[0].typeCode() == NT_STRING)) {
                rv.content = rv.content[0];
            }
        }
        return rv;
    }

    #! Processes content in a request
    private static auto getContent(hash<auto> content) {
        if (content.file) {
            return {
                "type": "input_file",
                "file_data": "data:application/pdf;base64," + content.file.content,
                "filename": content.file.name,
            };
        }
        return content.firstValue();
    }

    #! Processes tools in a response request
    private static hash<auto> getTool(hash<auto> tool) {
        hash<auto> rv = tool.firstValue();
        if (rv.filters) {
            rv.filters = getFilter($1);
        }
        return rv;
    }

    private static hash<auto> getFilter(hash<auto> filter) {
        hash<auto> rv = filter.firstValue();;
        if (rv.filters) {
            rv.filters = map getFilter($1), rv.filters;
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

    #! Returns output text for the output
    *string getOutputText(list<auto> output) {
        string rv;
        foreach hash<auto> h0 in (output) {
            foreach hash<auto> h1 in (h0.content) {
                if (h1.text) {
                    if (rv.val()) {
                        rv += "\n\n";
                    }
                    rv += h1.text;
                }
            }
        }
        return rv;
    }
}

public class OpenAiModelInputItemContentDataType inherits HashDataType {
    public {
        const Fields = {
            "file": {
                "display_name": "File Input",
                "short_desc": "File input to the model, used to generate a response",
                "desc": "File input to the model, used to generate a response",
                "type": new FileOrNothingDataType({"accept": MimeTypePdf}),
                "attr": {
                    "exclusive_with": ("text",),
                },
            },
            "text": {
                "display_name": "Text",
                "short_desc": "The text content of the input item",
                "desc": "The text content of the input item",
                "type": AbstractDataProviderTypeMap."*string",
                "attr": {
                    "exclusive_with": ("file",),
                },
            },
            /*
            "image": {
                "display_name": "Image Reference",
                "short_desc": "The image content of the input item",
                "desc": "The image content of the input item",
                "type": OpenAiModelInputImageReferenceDataType,
            },
            */
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelInputItemContentDataType = new OpenAiModelInputItemContentDataType();

public class OpenAiModelInputItemDataType inherits HashDataType {
    public {
        const Fields = {
            "content": {
                "display_name": "Content",
                "short_desc": "Text, image, or audio input to the model, used to generate a response",
                "desc": "Text, image, or audio input to the model, used to generate a response. Can also contain "
                    "previous assistant responses",
                "type": new SoftListDataType("ContentList", OpenAiModelInputItemContentDataType),
            },
            "role": {
                "display_name": "Role",
                "short_desc": "The role of the message input",
                "desc": "The role of the message input",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "User",
                        "value": "user",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Assistant",
                        "value": "assistant",
                    },
                    <AllowedValueInfo>{
                        "display_name": "System",
                        "value": "system",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Developer",
                        "value": "developer",
                    },
                ),
                "default_value": "user",
            },
            "type": {
                "display_name": "Type",
                "short_desc": "The type of the message input",
                "desc": "The type of the message input",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Message",
                        "value": "message",
                    },
                ),
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelInputItemDataType = new OpenAiModelInputItemDataType();

public class OpenAiModelInputDataType inherits HashDataType {
    public {
        const Fields = {
            "input_item": {
                "display_name": "Input Item",
                "short_desc": "The input item to the model",
                "desc": "The input item to the model",
                "type": OpenAiModelInputItemDataType,
            },
            "text": {
                "display_name": "Text Input",
                "short_desc": "Text input to the model, used to generate a response",
                "desc": "Text input to the model, used to generate a response",
                "type": AbstractDataProviderTypeMap."*string",
                "attr": {
                    "exclusive_with": ("file", "fileref"),
                },
            },
            /*
            "fileref": {
                "display_name": "File Reference",
                "short_desc": "A reference to a stored file of an image, used to generate a response",
                "desc": "A reference to a stored file of an image, used to generate a response",
                "type": AbstractDataProviderTypeMap."*string",
                "attr": {
                    "exclusive_with": ("file", "text"),
                },
            },
            */
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelInputDataType = new OpenAiModelInputDataType();

public class ReasoningDataType inherits HashDataType{
    public {
        const Fields = {
            "effort": {
                "display_name": "Effort",
                "short_desc": "Constrains effort on reasoning for reasoning models",
                "desc": "Constrains effort on reasoning for reasoning models. Reducing reasoning effort can result "
                    "in faster responses and fewer tokens used on reasoning in a response.",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Low",
                        "desc": "Low effort",
                        "value": "low",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Medium",
                        "desc": "Medium effort",
                        "value": "medium",
                    },
                    <AllowedValueInfo>{
                        "display_name": "High",
                        "desc": "High effort",
                        "value": "high",
                    },
                ),
                "type": AbstractDataProviderTypeMap."string",
                "default_value": "medium",
            },
            "summary": {
                "display_name": "Summary",
                "short_desc": "A summary of the reasoning performed by the model",
                "desc": "A summary of the reasoning performed by the model. This can be useful for debugging and "
                    "understanding the model's reasoning process",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Auto",
                        "value": "auto",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Concise",
                        "value": "concise",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Detailed",
                        "value": "detailed",
                    },
                ),
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const ReasoningDataType = new ReasoningDataType();
public const ReasoningOrNothingDataType = ReasoningDataType.getOrNothingType();

public class OpenAiModelRepsonseTextFormatDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Format Type",
                "short_desc": "The format that the model must output",
                "desc": "The format that the model must output",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Text",
                        "desc": "Free-form text output",
                        "value": "text",
                    },
                    <AllowedValueInfo>{
                        "display_name": "JSON Schema",
                        "desc": "Enables Structured Outputs, which ensures the model will match your supplied JSON "
                            "schema",
                        "value": "json_schema",
                    },
                    <AllowedValueInfo>{
                        "display_name": "JSON Object",
                        "desc": "Enables the older JSON mode, which ensures the message the model generates is valid "
                            "JSON. Using `json_schema` is preferred for models that support it",
                        "value": "json_object",
                    },
                ),
                "default_value": "text",
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelRepsonseTextFormatDataType = new OpenAiModelRepsonseTextFormatDataType();

public class OpenAiModelResponseTextDataType inherits HashDataType {
    public {
        const Fields = {
            "format": {
                "display_name": "Model Output Format Type",
                "short_desc": "The format that the model must output",
                "desc": "The format that the model must output",
                "type": OpenAiModelRepsonseTextFormatDataType,
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelResponseTextDataType = new OpenAiModelResponseTextDataType();
public const OpenAiModelResponseTextOrNothingDataType = OpenAiModelResponseTextDataType.getOrNothingType();

public class OpenAiModelFileSearchToolComparisonFilterDataType inherits HashDataType {
    public {
        const Fields = {
            "key": {
                "display_name": "Key",
                "short_desc": "The key to compare against the value",
                "desc": "The key to compare against the value",
                "type": AbstractDataProviderTypeMap."string",
            },
            "type": {
                "display_name": "Type",
                "short_desc": "Specifies the comparison operator",
                "desc": "Specifies the comparison operator",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Equals",
                        "value": "eq",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Not Equals",
                        "value": "ne",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Greater Than",
                        "value": "gt",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Greater Than or Equals",
                        "value": "gte",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Less Than",
                        "value": "lt",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Less Than or Equals",
                        "value": "lte",
                    },
                ),
            },
            "value": {
                "display_name": "Value",
                "short_desc": "The value to compare against",
                "desc": "The value to compare against; supports `string`, `number`, and `boolean` types",
                "type": AbstractDataProviderTypeMap."auto",
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelFileSearchToolComparisonFilterDataType = new OpenAiModelFileSearchToolComparisonFilterDataType();

public class OpenAiModelFileSearchToolCompoundFilterDataType inherits HashDataType {
    public {
        const Fields = {
            # NOTE the "filters" field requires a reference back to this type
            "filters": {
                "display_name": "Filters",
                "short_desc": "The filters to apply to the search",
                "desc": "The filters to apply to the search",
                "type": AbstractDataProviderTypeMap."softlist",
            },
            "operator": {
                "display_name": "Operator",
                "short_desc": "The operator to use for the compound filter",
                "desc": "The operator to use for the compound filter",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "And",
                        "desc": "And",
                        "value": "and",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Or",
                        "desc": "Or",
                        "value": "or",
                    },
                ),
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

# NOTE: the "filters" field is updated in the module init code to be self-referencing
public const OpenAiModelFileSearchToolCompoundFilterDataType = new OpenAiModelFileSearchToolCompoundFilterDataType();

public class OpenAiModelFileSearchToolFilterDataType inherits HashDataType {
    public {
        const Fields = {
            "comparison": {
                "display_name": "Comparison Filter",
                "short_desc": "A comparison filter to apply to the search",
                "desc": "A comparison filter to apply to the search",
                "type": OpenAiModelFileSearchToolComparisonFilterDataType,
                "exclusive_with": ("compound",),
            },
            "compound": {
                "display_name": "Compound Filter",
                "short_desc": "A compound filter to apply to the search",
                "desc": "A compound filter to apply to the search",
                "type": OpenAiModelFileSearchToolCompoundFilterDataType,
                "exclusive_with": ("comparison",),
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelFileSearchToolFilterDataType = new OpenAiModelFileSearchToolFilterDataType();
public const OpenAiModelFileSearchToolFilterOrNothingDataType =
    OpenAiModelFileSearchToolFilterDataType.getOrNothingType();

public class OpenAiModelFileSearchToolRankingOptionsDataType inherits HashDataType {
    public {
        const Fields = {
            "ranker": {
                "display_name": "Ranker",
                "short_desc": "The ranker to use for the file search",
                "desc": "The ranker to use for the file search",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "score_threshold": {
                "display_name": "Score Threshold",
                "short_desc": "The score threshold for the file search, between 0 and 1",
                "desc": "The score threshold for the file search, a number between 0 and 1. Numbers closer to 1 will "
                    "attempt to return only the most relevant results, but may return fewer results",
                "type": AbstractDataProviderTypeMap."*float",
            }
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelFileSearchToolRankingOptionsDataType = new OpenAiModelFileSearchToolRankingOptionsDataType();
public const OpenAiModelFileSearchToolRankingOptionsOrNothingDataType =
    OpenAiModelFileSearchToolRankingOptionsDataType.getOrNothingType();

public class OpenAiModelFileSearchToolDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Type",
                "short_desc": "The type of the file search tool",
                "desc": "The type of the file search tool",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "File Search",
                        "desc": "A file search tool",
                        "value": "file_search",
                    },
                ),
                "default_value": "file_search",
            },
            "vector_store_ids": {
                "display_name": "Vector Store IDs",
                "short_desc": "The IDs of the vector stores to search",
                "desc": "The IDs of the vector stores to search",
                "type": new ListDataType("VectorStoreIdList", AbstractDataProviderTypeMap."string"),
            },
            "filters": {
                "display_name": "Filters",
                "short_desc": "The filters to apply to the search",
                "desc": "The filters to apply to the search",
                "type": OpenAiModelFileSearchToolFilterOrNothingDataType,
            },
            "max_num_results": {
                "display_name": "Max Number of Results",
                "short_desc": "The maximum number of results to return from the search",
                "desc": "The maximum number of results to return. This number should be between 1 and 50 inclusive",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "ranking_options": {
                "display_name": "Ranking Options",
                "short_desc": "The ranking options to use for the search",
                "desc": "The ranking options to use for the search",
                "type": OpenAiModelFileSearchToolRankingOptionsOrNothingDataType,
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelFileSearchToolDataType = new OpenAiModelFileSearchToolDataType();

public class OpenAiModelFunctionToolDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "display_name": "Function Name",
                "short_desc": "The name of the function to call",
                "desc": "The name of the function to call",
                "type": AbstractDataProviderTypeMap."string",
            },
            "parameters": {
                "display_name": "Function Parameters",
                "short_desc": "A JSON schema object describing the parameters of the function",
                "desc": "A JSON schema object describing the parameters of the function",
                "type": AbstractDataProviderTypeMap."hash",
            },
            "strict": {
                "display_name": "Strict?",
                "short_desc": "Whether to enforce strict parameter validation",
                "desc": "Whether to enforce strict parameter validation",
                "type": AbstractDataProviderTypeMap."bool",
                "default_value": True,
            },
            "type": {
                "display_name": "Type",
                "short_desc": "The type of the function tool",
                "desc": "The type of the function tool",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Function",
                        "desc": "A function tool",
                        "value": "function",
                    },
                ),
                "default_value": "function",
            },
            "description": {
                "display_name": "Function Description",
                "short_desc": "A description of the function used by the model to determine whether or not to call "
                    "it",
                "desc": "A description of the function used by the model to determine whether or not to call it",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelFunctionToolDataType = new OpenAiModelFunctionToolDataType();

public class OpenAiModelUserLocationDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Type",
                "short_desc": "The type of location approximation",
                "desc": "The type of location approximation",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "User Location",
                        "value": "approximate",
                    },
                ),
                "default_value": "approximate",
            },
            "city": {
                "display_name": "City",
                "short_desc": "Free text input for the city of the user",
                "desc": "Free text input for the city of the user",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "country": {
                "display_name": "Country",
                "short_desc": "The two-letter ISO country code of the user",
                "desc": "The two-letter ISO country code of the user",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "CZ",
            },
            "region": {
                "display_name": "Region",
                "short_desc": "Free text input for the region of the user",
                "desc": "Free text input for the region of the user",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "Prague",
            },
            "timezone": {
                "display_name": "Timezone",
                "short_desc": "The IANA timezone of the user",
                "desc": "The IANA timezone of the user",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "Europe/Prague",
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelUserLocationDataType = new OpenAiModelUserLocationDataType();
public const OpenAiModelUserLocationOrNothingDataType = OpenAiModelUserLocationDataType.getOrNothingType();

public class OpenAiModelWebSearchToolDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Type",
                "short_desc": "The type of the web search tool",
                "desc": "The type of the web search tool",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Web Search Preview",
                        "value": "web_search_preview",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Web Search Preview 2025-03-11",
                        "value": "web_search_preview_2025_03_11",
                    },
                ),
                "default_value": "web_search_preview_2025_03_11",
            },
            "search_context_size": {
                "display_name": "Search Context Size",
                "short_desc": "High level guidance for the amount of context window space to use for the search",
                "desc": "High level guidance for the amount of context window space to use for the search",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Low",
                        "value": "low",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Medium",
                        "value": "medium",
                    },
                    <AllowedValueInfo>{
                        "display_name": "High",
                        "value": "high",
                    },
                ),
                "default_value": "medium",
            },
            "user_location": {
                "display_name": "User Location",
                "short_desc": "The location of the user making the request",
                "desc": "The location of the user making the request",
                "type": OpenAiModelUserLocationOrNothingDataType,
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelWebSearchToolDataType = new OpenAiModelWebSearchToolDataType();

public class OpenAiModelComputerUseToolDataType inherits HashDataType {
    public {
        const Fields = {
            "display_height": {
                "display_name": "Display Height",
                "short_desc": "The height of the computer display",
                "desc": "The height of the computer display",
                "type": AbstractDataProviderTypeMap."int",
            },
            "display_width": {
                "display_name": "Display Width",
                "short_desc": "The width of the computer display",
                "desc": "The width of the computer display",
                "type": AbstractDataProviderTypeMap."int",
            },
            "environment": {
                "display_name": "Environment",
                "short_desc": "The type of computer environment to control",
                "desc": "The type of computer environment to control",
                "type": AbstractDataProviderTypeMap."string",
            },
            "type": {
                "display_name": "Type",
                "short_desc": "The type of the computer use tool",
                "desc": "The type of the computer use tool",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Computer Use Preview",
                        "value": "computer_use_preview",
                    },
                ),
                "default_value": "computer_use_preview",
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelComputerUseToolDataType = new OpenAiModelComputerUseToolDataType();

public class OpenAiModelToolDataType inherits HashDataType {
    public {
        const Fields = {
            "file_search": {
                "display_name": "File Search Tool",
                "short_desc": "A tool that searches for relevant content from uploaded files",
                "desc": "A tool that searches for relevant content from uploaded files",
                "type": OpenAiModelFileSearchToolDataType,
                "exclusive_with": ("function", "web_search", "computer_use", "mcp"),
            },
            "function": {
                "display_name": "Function Tool",
                "short_desc": "Defines a function in your own code the model can choose to call",
                "desc": "Defines a function in your own code the model can choose to call",
                "type": OpenAiModelFunctionToolDataType,
                "exclusive_with": ("file_search", "web_search", "computer_use", "mcp"),
            },
            "web_search": {
                "display_name": "Web Search Tool",
                "short_desc": "This tool searches the web for relevant results to use in a response",
                "desc": "This tool searches the web for relevant results to use in a response",
                "type": OpenAiModelWebSearchToolDataType,
                "exclusive_with": ("file_search", "function", "computer_use", "mcp"),
            },
            "computer_use": {
                "display_name": "Computer Use Tool",
                "short_desc": "A tool that controls a virtual computer",
                "desc": "A tool that controls a virtual computer",
                "type": OpenAiModelComputerUseToolDataType,
                "exclusive_with": ("file_search", "function", "web_search", "mcp"),
            },
            "mcp": {
                "display_name": "MCP Tool",
                "short_desc": "Gives the model access to additional tools through the Model Context Protocol",
                "desc": "Gives the model access to additional tools through the Model Context Protocol",
                "type": new HashDataType(AutoHashOrNothingType, "McpTool", (
                    map {$1.key: new QoreDataField({"name": $1.key} + $1.value)}, {
                        "server_label": {
                            "display_name": "Server Label",
                            "type": AbstractDataProviderTypeMap."string",
                            "desc": "A label for this MCP server, used to identify it in tool calls",
                        },
                        "type": {
                            "display_name": "Type",
                            "type": AbstractDataProviderTypeMap."string",
                            "desc": "The type of the MCP server",
                            "allowed_values": (
                                <AllowedValueInfo>{
                                    "display_name": "MCP",
                                    "value": "mcp",
                                },
                            ),
                            "default_value": "mcp",
                        },
                        "allowed_tools": {
                            "display_name": "Allowed Tools",
                            "type": AbstractDataProviderTypeMap."any",
                            "desc": "List of allowed tool names or a filter object",
                        },
                        "authorization": {
                            "display_name": "Authorization Token",
                            "type": AbstractDataProviderTypeMap."*string",
                            "desc": "The authorization token to use when connecting to the MCP server",
                        },
                        "connector_id": {
                            "display_name": "Connector ID",
                            "type": AbstractDataProviderTypeMap."*string",
                            "desc": "Identifier for service connectors, like those available in ChatGPT. One of "
                                "`server_url` or `connector_id` must be provided",
                            "allowed_values": (
                                <AllowedValueInfo>{
                                    "value": "connector_dropbox",
                                },
                                <AllowedValueInfo>{
                                    "value": "connector_gmail",
                                },
                                <AllowedValueInfo>{
                                    "value": "connector_googlecalendar",
                                },
                                <AllowedValueInfo>{
                                    "value": "connector_googledrive",
                                },
                                <AllowedValueInfo>{
                                    "value": "connector_microsoftteams",
                                },
                                <AllowedValueInfo>{
                                    "value": "connector_outlookcalendar",
                                },
                                <AllowedValueInfo>{
                                    "value": "connector_outlookemail",
                                },
                                <AllowedValueInfo>{
                                    "value": "connector_sharepoint",
                                },
                            ),
                            # allow for future connector ID values
                            "allowed_values_creatable": True,
                            "exclusive_with": ("server_url",),
                            "required_groups": ("server",),
                        },
                        "headers": {
                            "display_name": "Headers",
                            "type": AbstractDataProviderTypeMap."*hash",
                            "desc": "Optional HTTP headers to send to the MCP server. Use for authentication or "
                                "other purposes",
                        },
                        "require_approval": {
                            "display_name": "Require Approval?",
                            "type": AbstractDataProviderTypeMap."any",
                            "desc": "Specify which of the MCP server's tools require approval",
                            "allowed_values": (
                                <AllowedValueInfo>{
                                    "display_name": "Always",
                                    "value": "always",
                                    "desc": "All tool calls require approval",
                                },
                                <AllowedValueInfo>{
                                    "display_name": "Never",
                                    "value": "never",
                                    "desc": "No tool calls require approval",
                                },
                            ),
                            "default_value": "always",
                            "allowed_values_creatable": True,
                        },
                        "server_description": {
                            "display_name": "Server Description",
                            "type": AbstractDataProviderTypeMap."*string",
                            "desc": "Optional description of the MCP server, used to provide more context",
                        },
                        "server_url": {
                            "display_name": "Server URL",
                            "type": AbstractDataProviderTypeMap."*string",
                            "desc": "The URL of the MCP server. One of `server_url` or `connector_id` must be "
                                "provided",
                            "exclusive_with": ("connection_id",),
                            "required_groups": ("server",),
                        },
                    }.pairIterator()
                )),
                "exclusive_with": ("file_search", "function", "web_search", "computer_use"),
            },
            "code_interpreter": {
                "display_name": "Code Interpreter Tool",
                "short_desc": "A tool that runs Python code to help generate a response to a prompt",
                "desc": "A tool that runs Python code to help generate a response to a prompt",
                "type": new HashDataType(AutoHashOrNothingType, "CodeInterpreterTool", (
                    map {$1.key: new QoreDataField({"name": $1.key} + $1.value)}, {
                        "container": {
                            "display_name": "Server Label",
                            "type": AbstractDataProviderTypeMap."any",
                            "desc": "The code interpreter container. Can be a container ID or an object that "
                                "specifies uploaded file IDs to make available to your code",
                        },
                        "type": {
                            "display_name": "Type",
                            "type": AbstractDataProviderTypeMap."string",
                            "desc": "The type of the code interpreter tool",
                            "allowed_values": (
                                <AllowedValueInfo>{
                                    "display_name": "Code Interpreter",
                                    "value": "code_interpreter",
                                },
                            ),
                            "default_value": "code_interpreter",
                        },
                    }.pairIterator()
                )),
                "exclusive_with": ("file_search", "function", "web_search", "computer_use"),
            },
            "image_generation": {
                "display_name": "Image Generation Tool",
                "short_desc": "A tool that generates images using a model like 'gpt-image-1'",
                "desc": "A tool that generates images using a model like `gpt-image-1`",
                "type": new HashDataType(AutoHashOrNothingType, "ImageGenerationTool", (
                    map {$1.key: new QoreDataField({"name": $1.key} + $1.value)}, {
                        "type": {
                            "display_name": "Type",
                            "type": AbstractDataProviderTypeMap."string",
                            "desc": "The type of the image generation tool",
                            "allowed_values": (
                                <AllowedValueInfo>{
                                    "display_name": "Image Generation",
                                    "value": "image_generation",
                                },
                            ),
                            "default_value": "image_generation",
                        },
                        "background": {
                            "display_name": "Background",
                            "type": AbstractDataProviderTypeMap."string",
                            "desc": "Background type for the generated image",
                            "default_value": "auto",
                            "allowed_values": (
                                <AllowedValueInfo>{
                                    "display_name": "Auto",
                                    "value": "auto",
                                },
                                <AllowedValueInfo>{
                                    "display_name": "Opaque",
                                    "value": "opaque",
                                },
                                <AllowedValueInfo>{
                                    "display_name": "Transparent",
                                    "value": "transparent",
                                },
                            ),
                        },
                        "input_fidelity": {
                            "display_name": "Input Fidelity",
                            "type": AbstractDataProviderTypeMap."string",
                            "desc": "Control how much effort the model will exert to match the style and features, "
                                "especially facial features, of input images. This parameter is only supported for "
                                "`gpt-image-1`",
                            "default_value": "low",
                            "allowed_values": (
                                <AllowedValueInfo>{
                                    "display_name": "High",
                                    "value": "high",
                                },
                                <AllowedValueInfo>{
                                    "display_name": "Low",
                                    "value": "low",
                                },
                            ),
                        },
                        "input_image_mask": {
                            "display_name": "Input Image Mask",
                            "type": new HashDataType(AutoHashOrNothingType, "ImageGenerationTool", (
                                map {$1.key: new QoreDataField({"name": $1.key} + $1.value)}, {
                                    "file_id": {
                                        "display_name": "File ID",
                                        "type": AbstractDataProviderTypeMap."*string",
                                        "desc": "File ID for the mask image",
                                    },
                                    "image_url": {
                                        "display_name": "Image URL",
                                        "type": AbstractDataProviderTypeMap."*string",
                                        "desc": "Base64-encoded mask image",
                                    },
                                }.pairIterator()
                            )),
                            "desc": "Optional mask for inpainting",
                        },
                        "model": {
                            "display_name": "Model",
                            "type": AbstractDataProviderTypeMap."string",
                            "desc": "The image generation model to use",
                            "default_value": "gpt-image-1",
                            "allowed_values": (
                                <AllowedValueInfo>{
                                    "display_name": "gpt-image-1",
                                    "value": "gpt-image-1",
                                },
                            ),
                            "allowed_values_creatable": True,
                        },
                        "moderation": {
                            "display_name": "Moderation",
                            "type": AbstractDataProviderTypeMap."string",
                            "desc": "Moderation level for the generated image",
                            "default_value": "auto",
                        },
                        "output_compression": {
                            "display_name": "Output Compression",
                            "type": AbstractDataProviderTypeMap."int",
                            "desc": "Compression level for the generated image",
                            "default_value": 100,
                        },
                        "output_format": {
                            "display_name": "Output Format",
                            "type": AbstractDataProviderTypeMap."string",
                            "desc": "The format of the generated image",
                            "default_value": "png",
                            "allowed_values": (
                                <AllowedValueInfo>{
                                    "display_name": "PNG",
                                    "value": "png",
                                },
                                <AllowedValueInfo>{
                                    "display_name": "JPG",
                                    "value": "jpg",
                                },
                                <AllowedValueInfo>{
                                    "display_name": "WEBP",
                                    "value": "webp",
                                },
                            ),
                        },
                        "partial_images": {
                            "display_name": "Partial Images",
                            "type": AbstractDataProviderTypeMap."int",
                            "desc": "Number of partial images to generate in streaming mode, from `0` (default) to "
                                "`3`",
                            "default_value": 0,
                        },
                        "quality": {
                            "display_name": "Quality",
                            "type": AbstractDataProviderTypeMap."string",
                            "desc": "Quality level for the generated image",
                            "default_value": "auto",
                            "allowed_values": (
                                <AllowedValueInfo>{
                                    "display_name": "Auto",
                                    "value": "auto",
                                },
                                <AllowedValueInfo>{
                                    "display_name": "Low",
                                    "value": "low",
                                },
                                <AllowedValueInfo>{
                                    "display_name": "Medium",
                                    "value": "medium",
                                },
                                <AllowedValueInfo>{
                                    "display_name": "High",
                                    "value": "high",
                                },
                            ),
                        },
                        "size": {
                            "display_name": "Image Size",
                            "short_desc": "The size of the generated image(s)",
                            "desc": "The size of the generated image(s)",
                            "type": AbstractDataProviderTypeMap."string",
                            "allowed_values": (
                                <AllowedValueInfo>{
                                    "display_name": "Auto",
                                    "value": "auto",
                                    "desc": "The default size for the model (`gpt-image-1` only)",
                                },
                                <AllowedValueInfo>{
                                    "display_name": "1024x1024",
                                    "value": "1024x1024",
                                    "desc": "1024 x 1024 pixels",
                                },
                                <AllowedValueInfo>{
                                    "display_name": "1024x1536",
                                    "value": "1024x1536",
                                    "desc": "1024 x 1536 pixels",
                                },
                                <AllowedValueInfo>{
                                    "display_name": "1536x1024",
                                    "value": "1536x1024",
                                    "desc": "1536 x 1024 pixels",
                                },
                            ),
                            "default_value": "auto",
                        },
                    }.pairIterator()
                )),
                "exclusive_with": ("file_search", "function", "web_search", "computer_use"),
            },
            "local_shell": {
                "display_name": "Local Shell Tool",
                "short_desc": "A tool that allows the model to execute shell commands in a local environment",
                "desc": "A tool that allows the model to execute shell commands in a local environment",
                "type": new HashDataType(AutoHashOrNothingType, "LocalShellTool", (
                    map {$1.key: new QoreDataField({"name": $1.key} + $1.value)}, {
                        "type": {
                            "display_name": "Type",
                            "type": AbstractDataProviderTypeMap."string",
                            "desc": "The type of the local shell tool",
                            "allowed_values": (
                                <AllowedValueInfo>{
                                    "display_name": "Local Shell",
                                    "value": "local_shell",
                                },
                            ),
                            "default_value": "local_shell",
                        },
                    }.pairIterator()
                )),
                "exclusive_with": ("file_search", "function", "web_search", "computer_use"),
            },
            "custom_tool": {
                "display_name": "Custom Tool",
                "short_desc": "A custom tool defined by the user",
                "desc": "A custom tool defined by the user",
                "type": new HashDataType(AutoHashOrNothingType, "CustomTool", (
                    map {$1.key: new QoreDataField({"name": $1.key} + $1.value)}, {
                        "name": {
                            "display_name": "Tool Name",
                            "type": AbstractDataProviderTypeMap."string",
                            "desc": "The name of the custom tool",
                        },
                        "description": {
                            "display_name": "Tool Description",
                            "type": AbstractDataProviderTypeMap."*string",
                            "desc": "A description of the custom tool used by the model to determine whether or not "
                                "to call it",
                        },
                        "type": {
                            "display_name": "Type",
                            "type": AbstractDataProviderTypeMap."string",
                            "desc": "The type of the custom tool",
                            "allowed_values": (
                                <AllowedValueInfo>{
                                    "display_name": "Custom",
                                    "value": "custom",
                                },
                            ),
                            "default_value": "custom",
                        },
                        "format": {
                            "display_name": "Tool Parameters",
                            "type": AbstractDataProviderTypeMap."*hash",
                            "desc": "The input format for the custom tool. Default is unconstrained text",
                        },
                    }.pairIterator()
                )),
                "exclusive_with": ("file_search", "function", "web_search", "computer_use"),
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelToolDataType = new OpenAiModelToolDataType();

public class OpenAiModelCreateResponseRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "input": {
                "display_name": "Input",
                "short_desc": "Text, image, or file inputs to the model, used to generate a response",
                "desc": "Text, image, or file inputs to the model, used to generate a response",
                "type": new SoftListDataType("ModelInputList", OpenAiModelInputDataType),
            },
            "instructions": {
                "display_name": "Instructions",
                "short_desc": "Inserts a system (or developer) message as the first item in the model's context",
                "desc": "Inserts a system (or developer) message as the first item in the model's context\n"
                    "When using along with `previous_response_id`, the instructions from a previous response will "
                    "not be carried over to the next response. This makes it simple to swap out system (or "
                    "developer) messages in new responses",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "max_output_tokens": {
                "display_name": "Max Output Tokens",
                "short_desc": "The maximum number of tokens to generate in the completion",
                "desc": "An upper bound for the number of tokens that can be generated for a response, including "
                    "visible output tokens and reasoning tokens",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "metadata": OpenAiRunDataType::Fields.metadata,
            "parallel_tool_calls": {
                "display_name": "Parallel Tool Calls?",
                "short_desc": "Whether to allow the model to run tool calls in parallel",
                "desc": "Whether to allow the model to run tool calls in parallel",
                "type": AbstractDataProviderTypeMap."bool",
                "default_value": True,
            },
            "previous_response_id": {
                "display_name": "Previous Response ID",
                "short_desc": "The unique ID of the previous response to the model",
                "desc": "The unique ID of the previous response to the model. Use this to create multi-turn "
                    "conversations",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "prompt": {
                "display_name": "Prompt",
                "short_desc": "Configuration options for the prompt sent to the model",
                "desc": "Configuration options for the prompt sent to the model",
                "type": new HashDataType(AutoHashOrNothingType, "Prompt", (
                    map {$1.key: new QoreDataField({"name": $1.key} + $1.value)}, {
                        "id": {
                            "display_name": "Prompt ID",
                            "type": AbstractDataProviderTypeMap."string",
                            "desc": "The unique identifier of the prompt template to use",
                        },
                        "variables": {
                            "display_name": "Variables",
                            "type": AbstractDataProviderTypeMap."*hash",
                            "desc": "Optional map of values to substitute in for variables in your prompt. The "
                                "substitution values can either be strings, or other Response input types like "
                                "images or files",
                        },
                        "version": {
                            "display_name": "Version",
                            "type": AbstractDataProviderTypeMap."*string",
                            "desc": "Optional version of the prompt template",
                        },
                    }.pairIterator()
                )),
            },
            "prompt_cache_key": {
                "display_name": "Prompt Cache Key",
                "short_desc": "A key that can be used to cache prompts for the model",
                "desc": "Used by OpenAI to cache responses for similar requests to optimize your cache hit rates",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "reasoning": {
                "display_name": "Reasoning",
                "short_desc": "Configuration options for o-series reasoning models",
                "desc": "Configuration options for o-series reasoning models",
                "type": ReasoningOrNothingDataType,
            },
            "safety_identifier": {
                "display_name": "Safety Identifier",
                "short_desc": "An optional unique identifier that enables additional safety checks",
                "desc": "A stable identifier used to help detect users of your application that may be violating "
                    "OpenAI's usage policies. The IDs should be a string that uniquely identifies each user. OpenAI "
                    "recommends hashing their username or email address, in order to avoid sending any identifying "
                    "information",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "service_tier": {
                "display_name": "Service Tier",
                "short_desc": "Specifies the latency tier to use for processing the request",
                "desc": "Specifies the latency tier to use for processing the request",
                "type": AbstractDataProviderTypeMap."string",
                "default_value": "auto",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Auto",
                        "desc": "If the Project is Scale tier enabled, the system will utilize scale tier credits "
                            "until they are exhausted; if not, the request will be processed using the default "
                            "service tier with a lower uptime SLA and no latency guarentee",
                        "value": "auto",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Default",
                        "desc": "The request will be processed using the default service tier with a lower uptime "
                            "SLA and no latency guarentee",
                        "value": "default",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Flex",
                        "desc": "The request will be processed using the flex service tier",
                        "value": "flex",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Priority",
                        "desc": "The request will be processed using the priority service tier",
                        "value": "priority",
                    },
                ),
            },
            "store": {
                "display_name": "Store?",
                "short_desc": "Whether to store the generated model response for later retrieval via API",
                "desc": "WhetWhether to store the generated model response for later retrieval via APII",
                "type": AbstractDataProviderTypeMap."bool",
                "default_value": False,
            },
            "stream": OpenAiChatCompletionRequestType::Fields.stream,
            "stream_options": {
                "display_name": "Stream Options",
                "short_desc": "Options for controlling the stream output",
                "desc": "Options for controlling the stream output",
                "type": new HashDataType(AutoHashOrNothingType, "StreamOptions", (
                    map {$1.key: new QoreDataField({"name": $1.key} + $1.value)}, {
                        "include_obfuscation": {
                            "display_name": "Include Obfuscation?",
                            "type": AbstractDataProviderTypeMap."bool",
                            "desc": "When true, stream obfuscation will be enabled. Stream obfuscation adds random "
                                "characters to an `obfuscation` field on streaming delta events to normalize payload "
                                "sizes as a mitigation to certain side-channel attacks. These obfuscation fields are "
                                "included by default, but add a small amount of overhead to the data stream. You can "
                                "set `include_obfuscation` to `false` to optimize for bandwidth if you trust the "
                                "network links between your application and the OpenAI API",
                        },
                    }.pairIterator()
                )),
            },
            "temperature": OpenAiChatCompletionRequestType::Fields.temperature,
            "text": {
                "display_name": "Response Text Format",
                "short_desc": "Configuration options for a text response from the model",
                "desc": "Configuration options for a text response from the model",
                "type": OpenAiModelResponseTextOrNothingDataType,
            },
            "tool_choice": OpenAiChatCompletionRequestType::Fields.tool_choice,
            "tools": {
                "display_name": "Tools",
                "short_desc": "The tools that the model can use to generate a response",
                "desc": "The tools that the model can use to generate a response",
                "type": new SoftListDataType("*ModelToolList", OpenAiModelToolDataType, True),
            },
            "top_logprobs": {
                "display_name": "Top Log Probabilities",
                "short_desc": "The number of most likely tokens to return at each token position",
                "desc": "An integer between `0` and `20` specifying the number of most likely tokens to return at "
                    "each token position, each with an associated log probability",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "top_p": OpenAiChatCompletionRequestType::Fields.top_p,
            "truncation": {
                "display_name": "Truncation",
                "short_desc": "The truncation strategy to use for the model response",
                "desc": "The truncation strategy to use for the model response",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Auto",
                        "desc": " the context of this response and previous ones exceeds the model's context window "
                            "size, the model will truncate the response to fit the context window by dropping input "
                            "items in the middle of the conversation",
                        "value": "auto",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Disabled",
                        "desc": "If a model response will exceed the context window size for a model, the request "
                            "will fail with a 400 error",
                        "value": "disabled",
                    },
                ),
                "default_value": "disabled",
            },
            "user": {
                "display_name": "User",
                "short_desc": "A unique identifier representing your end-user, which can help OpenAI to monitor and "
                    "detect abuse",
                "desc": "A unique identifier representing your end-user, which can help OpenAI to monitor and detect "
                    "abuse",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelCreateResponseRequestDataType = new OpenAiModelCreateResponseRequestDataType();

public class OpenAiModelCreateErrorDataType inherits HashDataType {
    public {
        const Fields = {
            "code": {
                "display_name": "Error Code",
                "short_desc": "The error code for the response",
                "desc": "The error code for the response",
                "type": AbstractDataProviderTypeMap."string",
            },
            "message": {
                "display_name": "Error Message",
                "short_desc": "A human-readable description of the error",
                "desc": "A human-readable description of the error",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelCreateErrorDataType = new OpenAiModelCreateErrorDataType();
public const OpenAiModelCreateErrorOrNothingDataType = OpenAiModelCreateErrorDataType.getOrNothingType();

public class OpenAiModelCreateIncompleteDetailsDataType inherits HashDataType {
    public {
        const Fields = {
            "reason": {
                "display_name": "Reason",
                "short_desc": "The reason the response is incomplete",
                "desc": "The reason the response is incomplete",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelCreateIncompleteDetailsDataType = new OpenAiModelCreateIncompleteDetailsDataType();
public const OpenAiModelCreateIncompleteDetailsOrNothingDataType =
    OpenAiModelCreateIncompleteDetailsDataType.getOrNothingType();

public class OpenAiModelUsageInputTokensDetailsDataType inherits HashDataType {
    public {
        const Fields = {
            "cached_tokens": {
                "display_name": "Cached Tokens",
                "short_desc": "The number of tokens that were retrieved from the cache",
                "desc": "The number of tokens that were retrieved from the cache",
                "type": AbstractDataProviderTypeMap."int",
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelUsageInputTokensDetailsDataType = new OpenAiModelUsageInputTokensDetailsDataType();

public class OpenAiModelUsageOutputTokensDetailsDataType inherits HashDataType {
    public {
        const Fields = {
            "reasoning_tokens": {
                "display_name": "Reasoning Tokens",
                "short_desc": "The number of reasoning tokens",
                "desc": "The number of reasoning tokens",
                "type": AbstractDataProviderTypeMap."int",
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelUsageOutputTokensDetailsDataType = new OpenAiModelUsageOutputTokensDetailsDataType();

public class OpenAiModelUsageDataType inherits HashDataType {
    public {
        const Fields = {
            "input_tokens": {
                "display_name": "Input Tokens",
                "short_desc": "The number of tokens in the input",
                "desc": "The number of tokens in the input",
                "type": AbstractDataProviderTypeMap."int",
            },
            "input_tokens_details": {
                "display_name": "Input Tokens Details",
                "short_desc": "Details about the input tokens used in the request",
                "desc": "Details about the input tokens used in the request",
                "type": OpenAiModelUsageInputTokensDetailsDataType,
            },
            "output_tokens": {
                "display_name": "Output Tokens",
                "short_desc": "The number of tokens in the output",
                "desc": "The number of tokens in the output",
                "type": AbstractDataProviderTypeMap."int",
            },
            "output_tokens_details": {
                "display_name": "Output Tokens Details",
                "short_desc": "Details about the output tokens used in the request",
                "desc": "Details about the output tokens used in the request",
                "type": OpenAiModelUsageOutputTokensDetailsDataType,
            },
            "total_tokens": {
                "display_name": "Total Tokens",
                "short_desc": "The total number of tokens used in the request and response",
                "desc": "The total number of tokens used in the request and response",
                "type": AbstractDataProviderTypeMap."int",
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelUsageDataType = new OpenAiModelUsageDataType();

public class OpenAiModelCreateResponseResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "created_at": {
                "display_name": "Created At",
                "short_desc": "The date and time the model response was created",
                "desc": "The date and time the model response was created",
                "type": AbstractDataProviderTypeMap.epoch,
            },
            "error": {
                "display_name": "Error",
                "short_desc": "An error object returned when the model fails to generate a Response",
                "desc": "An error object returned when the model fails to generate a Response",
                "type": OpenAiModelCreateErrorOrNothingDataType,
            },
            "id": {
                "display_name": "Response ID",
                "short_desc": "The unique ID of the model response",
                "desc": "The unique ID of the model response",
                "type": AbstractDataProviderTypeMap."string",
            },
            "incomplete_details": {
                "display_name": "Incomplete Details",
                "short_desc": "Details about why the response is incomplete",
                "desc": "Details about why the response is incomplete",
                "type": OpenAiModelCreateIncompleteDetailsOrNothingDataType,
            },
            "instructions": {
                "display_name": "Instructions",
                "short_desc": "The instructions used to generate the model response",
                "desc": "The instructions used to generate the model response",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "max_output_tokens": {
                "display_name": "Max Output Tokens",
                "short_desc": "The maximum number of tokens that can be generated for the response",
                "desc": "An upper bound for the number of tokens that can be generated for a response, including "
                    "visible output tokens and reasoning tokens",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "metadata": OpenAiRunDataType::Fields.metadata,
            "model": {
                "display_name": "Model",
                "short_desc": "The model used to generate the response",
                "desc": "The model used to generate the response",
                "type": AbstractDataProviderTypeMap."string",
            },
            "object": {
                "display_name": "Object Type",
                "short_desc": "The type of object returned by the model",
                "desc": "The type of object returned by the model",
                "type": AbstractDataProviderTypeMap."string",
                "default_value": "response",
            },
            "output": {
                "display_name": "Output",
                "short_desc": "An array of content items generated by the model",
                "desc": "An array of content items generated by the model",
                "type": new ListDataType("*ModelOutputList", AbstractDataProviderTypeMap."hash", True),
            },
            "output_text": {
                "display_name": "Output Text",
                "short_desc": "The text generated by the model",
                "desc": "SDK-only convenience property that contains the aggregated text output from all "
                    "`output_text` items in the output array, if any",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "parallel_tool_calls": {
                "display_name": "Parallel Tool Calls?",
                "short_desc": "Whether the model was allowed to run tool calls in parallel",
                "desc": "Whether the model was allowed to run tool calls in parallel",
                "type": AbstractDataProviderTypeMap."bool",
            },
            "previous_response_id": {
                "display_name": "Previous Response ID",
                "short_desc": "The unique ID of the previous response to the model",
                "desc": "The unique ID of the previous response to the model. Use this to create multi-turn "
                    "conversations",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "reasoning": OpenAiModelCreateResponseRequestDataType::Fields.reasoning,
            "service_tier": OpenAiModelCreateResponseRequestDataType::Fields.service_tier,
            "status": {
                "display_name": "Status",
                "short_desc": "The status of the model response",
                "desc": "The status of the model response",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Completed",
                        "value": "completed",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Incomplete",
                        "value": "incomplete",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Failed",
                        "value": "failed",
                    },
                    <AllowedValueInfo>{
                        "display_name": "In Progress",
                        "value": "in_progress",
                    },
                ),
            },
            "temperature": OpenAiModelCreateResponseRequestDataType::Fields.temperature,
            "text": OpenAiModelCreateResponseRequestDataType::Fields.text,
            "tool_choice": OpenAiModelCreateResponseRequestDataType::Fields.tool_choice,
            "tools": OpenAiModelCreateResponseRequestDataType::Fields.tools,
            "top_p": OpenAiModelCreateResponseRequestDataType::Fields.top_p,
            "truncation": OpenAiModelCreateResponseRequestDataType::Fields.truncation,
            "usage": {
                "display_name": "Usage",
                "short_desc": "The usage information for the model response",
                "desc": "Represents token usage details including input tokens, output tokens, a breakdown of output "
                    "tokens, and the total tokens used.",
                "type": OpenAiModelUsageDataType,
            },
            "user": OpenAiModelCreateResponseRequestDataType::Fields.user,
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
    }
}

public const OpenAiModelCreateResponseResponseDataType = new OpenAiModelCreateResponseResponseDataType();
}
