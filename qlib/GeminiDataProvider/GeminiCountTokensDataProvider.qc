# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GeminiDataProvider module definition

/** GeminiCountTokensDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The Gemini count tokens data provider
public class GeminiCountTokensDataProvider inherits GeminiDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "count-tokens",
            "type": "GeminiCountTokensDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new GeminiCountTokensRequestDataType();

        #! Response type
        const ResponseType = new GeminiCountTokensResponseDataType();
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
        # Handle prompt shortcut
        if (req.prompt) {
            string prompt = remove req.prompt;
            req.contents = ({
                "role": "user",
                "parts": ({"text": prompt},),
            },);
        } else if (!req.contents) {
            throw "COUNT-TOKENS-ERROR", "either 'prompt' or 'contents' must be provided";
        }
        string path = buildModelPath(model, "countTokens");
        return doRestCommand("POST", path, req).body;
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
