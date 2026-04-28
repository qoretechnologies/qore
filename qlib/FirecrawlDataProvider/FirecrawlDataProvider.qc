# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlDataProvider.qc Firecrawl root data provider

/*  FirecrawlDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! The main Firecrawl data provider class (root provider)
public class FirecrawlDataProvider inherits FirecrawlDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "firecrawl",
            "desc": "Firecrawl data provider for web scraping, crawling, mapping, search, "
                "extraction, and AI agent tasks",
            "type": "FirecrawlDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
        };

        #! Child provider map
        const ChildMap = {
            "scrape": Class::forName("FirecrawlScrapeDataProvider"),
            "crawl": Class::forName("FirecrawlCrawlDataProvider"),
            "get-crawl-status": Class::forName("FirecrawlGetCrawlStatusDataProvider"),
            "cancel-crawl": Class::forName("FirecrawlCancelCrawlDataProvider"),
            "batch-scrape": Class::forName("FirecrawlBatchScrapeDataProvider"),
            "get-batch-scrape-status": Class::forName("FirecrawlGetBatchScrapeStatusDataProvider"),
            "cancel-batch-scrape": Class::forName("FirecrawlCancelBatchScrapeDataProvider"),
            "map": Class::forName("FirecrawlMapDataProvider"),
            "search": Class::forName("FirecrawlSearchDataProvider"),
            "extract": Class::forName("FirecrawlExtractDataProvider"),
            "get-extract-status": Class::forName("FirecrawlGetExtractStatusDataProvider"),
            "agent": Class::forName("FirecrawlAgentDataProvider"),
            "get-agent-status": Class::forName("FirecrawlGetAgentStatusDataProvider"),
            "cancel-agent": Class::forName("FirecrawlCancelAgentDataProvider"),
            "parse": Class::forName("FirecrawlParseDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : FirecrawlDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(*RestClientIo::RestClientIo rest) : FirecrawlDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "firecrawl";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Firecrawl data provider for web scraping, crawling, mapping, search, "
            "extraction, and AI agent tasks";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace FirecrawlDataProvider
