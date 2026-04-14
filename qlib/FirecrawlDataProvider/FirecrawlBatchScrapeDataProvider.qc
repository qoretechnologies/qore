# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlBatchScrapeDataProvider.qc Firecrawl batch-scrape action data provider

/*  FirecrawlBatchScrapeDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FirecrawlDataProvider module
public namespace FirecrawlDataProvider {

#! Firecrawl batch-scrape action data provider (async: POST then auto-poll)
public class FirecrawlBatchScrapeDataProvider inherits FirecrawlDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "batch-scrape",
            "desc": "Scrapes multiple URLs in batch and returns their content. "
                "Automatically polls until the batch scrape completes",
            "type": "FirecrawlBatchScrapeDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(*RestClientIo::RestClientIo rest) : FirecrawlDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "batch-scrape";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Scrapes multiple URLs in batch and returns their content. "
            "Automatically polls until the batch scrape completes";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new FirecrawlBatchScrapeRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new FirecrawlBatchScrapeResponseDataType();
    }

    #! Performs the request (POST + auto-poll)
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPostAndWait("batch/scrape", "batch/scrape", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace FirecrawlDataProvider
