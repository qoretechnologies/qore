# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JinaRerankDataProvider class definition

/** JinaRerankDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
public class JinaRerankDataProvider inherits JinaDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "rerank",
            "type": "JinaRerankDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });


    }

    private {
        #! The request type, built on first use; see requestType()
        static *JinaRerankRequestDataType request_type;
        #! Serializes building the request type
        static Mutex request_type_lock();
        #! The response type, built on first use; see responseType()
        static *JinaRerankResponseDataType response_type;
        #! Serializes building the response type
        static Mutex response_type_lock();
    }

    #! Returns the request type, which is built on first use
    /** Built from the vendored API description, so loading the module does not parse it
    */
    static JinaRerankRequestDataType requestType() {
        AutoLock al(request_type_lock);
        if (!request_type) {
            request_type = new JinaRerankRequestDataType();
        }
        return request_type;
    }

    #! Returns the response type, which is built on first use
    /** Built from the vendored API description, so loading the module does not parse it
    */
    static JinaRerankResponseDataType responseType() {
        AutoLock al(response_type_lock);
        if (!response_type) {
            response_type = new JinaRerankResponseDataType();
        }
        return response_type;
    }

    constructor(RestClientIo::RestClientIo rest) : JinaDataProviderCommon(rest) {
    }

    constructor(*hash<auto> options) : JinaDataProviderCommon(options) {
    }

    string getName() {
        return ProviderInfo.name;
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return getSchemaProvider("rerank").getRequestType();
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return getSchemaProvider("rerank").getResponseType();
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return getSchemaProvider("rerank").doRequest(req, request_options);
    }
}
}
