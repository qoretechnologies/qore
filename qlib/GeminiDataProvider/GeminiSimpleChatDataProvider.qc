# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GeminiDataProvider module definition

/** GeminiSimpleChatDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GeminiDataProvider module
public namespace GeminiDataProvider {
#! The Gemini simple chat data provider (single message in, single reply out)
public class GeminiSimpleChatDataProvider inherits GeminiDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "simple-chat",
            "type": "GeminiSimpleChatDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new GeminiSimpleChatRequestDataType();

        #! Response type
        const ResponseType = new GeminiSimpleChatResponseDataType();
    }

    #! Creates the object from the REST client
    constructor(RestClientIo::RestClientIo rest) : GeminiDataProviderBase(rest) {
    }

    #! Creates the object from constructor options
    constructor(hash<auto> options) : GeminiDataProviderBase(options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string model = remove req.model;
        string role = remove req.role ?? "user";
        string content = remove req.content;
        *string system_instruction = remove req.systemInstruction;

        # Build the generateContent request body
        hash<auto!> body = {
            "contents": ({
                "role": role,
                "parts": ({"text": content},),
            },),
        };

        # Add generation config if any generation params are set
        hash<auto> gen_config;
        if (exists req.temperature) gen_config.temperature = remove req.temperature;
        if (exists req.topP) gen_config.topP = remove req.topP;
        if (exists req.topK) gen_config.topK = remove req.topK;
        if (exists req.maxOutputTokens) gen_config.maxOutputTokens = remove req.maxOutputTokens;
        if (gen_config) {
            body.generationConfig = gen_config;
        }

        # Add system instruction if provided
        if (system_instruction) {
            body.systemInstruction = {
                "parts": ({"text": system_instruction},),
            };
        }

        string path = buildModelPath(model, "generateContent");
        hash<auto> resp = doRestCommand("POST", path, body).body;

        # Flatten the response to simple fields
        hash<auto> rv = {};
        if (resp.candidates && resp.candidates[0]) {
            hash<auto> candidate = resp.candidates[0];
            if (candidate.content && candidate.content.parts && candidate.content.parts[0]) {
                rv.text = candidate.content.parts[0].text;
            }
            rv.finishReason = candidate.finishReason;
        }
        if (resp.usageMetadata) {
            rv += resp.usageMetadata;
        }
        return rv;
    }

    #! Returns the description of a successful request message, if any
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
