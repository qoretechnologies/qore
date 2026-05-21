# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Tavily sub-provider class definitions (search / extract / crawl / map)

/** TavilySubProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace TavilyDataProvider {
public class TavilySearchDataProvider inherits TavilyDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "search",
            "type": "TavilySearchDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        static TavilySearchRequestDataType RequestType = new TavilySearchRequestDataType();
        static TavilySearchResponseDataType ResponseType = new TavilySearchResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : TavilyDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : TavilyDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("POST", "search", req).body;
    }
}

public class TavilyExtractDataProvider inherits TavilyDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "extract",
            "type": "TavilyExtractDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        static TavilyExtractRequestDataType RequestType = new TavilyExtractRequestDataType();
        static TavilyExtractResponseDataType ResponseType =
            new TavilyExtractResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : TavilyDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : TavilyDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("POST", "extract", req).body;
    }
}

public class TavilyCrawlDataProvider inherits TavilyDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "crawl",
            "type": "TavilyCrawlDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        static TavilyCrawlRequestDataType RequestType = new TavilyCrawlRequestDataType();
        static TavilyCrawlResponseDataType ResponseType = new TavilyCrawlResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : TavilyDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : TavilyDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("POST", "crawl", req).body;
    }
}

public class TavilyMapDataProvider inherits TavilyDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "map",
            "type": "TavilyMapDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        static TavilyMapRequestDataType RequestType = new TavilyMapRequestDataType();
        static TavilyMapResponseDataType ResponseType = new TavilyMapResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : TavilyDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : TavilyDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("POST", "map", req).body;
    }
}
}
