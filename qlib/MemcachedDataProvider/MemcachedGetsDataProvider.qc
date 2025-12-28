# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MemcachedGetsDataProvider class definition

/** MemcachedGetsDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

%require-types
%enable-all-warnings
%new-style
%strict-args

public namespace MemcachedDataProvider {
#! Data provider for memcached gets operation (get with CAS token)
public class MemcachedGetsDataProvider inherits MemcachedDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "gets",
            "desc": "Memcached gets operation - retrieves a value with its CAS token for optimistic locking",
            "type": "MemcachedGetsDataProvider",
            "constructor_options": MemcachedDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = MemcachedGetRequestDataType;

        #! Response type
        const ResponseType = MemcachedGetResponseDataType;
    }

    #! Creates the object from a MemcachedClient
    constructor(MemcachedClient memcached) : MemcachedDataProviderBase(memcached) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : MemcachedDataProviderBase(options) {
    }

    #! Returns the name of the data provider
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns a description of the data provider
    *string getDesc() {
        return sprintf("Memcached gets provider for `%s`", memcached.getSafeURL());
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return memcached.gets(req.key);
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns provider info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
