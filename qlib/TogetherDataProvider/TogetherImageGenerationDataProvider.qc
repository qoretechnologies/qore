# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TogetherImageGenerationDataProvider class definition

/** TogetherImageGenerationDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace TogetherDataProvider {
#! Data provider for the Together AI `/v1/images/generations` endpoint
/** Generates one or more images from a text prompt using a Together-hosted
    diffusion model (FLUX, Stable Diffusion XL).
*/
public class TogetherImageGenerationDataProvider inherits TogetherDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "generate-image",
            "type": "TogetherImageGenerationDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        static TogetherImageGenerationRequestDataType RequestType =
            new TogetherImageGenerationRequestDataType();

        static TogetherImageGenerationResponseDataType ResponseType =
            new TogetherImageGenerationResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : TogetherDataProviderCommon(rest) {
    }

    constructor(*hash<auto> options) : TogetherDataProviderCommon(options) {
    }

    string getName() {
        return ProviderInfo.name;
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Generates images from a text prompt and returns the response
    /** Converts Together's Unix-seconds `created` field to a Qore date on
        the response to match the typed `*date` field in the response schema.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = doRestCommand("POST", "images/generations", req).body;
        if (body.created.typeCode() == NT_INT) {
            body.created = 1970-01-01Z + seconds(body.created);
        }
        return body;
    }
}
}
