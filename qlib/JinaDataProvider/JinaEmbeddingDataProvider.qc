# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JinaEmbeddingDataProvider class definition

/** JinaEmbeddingDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace JinaDataProvider {
public class JinaEmbeddingDataProvider inherits JinaDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-embeddings",
            "type": "JinaEmbeddingDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });


    }

    private {
        #! The request type, built on first use; see requestType()
        static *JinaEmbeddingRequestDataType request_type;
        #! The ID of the request type's builder; see requestType()
        static int request_type_id = DataProviderActionCatalog::registerLazyValue(
            \JinaEmbeddingDataProvider::buildRequestType());
        #! The response type, built on first use; see responseType()
        static *JinaEmbeddingResponseDataType response_type;
        #! The ID of the response type's builder; see responseType()
        static int response_type_id = DataProviderActionCatalog::registerLazyValue(
            \JinaEmbeddingDataProvider::buildResponseType());
    }

    #! Returns the request type, which is built on first use
    /** Built from the vendored API description, so loading the module does not parse it
    */
    static JinaEmbeddingRequestDataType requestType() {
        return request_type ?? (request_type = DataProviderActionCatalog::getLazyValue(request_type_id));
    }

    #! Builds the request type; see requestType()
    private static JinaEmbeddingRequestDataType buildRequestType() {
        return new JinaEmbeddingRequestDataType();
    }

    #! Returns the response type, which is built on first use
    /** Built from the vendored API description, so loading the module does not parse it
    */
    static JinaEmbeddingResponseDataType responseType() {
        return response_type ?? (response_type = DataProviderActionCatalog::getLazyValue(response_type_id));
    }

    #! Builds the response type; see responseType()
    private static JinaEmbeddingResponseDataType buildResponseType() {
        return new JinaEmbeddingResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : JinaDataProviderCommon(rest) {
    }

    constructor(*hash<auto> options) : JinaDataProviderCommon(options) {
    }

    string getName() {
        return ProviderInfo.name;
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return getSchemaProvider("create-embeddings").getRequestType();
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return getSchemaProvider("create-embeddings").getResponseType();
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accepts the former encoding option as an alias for Jina's actual embedding_type field
    /** @param req the embedding options
        @param request_options additional request options
        @return the embedding response
        @throw JINA-REQUEST-ERROR conflicting encoding aliases were supplied
    */
    auto doRequest(auto req, *hash<auto> request_options) {
        if (req.typeCode() == NT_HASH && exists req.encoding_format) {
            if (exists req.embedding_type && req.embedding_type != req.encoding_format) {
                throw "JINA-REQUEST-ERROR", "encoding_format conflicts with embedding_type";
            }
            req.embedding_type = remove req.encoding_format;
        }
        return AbstractDataProvider::doRequest(req, request_options);
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return getSchemaProvider("create-embeddings").doRequest(req, request_options);
    }
}
}
