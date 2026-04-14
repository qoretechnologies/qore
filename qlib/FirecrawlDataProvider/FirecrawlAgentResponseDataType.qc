# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlAgentResponseDataType.qc response type for the Firecrawl agent action

/*  FirecrawlAgentResponseDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FirecrawlDataProvider module
public namespace FirecrawlDataProvider {

#! Response type for the Firecrawl agent action (also used for get-agent-status)
public class FirecrawlAgentResponseDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "success": {
                "display_name": "Success",
                "short_desc": "Whether the request succeeded",
                "desc": "Whether the agent request completed successfully",
                "type": SoftBoolType,
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Agent job status",
                "desc": "The current status of the agent job (e.g., `processing`, "
                    "`completed`, `failed`)",
                "type": StringOrNothingType,
            },
            "data": {
                "display_name": "Data",
                "short_desc": "Agent output data",
                "desc": "The data returned by the agent, structured according to the "
                    "provided schema or prompt",
                "type": AutoHashOrNothingType,
            },
            "totalCreditsUsed": {
                "display_name": "Total Credits Used",
                "short_desc": "Total API credits consumed",
                "desc": "Total number of API credits consumed by this agent task",
                "type": SoftIntOrNothingType,
            },
            "sources": {
                "display_name": "Sources",
                "short_desc": "Source URLs used",
                "desc": "List of source URLs that the agent visited during the task",
                "type": SoftListOrNothingType,
            },
            "warning": {
                "display_name": "Warning",
                "short_desc": "Warning message",
                "desc": "A warning message, if any, from the agent operation",
                "type": StringOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FirecrawlDataProvider
