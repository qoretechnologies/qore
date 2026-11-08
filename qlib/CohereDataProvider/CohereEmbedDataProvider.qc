# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CohereEmbedDataProvider class definition

/** CohereEmbedDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace CohereDataProvider {
#! Data provider for Cohere's <tt>/v2/embed</tt> endpoint
public class CohereEmbedDataProvider inherits CohereDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "embed",
            "type": "CohereEmbedDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! The request type, built on first use; see requestType()
        static *CohereEmbedRequestDataType request_type;
        #! The ID of the request type's builder; see requestType()
        static int request_type_id = DataProviderActionCatalog::registerLazyValue(
            auto sub () { return CohereEmbedDataProvider::buildRequestType(); });
        #! The response type, built on first use; see responseType()
        static *CohereEmbedResponseDataType response_type;
        #! The ID of the response type's builder; see responseType()
        static int response_type_id = DataProviderActionCatalog::registerLazyValue(
            auto sub () { return CohereEmbedDataProvider::buildResponseType(); });
    }

    #! Returns the request type, which is built on first use
    /** Built from the vendored API description, so loading the module does not parse it
    */
    static CohereEmbedRequestDataType requestType() {
        return request_type ?? (request_type = DataProviderActionCatalog::getLazyValue(request_type_id));
    }

    #! Builds the request type; see requestType()
    private static CohereEmbedRequestDataType buildRequestType() {
        return new CohereEmbedRequestDataType();
    }

    #! Returns the response type, which is built on first use
    /** Built from the vendored API description, so loading the module does not parse it
    */
    static CohereEmbedResponseDataType responseType() {
        return response_type ?? (response_type = DataProviderActionCatalog::getLazyValue(response_type_id));
    }

    #! Builds the response type; see responseType()
    private static CohereEmbedResponseDataType buildResponseType() {
        return new CohereEmbedResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : CohereDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : CohereDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return requestType(); }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return responseType(); }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Forwards the embedding request to Cohere's <tt>/v2/embed</tt> endpoint
    /** Cohere v2 requires <tt>embedding_types</tt>; if not supplied, defaults to
        <tt>["float"]</tt> so the response uses <tt>embeddings.float</tt>.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return getSchemaProvider("embed").doRequest(req, request_options);
    }
}
}
