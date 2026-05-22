# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AssemblyAiDataProviderCommon class definition

/** AssemblyAiDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace AssemblyAiDataProvider {
public class AssemblyAiDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        const DefaultAssemblyAiUrl = "https://api.assemblyai.com";

        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "AssemblyAI API Key",
                "short_desc": "AssemblyAI API key (sent as bare `Authorization` header)",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The AssemblyAI API key — sent as the bare `Authorization` "
                    "header value with no `Bearer` / `Token` prefix.",
                "required": True,
                "sensitive": True,
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "Server URL",
                "short_desc": "The URL of the AssemblyAI REST API",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The URL of the AssemblyAI REST API.",
                "default_value": DefaultAssemblyAiUrl,
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("AssemblyAiRestClient::AssemblyAiRestClientIo"), NOTHING, {
                        DTT_ClientOnly: True,
                    }),
                "desc": "The AssemblyAI REST client object to use",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options for the REST client constructor",
                "type": AbstractDataProviderType::get(AutoHashType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Options forwarded to the AssemblyAiRestClientIo constructor.",
            },
        };
    }

    private {
        *AssemblyAiRestClient::AssemblyAiRestClientIo rest;
    }

    constructor(AssemblyAiRestClient::AssemblyAiRestClientIo rest) {
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
                    "missing required 'apikey' argument for AssemblyAI API communication";
            }
            hash<auto> opts = (copts.restclient_options ?? {}) + {
                "url": copts.url,
                "apikey": copts.apikey,
            };
            rest = new AssemblyAiRestClient::AssemblyAiRestClientIo(opts);
        }
    }
}
}
