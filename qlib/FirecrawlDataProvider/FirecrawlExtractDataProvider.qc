# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlExtractDataProvider.qc Firecrawl extract action data provider

/*  FirecrawlExtractDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Firecrawl extract action data provider (async: POST then auto-poll)
public class FirecrawlExtractDataProvider inherits FirecrawlDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "extract",
            "desc": "Extracts structured data from one or more web pages using AI. "
                "Automatically polls until the extraction completes",
            "type": "FirecrawlExtractDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(*RestClientIo::RestClientIo rest) : FirecrawlDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "extract";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Extracts structured data from one or more web pages using AI. "
            "Automatically polls until the extraction completes";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new FirecrawlExtractRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new FirecrawlExtractResponseDataType();
    }

    #! Performs the request (POST + auto-poll)
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPostAndWait("extract", "extract", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace FirecrawlDataProvider
