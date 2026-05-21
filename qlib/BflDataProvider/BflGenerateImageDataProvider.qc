# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BflGenerateImageDataProvider class definition

/** BflGenerateImageDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace BflDataProvider {
public class BflGenerateImageRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "prompt": {
                "display_name": "Prompt",
                "short_desc": "Text prompt describing the image",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Text prompt describing the desired image.",
                "required": True,
                "example_value": "a futuristic city skyline at dusk",
            },
            "model": {
                "display_name": "Model",
                "short_desc": "FLUX model identifier",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "FLUX model identifier — `flux-pro-1.1`, "
                    "`flux-pro-1.1-ultra` (highest quality), `flux-pro`, "
                    "`flux-dev`, `flux-kontext-pro` / `flux-kontext-max` "
                    "(image-to-image edit).",
                "default_value": "flux-pro-1.1",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "flux-pro-1.1",       "display_name": "FLUX Pro 1.1"},
                    <AllowedValueInfo>{"value": "flux-pro-1.1-ultra", "display_name": "FLUX Pro 1.1 Ultra"},
                    <AllowedValueInfo>{"value": "flux-pro",           "display_name": "FLUX Pro"},
                    <AllowedValueInfo>{"value": "flux-dev",           "display_name": "FLUX Dev"},
                    <AllowedValueInfo>{"value": "flux-kontext-pro",   "display_name": "FLUX Kontext Pro (image edit)"},
                    <AllowedValueInfo>{"value": "flux-kontext-max",   "display_name": "FLUX Kontext Max (image edit)"},
                ),
            },
            "aspect_ratio": {
                "display_name": "Aspect Ratio",
                "short_desc": "Output aspect ratio",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Output aspect ratio.",
                "example_value": "16:9",
            },
            "width": {
                "display_name": "Width",
                "short_desc": "Output image width in pixels",
                "type": AbstractDataProviderTypeMap."*softint",
                "desc": "Output image width in pixels (model-specific limits).",
            },
            "height": {
                "display_name": "Height",
                "short_desc": "Output image height in pixels",
                "type": AbstractDataProviderTypeMap."*softint",
                "desc": "Output image height in pixels (model-specific limits).",
            },
            "seed": {
                "display_name": "Seed",
                "short_desc": "Random seed for reproducibility",
                "type": AbstractDataProviderTypeMap."*softint",
                "desc": "Random seed for reproducibility — same seed + prompt "
                    "should produce the same image.",
            },
            "negative_prompt": {
                "display_name": "Negative Prompt",
                "short_desc": "What to avoid in the image",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Text describing what to avoid in the image.",
            },
            "prompt_upsampling": {
                "display_name": "Prompt Upsampling",
                "short_desc": "Let BFL rewrite the prompt for better quality",
                "type": AbstractDataProviderTypeMap."*softbool",
                "desc": "When `True`, BFL rewrites the prompt server-side for "
                    "improved quality.  Off by default.",
            },
            "input_image": {
                "display_name": "Input Image (base64)",
                "short_desc": "Base64-encoded input image (for `flux-kontext-*` models)",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Base64-encoded input image, used by the "
                    "`flux-kontext-pro` and `flux-kontext-max` models for "
                    "image-to-image editing.",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class BflGenerateImageResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Job ID",
                "desc": "BFL job identifier.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "status": {
                "display_name": "Status",
                "desc": "Final job status (always `Ready` when this response is "
                    "returned; non-Ready statuses raise `BFL-ERROR`).",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "result": {
                "display_name": "Result",
                "desc": "Result hash — contains `sample` (image URL) and "
                    "`seed` (seed used).  The URL points at a temporary CDN "
                    "location; download the image immediately to persist.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class BflGenerateImageDataProvider inherits BflDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "generate-image",
            "type": "BflGenerateImageDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        static BflGenerateImageRequestDataType RequestType =
            new BflGenerateImageRequestDataType();
        static BflGenerateImageResponseDataType ResponseType =
            new BflGenerateImageResponseDataType();
    }

    constructor(BflRestClient::BflRestClientIo rest)
            : BflDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : BflDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string model = req.model ?? BflRestClient::DefaultImageModel;
        *string path = BflRestClient::ImageModelPathMap{model};
        if (!path.val()) {
            throw "BFL-CONFIG-ERROR",
                sprintf("BFL image model %y is unknown; known models: %y",
                    model, keys BflRestClient::ImageModelPathMap);
        }
        hash<auto> body = req - "model";
        return rest.submitAndWait(path, body);
    }
}
}
