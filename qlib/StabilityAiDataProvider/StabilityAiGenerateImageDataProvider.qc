# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore StabilityAiGenerateImageDataProvider class definition

/** StabilityAiGenerateImageDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace StabilityAiDataProvider {
public class StabilityAiGenerateImageDataProvider inherits StabilityAiDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "generate-image",
            "type": "StabilityAiGenerateImageDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        static StabilityAiGenerateImageRequestDataType RequestType =
            new StabilityAiGenerateImageRequestDataType();
        static StabilityAiGenerateImageResponseDataType ResponseType =
            new StabilityAiGenerateImageResponseDataType();
    }

    constructor(StabilityAiRestClient::StabilityAiRestClientIo rest)
            : StabilityAiDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : StabilityAiDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string model = req.model ?? StabilityAiRestClient::DefaultModel;
        *string path = StabilityAiRestClient::ModelPathMap{model};
        if (!path) {
            throw "STABILITY-AI-CONFIG-ERROR",
                sprintf("model %y is unknown; known models: %y",
                    model, keys StabilityAiRestClient::ModelPathMap);
        }
        hash<auto!> body;
        body.prompt = req.prompt;
        body.output_format = req.output_format ?? "png";
        if (req.aspect_ratio.val()) {
            body.aspect_ratio = req.aspect_ratio;
        }
        if (req.negative_prompt.val()) {
            body.negative_prompt = req.negative_prompt;
        }
        if (exists req.seed) {
            body.seed = req.seed;
        }
        if (req.style_preset.val()) {
            body.style_preset = req.style_preset;
        }

        string accept = sprintf("image/%s", body.output_format);
        hash<auto> resp = rest.restPost(path, body, {"Accept": accept});
        return {
            "image":         resp.body,
            "content_type":  accept,
            "finish_reason": resp.headers."finish-reason",
            "seed":          resp.headers."seed",
        };
    }
}
}
