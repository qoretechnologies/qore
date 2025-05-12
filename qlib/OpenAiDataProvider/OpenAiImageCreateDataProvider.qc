# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAiDataProvider module definition

/** OpenAiImageCreateDataProvider.qc Copyright 2024 - 2025 Qore Technologies, s.r.o.

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
public class OpenAiImageCreateDataProvider inherits OpenAiDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-image",
            "desc": "Creates an image from a prompt",
            "type": "OpenAiImageCreateDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = OpenAiImageCreateRequestType;

        #! Response type
        const ResponseType = OpenAiCreateImageResponseType;
    }

    #! Creates the object from the arguments
    constructor(*RestClient rest) : OpenAiDataProviderCommon(rest) {
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
        # make the request and hardcode a base64-encoded JSON response
        return doRestCommand("POST", "images/generations", req + {"response_format": "b64_json"}).body;
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

#! The image generation request type
public class OpenAiImageCreateRequestType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "prompt": {
                "display_name": "Prompt",
                "short_desc": "A text description of the desired image(s).",
                "desc": "A text description of the desired image(s). The maximum length is 32000 characters for "
                    "`gpt-image-1`, 1000 characters for `dall-e-2` and 4000 characters for `dall-e-3`",
                "type": AbstractDataProviderTypeMap."string",
            },
            "background": {
                "display_name": "Background",
                "short_desc": "Allows to set transparency for the background of the generated image(s) when using "
                    "'gpt-image-1'",
                "desc": "Allows to set transparency for the background of the generated image(s) when using "
                    "`gpt-image-1`",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Transparent",
                        "value": "transparent",
                        "desc": "Transparent background",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Opaque",
                        "value": "opaque",
                        "desc": "Opaque background",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Auto",
                        "value": "auto",
                        "desc": "The model will automatically determine the best background for the image",
                    },
                ),
            },
            "model": {
                "display_name": "Model ID",
                "short_desc": "The model to use for image generation",
                "desc": "The model to use for image generation",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "dall-e-2",
                    },
                    <AllowedValueInfo>{
                        "value": "dall-e-3",
                    },
                    <AllowedValueInfo>{
                        "value": "gpt-image-1",
                    },
                ),
                "default_value": "dall-e-2",
            },
            "moderation": {
                "display_name": "Moderation",
                "short_desc": "Control the content-moderation level for images generated by 'gpt-image-1'",
                "desc": "Control the content-moderation level for images generated by `gpt-image-1`",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Auto",
                        "value": "auto",
                        "desc": "Automatic filtering",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Low",
                        "value": "low",
                        "desc": "Less restrictive filtering",
                    },
                ),
            },
            "n": {
                "display_name": "Number of Images",
                "short_desc": "The number of images to generate between 1 and 10",
                "desc": "The number of images to generate. For `dall-e-3`, only `n = 1` is supported",
                "type": AbstractDataProviderTypeMap."int",
                "default_value": 1,
            },
            "output_compression": {
                "display_name": "Output Compression",
                "short_desc": "The compression level (0-100%) for the generated image(s)",
                "desc": "The compression level (0-100%) for the generated image(s). This parameter is only supported "
                    "for `gpt-image-1` with the `webp` or `jpeg` output formats, and defaults to `100`",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "output_format": {
                "display_name": "Output Format",
                "short_desc": "The output format for the generated image(s)",
                "desc": "The output format for the generated image(s). This parameter is only supported for "
                    "`gpt-image-1` and defaults to `png`",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "PNG",
                        "value": "png",
                        "desc": "Portable Network Graphics (PNG) format",
                    },
                    <AllowedValueInfo>{
                        "display_name": "JPEG",
                        "value": "jpeg",
                        "desc": "Joint Photographic Experts Group (JPEG) format",
                    },
                    <AllowedValueInfo>{
                        "display_name": "WebP",
                        "value": "webp",
                        "desc": "WebP format",
                    },
                ),
            },
            "quality": {
                "display_name": "Image Quality",
                "short_desc": "The quality of the generated image(s)",
                "desc": "The quality of the generated image(s)",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Auto",
                        "value": "auto",
                        "desc": "The model will automatically determine the best quality for the image",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Low",
                        "value": "low",
                        "desc": "Low quality (`gpt-image-1` only)",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Medium",
                        "value": "medium",
                        "desc": "Medium quality (`gpt-image-1` only)",
                    },
                    <AllowedValueInfo>{
                        "display_name": "High",
                        "value": "high",
                        "desc": "High quality (`gpt-image-1` only)",
                    },
                    <AllowedValueInfo>{
                        "display_name": "HD",
                        "value": "hd",
                        "desc": "High definition (`dall-e-3` only)",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Standard",
                        "value": "standard",
                        "desc": "Standard quality (`dall-e-2` and `dall-e-3` only)",
                    },
                ),
            },
            "size": {
                "display_name": "Image Size",
                "short_desc": "The size of the generated image(s)",
                "desc": "The size of the generated image(s)",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Auto",
                        "value": "auto",
                        "desc": "The default size for the model (`gpt-image-1` only)",
                    },
                    <AllowedValueInfo>{
                        "display_name": "256x256",
                        "value": "256x256",
                        "desc": "256x256 pixels (`dall-e-2` only)",
                    },
                    <AllowedValueInfo>{
                        "display_name": "512x512",
                        "value": "512x512",
                        "desc": "512x512 pixels (`dall-e-2` and `dall-e-3` only)",
                    },
                    <AllowedValueInfo>{
                        "display_name": "1024x1024",
                        "value": "1024x1024",
                        "desc": "1024x1024 pixels (all models)",
                    },
                    <AllowedValueInfo>{
                        "display_name": "1536x1024 Landscape",
                        "value": "1536x1024",
                        "desc": "1536x1024 pixels landscape (`gpt-image-1` only)",
                    },
                    <AllowedValueInfo>{
                        "display_name": "1024x1536 Portrait",
                        "value": "1024x1536",
                        "desc": "1024x1536 pixels portrait (`gpt-image-1` only)",
                    },
                    <AllowedValueInfo>{
                        "display_name": "1792x1024 Landscape",
                        "value": "1792x1024",
                        "desc": "1792x1024 pixels landscape (`dall-e-3` only)",
                    },
                    <AllowedValueInfo>{
                        "display_name": "1024x1792 Portrait",
                        "value": "1024x1792",
                        "desc": "1024x1792 pixels portrait (`dall-e-3` only)",
                    },
                ),
            },
            "style": {
                "display_name": "Image Style",
                "short_desc": "The style of the generated image(s) for 'dall-e-3' only",
                "desc": "The style of the generated image(s) for `dall-e-3` only",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Vivid",
                        "value": "vivid",
                        "desc": "Causes the model to lean towards generating hyper-real and dramatic images",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Natural",
                        "value": "natural",
                        "desc": "Causes the model to produce more natural, less hyper-real looking images",
                    },
                ),
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

    #! Creates the type
    constructor() : HashDataType("OpenAiImageCreateRequest") {
        addQoreFields(Fields);
    }
}

#! Image generation request type constant
public const OpenAiImageCreateRequestType = new OpenAiImageCreateRequestType();

#! Image data type constant
public const OpenAiImageDataType = new OpenAiImageDataType();

public class OpenAiImageDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "b64_json": {
                "display_name": "Base64 Image Data",
                "short_desc": "The base64-encoded image data",
                "desc": "The base64-encoded JSON of the generated image. Default value for `gpt-image-1`, and only "
                    "present if `response_format` is set to `b64_json` for `dall-e-2` and `dall-e-3`",
                "type": AbstractDataProviderTypeMap."*string",
                "exclusive_with": ("url",),
            },
            "revised_prompt": {
                "display_name": "Revised Prompt",
                "short_desc": "The revised prompt used to generate the image ('dall-e-3' only)",
                "desc": "The revised prompt used to generate the image (`dall-e-3` only)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "url": {
                "display_name": "Image URL",
                "short_desc": "The URL of the image",
                "desc": "The URL of the image",
                "type": AbstractDataProviderTypeMap."*string",
                "exclusive_with": ("b64_json",),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiImage") {
        addQoreFields(Fields);
    }
}

#! Image type
public class OpenAiCreateImageResponseType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "created": {
                "display_name": "Created Timestamp",
                "short_desc": "The date and time the image was created",
                "desc": "The date and time the image was created",
                "type": AbstractDataProviderTypeMap.epoch,
            },
            "data": {
                "display_name": "Images",
                "short_desc": "The image(s) created",
                "desc": "The image(s) created",
                "type": new ListDataType("ImageList", OpenAiImageDataType),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiImageDataType") {
        addQoreFields(Fields);
    }
}

#! Create image response type constant
public const OpenAiCreateImageResponseType = new OpenAiCreateImageResponseType();
}
