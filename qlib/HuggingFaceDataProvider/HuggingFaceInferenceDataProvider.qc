# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore HuggingFaceDataProvider module definition

/** HuggingFaceInferenceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the HuggingFaceDataProvider module
public namespace HuggingFaceDataProvider {
#! Container provider for Hugging Face Inference API
/** Creates a separate RestClientIo for the inference endpoint at
    \c https://router.huggingface.co/v1
*/
public class HuggingFaceInferenceDataProvider inherits HuggingFaceDataProviderCommon {
    public {
        #! Hugging Face Inference API base URL
        const InferenceUrl = "https://router.huggingface.co";

        #! Inference API path
        const InferenceApiPath = "/v1";

        const ProviderInfo = <DataProviderInfo>{
            "name": "inference",
            "type": "HuggingFaceInferenceDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! Separate RestClientIo for the inference endpoint
        *RestClientIo::RestClientIo inference_rest;

        const ChildMap = {
            "chat-completions": Class::forName(
                "HuggingFaceDataProvider::HuggingFaceChatCompletionsDataProvider"),
        };
    }

    #! Creates the object from a RestClientIo client (Hub API client)
    constructor(RestClientIo::RestClientIo rest) : HuggingFaceDataProviderCommon(rest) {
        createInferenceClient();
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : HuggingFaceDataProviderCommon(options) {
        createInferenceClient();
    }

    string getName() { return ProviderInfo.name; }

    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) { return; }
        # pass inference_rest to children, not the Hub rest client
        return cls.newObject(inference_rest);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Creates the inference REST client from the Hub client's auth headers
    private createInferenceClient() {
        if (!rest) {
            return;
        }
        # propagate Authorization header from the Hub client
        hash<auto> opts = {
            "url": InferenceUrl,
            "default_path": InferenceApiPath,
            "data": "json",
            "headers": rest.getDefaultHeaders(),
        };
        inference_rest = new RestClientIo::RestClientIo(opts);
    }
}
}
