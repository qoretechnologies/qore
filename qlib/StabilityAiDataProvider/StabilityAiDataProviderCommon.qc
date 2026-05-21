# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore StabilityAiDataProviderCommon class definition

/** StabilityAiDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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
public class StabilityAiDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "Stability AI API key (Bearer token)",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Stability AI API key.",
                "required": True,
                "sensitive": True,
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "Server URL",
                "short_desc": "The URL of the Stability AI REST API",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The URL of the Stability AI REST API.",
                "default_value": "https://api.stability.ai",
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("StabilityAiRestClient::StabilityAiRestClientIo"),
                    NOTHING, {DTT_ClientOnly: True}),
                "desc": "The Stability AI REST client object to use",
            },
        };
    }

    private {
        *StabilityAiRestClient::StabilityAiRestClientIo rest;
    }

    constructor(StabilityAiRestClient::StabilityAiRestClientIo rest) {
        self.rest = rest;
    }

    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        }
        if (!rest) {
            if (!copts.apikey.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "missing required 'apikey' argument for Stability AI";
            }
            rest = new StabilityAiRestClient::StabilityAiRestClientIo({
                "apikey": copts.apikey, "url": copts.url,
            });
        }
    }
}
}
