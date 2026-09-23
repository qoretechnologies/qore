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

        static *StabilityAiGenerateImageRequestDataType RequestType;
        static StabilityAiGenerateImageResponseDataType ResponseType =
            new StabilityAiGenerateImageResponseDataType();
    }

    private { static Mutex type_mutex(); }

    #! Materializes schema-derived fields once after the independent inventory is registered
    /** @throw REST-SCHEMA-ACTION-ERROR the pinned input schema is invalid
        @note RequestType is immutable after initialization; inventory callbacks never call this method.
    */
    static initializeTypes() {
        AutoLock lock(type_mutex);
        if (!RequestType) { RequestType = new StabilityAiGenerateImageRequestDataType(); }
    }

    constructor(StabilityAiRestClient::StabilityAiRestClientIo rest)
            : StabilityAiDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : StabilityAiDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Rejects explicit null fields before optional type conversion can discard them
    /** @param req compatible generation options
        @param request_options provider request options
        @return verified image result
        @throw STABILITYAI-INPUT-ERROR an option is null or unknown
    */
    auto doRequest(auto req, *hash<auto> request_options) {
        if (req.typeCode() != NT_HASH) { throw "STABILITYAI-INPUT-ERROR", "action options must be an object"; }
        foreach hash<auto> item in (req.pairIterator()) {
            if (!RequestType.getFields().hasKey(item.key) || !exists item.value || item.value.typeCode() == NT_NULL) {
                throw "STABILITYAI-INPUT-ERROR", sprintf("unknown or null option %y", item.key);
            }
        }
        if (req.hasKey("seed") && (req.seed.typeCode() == NT_BOOLEAN
                || (req.seed.typeCode() == NT_STRING && req.seed !~ /^\d+$/)
                || (inlist(req.seed.typeCode(), (NT_FLOAT, NT_NUMBER)) && req.seed != int(req.seed)))) {
            throw "STABILITYAI-INPUT-ERROR", "seed must be an integer";
        }
        return StabilityAiDataProviderCommon::doRequest(req, request_options);
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return StabilityAiApi::generateImage(rest, req);
    }
}
}
