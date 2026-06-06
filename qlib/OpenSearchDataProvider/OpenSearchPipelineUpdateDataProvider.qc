# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenSearchDataProvider module definition

/** OpenSearchPipelineUpdateDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the OpenSearchDataProvider module
public namespace OpenSearchDataProvider {
#! The OpenSearch pipeline update API data provider
/** This API allows the caller to update an OpenSearch pipeline
*/
public class OpenSearchPipelineUpdateDataProvider inherits OpenSearchPipelineCreateDataProvider {
    public {
        #! Provider info
        const hash<DataProviderInfo> ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "OpenSearch pipeline update API data provider",
            "type": "OpenSearchPipelineUpdateDataProvider",
            "constructor_options": OpenSearchDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const hash<DataProvider::DataProviderSummaryInfo> ProviderSummaryInfo = cast<hash<DataProvider::DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : OpenSearchPipelineCreateDataProvider(options) {
    }

    #! Creates the object from a REST connection
    constructor(RestClient::AbstractRestClient rest) : OpenSearchPipelineCreateDataProvider(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("OpenSearch data pipeline %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
