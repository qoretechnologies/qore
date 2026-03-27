# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAiDataProvider module definition

/** OpenAiRunSubmitToolOutputsDataProvider.qc Copyright 2024 Qore Technologies, s.r.o.

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
public class OpenAiRunSubmitToolOutputsDataProvider inherits OpenAiDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "submittooloutputs",
            "type": "OpenAiRunSubmitToolOutputsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = OpenAiRunSubmitToolOutputsRequestType;

        #! Response type
        const ResponseType = OpenAiRunDataType;
    }

    private {
        #! The thread ID
        string thread_id;

        #! The run info
        hash<auto> run;
    }

    #! Creates the object from the arguments
    constructor(*RestClientIo::RestClientIo rest, string thread_id, hash<auto> run) : OpenAiDataProviderCommon(rest) {
        self.thread_id = thread_id;
        self.run = run;
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
        hash<auto> rv = doRestCommand("PUT", "threads/" + thread_id + "/runs/" + run.id + "/submit_tool_outputs",
            req, AssistantApiHdr).body;
        rv.created_at = localtime(rv.created_at);
        map run{$1.key} = localtime($1.value), run{OpenAiRunCreateDataProvider::OptDateFields}.pairIterator(),
            $1.value;
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

#! Tool output data type
public class OpenAiToolOutputDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "tool_call_id": {
                "display_name": "Tool Call ID",
                "short_desc": "The ID of the tool call in the 'required_action' object within the run object the "
                    "output is being submitted for",
                "desc": "The ID of the tool call in the `required_action` object within the run object the output is "
                    "being submitted for",
                "type": AbstractDataProviderTypeMap."string",
            },
            "output": {
                "display_name": "Output",
                "short_desc": "The output of the tool call to be submitted to continue the run",
                "desc": "The output of the tool call to be submitted to continue the run",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiToolOutput") {
        addQoreFields(Fields);
    }
}

#! Tool output type constant
public const OpenAiToolOutputDataType = new OpenAiToolOutputDataType();

#! The run tool data type
public class OpenAiRunSubmitToolOutputsRequestType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "tool_outputs": {
                "display_name": "Tool Outputs",
                "short_desc": "A list of tools for which the outputs are being submitted",
                "desc": "A list of tools for which the outputs are being submitted",
                "type": new ListDataType("OpenAiToolOutputs", OpenAiToolOutputDataType),
            },
            "stream": OpenAiChatCompletionRequestType::Fields.stream,
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiRunSubmitToolOutputsRequest") {
        addQoreFields(Fields);
    }
}

#! Constant for the run tool data type
public const OpenAiRunSubmitToolOutputsRequestType = new OpenAiRunSubmitToolOutputsRequestType();
}
