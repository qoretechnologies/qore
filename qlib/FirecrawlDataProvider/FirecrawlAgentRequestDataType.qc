# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlAgentRequestDataType.qc request type for the Firecrawl agent action

/*  FirecrawlAgentRequestDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for the Firecrawl agent action
public class FirecrawlAgentRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "prompt": {
                "display_name": "Prompt",
                "short_desc": "The agent prompt",
                "desc": "Natural language prompt describing what data to find and extract "
                    "(maximum 10000 characters)",
                "type": StringType,
                "required": True,
                "example_value": "Find the pricing information for Firecrawl",
            },
            "urls": {
                "display_name": "URLs",
                "short_desc": "URLs to search",
                "desc": "Optional list of URLs to constrain the agent's search scope",
                "type": SoftListOrNothingType,
                "attr": {"preselected": True},
                "example_value": ("https://example.com",),
            },
            "schema": {
                "display_name": "Schema",
                "short_desc": "JSON Schema for output",
                "desc": "A JSON Schema object defining the structure of the output data. "
                    "Used to guide the agent and validate the output format.\n\n"
                    "**Example**: `{\"type\": \"object\", \"properties\": "
                    "{\"name\": {\"type\": \"string\"}, \"price\": {\"type\": \"number\"}}}`",
                "type": AutoHashOrNothingType,
            },
            "maxCredits": {
                "display_name": "Max Credits",
                "short_desc": "Maximum credits to use",
                "desc": "Maximum number of API credits to use for this agent task. "
                    "Defaults to `2500`",
                "type": SoftIntOrNothingType,
            },
            "strictConstrainToURLs": {
                "display_name": "Strict Constrain to URLs",
                "short_desc": "Strictly limit to provided URLs",
                "desc": "If `True`, the agent is strictly limited to searching only the "
                    "provided URLs and does not follow links to other pages",
                "type": SoftBoolOrNothingType,
            },
            "model": {
                "display_name": "Model",
                "short_desc": "Agent model to use",
                "desc": "The agent model to use for the task",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "spark-1-mini", "display_name": "Spark 1 Mini"},
                    <AllowedValueInfo>{"value": "spark-1-pro", "display_name": "Spark 1 Pro"},
                ),
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FirecrawlDataProvider
