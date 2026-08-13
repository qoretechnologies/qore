# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore StabilityAiDataProvider data type definitions

/** StabilityAiDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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
public class StabilityAiGenerateImageRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "prompt": {
                "display_name": "Prompt",
                "short_desc": "Text prompt describing the image",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Text prompt describing the desired image.  Be specific "
                    "about subject, style, lighting, and composition for best "
                    "results.",
                "required": True,
                "example_value": "a serene mountain landscape at sunrise, "
                    "painted in the style of Caspar David Friedrich",
            },
            "model": {
                "display_name": "Model",
                "short_desc": "Stability AI model identifier",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Stability AI model identifier — `ultra` for highest "
                    "quality (SD3 Ultra), `sd3` for Stable Diffusion 3 / 3.5, "
                    "or `core` for the faster / cheaper model.",
                "default_value": "ultra",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "ultra", "display_name": "Ultra (highest quality)"},
                    <AllowedValueInfo>{"value": "sd3",   "display_name": "Stable Diffusion 3 / 3.5"},
                    <AllowedValueInfo>{"value": "core",  "display_name": "Core (faster, cheaper)"},
                ),
            },
            "output_format": {
                "display_name": "Output Format",
                "short_desc": "Output image format",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Output image format.",
                "default_value": "png",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "png",  "display_name": "PNG"},
                    <AllowedValueInfo>{"value": "jpeg", "display_name": "JPEG"},
                    <AllowedValueInfo>{"value": "webp", "display_name": "WebP"},
                ),
            },
            "aspect_ratio": {
                "display_name": "Aspect Ratio",
                "short_desc": "Output aspect ratio",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Output aspect ratio.  Common values: `1:1` (square), "
                    "`16:9` (landscape video), `9:16` (portrait video), `4:3`, "
                    "`3:4`, `21:9` (cinemascope).",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "1:1",   "display_name": "1:1 (Square)"},
                    <AllowedValueInfo>{"value": "16:9",  "display_name": "16:9 (Landscape Video)"},
                    <AllowedValueInfo>{"value": "9:16",  "display_name": "9:16 (Portrait Video)"},
                    <AllowedValueInfo>{"value": "4:3",   "display_name": "4:3 (Standard)"},
                    <AllowedValueInfo>{"value": "3:4",   "display_name": "3:4 (Portrait)"},
                    <AllowedValueInfo>{"value": "21:9",  "display_name": "21:9 (Cinemascope)"},
                    <AllowedValueInfo>{"value": "9:21",  "display_name": "9:21 (Tall Cinemascope)"},
                    <AllowedValueInfo>{"value": "2:3",   "display_name": "2:3 (Tall Portrait)"},
                    <AllowedValueInfo>{"value": "3:2",   "display_name": "3:2 (Wide Landscape)"},
                    <AllowedValueInfo>{"value": "5:4",   "display_name": "5:4"},
                    <AllowedValueInfo>{"value": "4:5",   "display_name": "4:5"},
                ),
            },
            "negative_prompt": {
                "display_name": "Negative Prompt",
                "short_desc": "What you don't want in the image",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Text describing what to avoid in the image — e.g. "
                    "`blurry, low quality, text, watermark`.",
                "example_value": "blurry, low quality, watermark",
            },
            "seed": {
                "display_name": "Seed",
                "short_desc": "Random seed for reproducibility (0–4294967294)",
                "type": AbstractDataProviderTypeMap."*softint",
                "desc": "Random seed used for generation — passing the same "
                    "seed + prompt should produce the same image.  `0` (default) "
                    "uses a random seed.",
            },
            "style_preset": {
                "display_name": "Style Preset",
                "short_desc": "Pre-baked style preset (core model only)",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Pre-baked style preset.  Only honoured by the `core` "
                    "model; ignored by `sd3` and `ultra`.",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "photographic",       "display_name": "Photographic"},
                    <AllowedValueInfo>{"value": "cinematic",          "display_name": "Cinematic"},
                    <AllowedValueInfo>{"value": "digital-art",        "display_name": "Digital Art"},
                    <AllowedValueInfo>{"value": "anime",              "display_name": "Anime"},
                    <AllowedValueInfo>{"value": "comic-book",         "display_name": "Comic Book"},
                    <AllowedValueInfo>{"value": "fantasy-art",        "display_name": "Fantasy Art"},
                    <AllowedValueInfo>{"value": "line-art",           "display_name": "Line Art"},
                    <AllowedValueInfo>{"value": "analog-film",        "display_name": "Analog Film"},
                    <AllowedValueInfo>{"value": "neon-punk",          "display_name": "Neon Punk"},
                    <AllowedValueInfo>{"value": "isometric",          "display_name": "Isometric"},
                    <AllowedValueInfo>{"value": "low-poly",           "display_name": "Low Poly"},
                    <AllowedValueInfo>{"value": "origami",            "display_name": "Origami"},
                    <AllowedValueInfo>{"value": "modeling-compound",  "display_name": "Modeling Compound"},
                    <AllowedValueInfo>{"value": "3d-model",           "display_name": "3D Model"},
                    <AllowedValueInfo>{"value": "pixel-art",          "display_name": "Pixel Art"},
                    <AllowedValueInfo>{"value": "tile-texture",       "display_name": "Tile Texture"},
                ),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class StabilityAiGenerateImageResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "image": {
                "display_name": "Image Bytes",
                "desc": "Raw image bytes in the requested format.",
                "type": AbstractDataProviderTypeMap."binary",
            },
            "content_type": {
                "display_name": "Content Type",
                "desc": "MIME type of the image (e.g. `image/png`, "
                    "`image/jpeg`, `image/webp`).",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "finish_reason": {
                "display_name": "Finish Reason",
                "desc": "Stability AI finish reason — typically `SUCCESS` or "
                    "`CONTENT_FILTERED` when the safety filter blocked the request.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "seed": {
                "display_name": "Seed",
                "desc": "The seed Stability AI used for this generation.",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}
}
