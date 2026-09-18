# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AzureDocumentIntelligenceDataProviderCommon class definition

/** AzureDocumentIntelligenceDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace AzureDocumentIntelligenceDataProvider {
public class AzureDocumentIntelligenceDataProviderCommon
        inherits DataProvider::AbstractDataProvider {
    public {
        const ConstructorOptions = {
            "url": <DataProviderOptionInfo>{
                "display_name": "Azure Endpoint URL",
                "short_desc": "Azure Document Intelligence resource URL",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Azure Document Intelligence resource URL — typically "
                    "`https://<resource-name>.cognitiveservices.azure.com`.  "
                    "Find this in the Azure portal under your Document "
                    "Intelligence resource > Keys and Endpoint.",
                "example_value": "https://my-docint.cognitiveservices.azure.com",
            },
            "apikey": <DataProviderOptionInfo>{
                "display_name": "Subscription Key",
                "short_desc": "Azure Document Intelligence subscription key",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Azure Document Intelligence subscription key — sent as "
                    "the `Ocp-Apim-Subscription-Key` header.",
                "sensitive": True,
            },
            "entra_token": <DataProviderOptionInfo>{
                "display_name": "Entra Bearer Token", "short_desc": "A caller-managed Entra token",
                "type": AbstractDataProviderTypeMap."*string", "sensitive": True,
                "desc": "Access token for `https://cognitiveservices.azure.com/.default`. "
                    "Use instead of `apikey`; the caller replaces expiring tokens.",
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("AzureDocumentIntelligenceRestClient::AzureDocumentIntelligenceRestClientIo"),
                    NOTHING, {DTT_ClientOnly: True}),
                "desc": "The Azure Document Intelligence REST client object to use",
            },
        };
    }

    private {
        *AzureDocumentIntelligenceRestClient::AzureDocumentIntelligenceRestClientIo rest;
    }

    constructor(AzureDocumentIntelligenceRestClient::AzureDocumentIntelligenceRestClientIo rest) {
        self.rest = rest;
    }

    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        }
        if (!rest) {
            if (!copts.url.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "missing required 'url' argument for Azure Document Intelligence";
            }
            rest = new AzureDocumentIntelligenceRestClient::AzureDocumentIntelligenceRestClientIo({
                "url":    copts.url,
            } + copts{"apikey", "entra_token"});
        }
    }

    #! Returns the resource-scoped references offered by this provider
    /** @return supported reference names */
    *hash<string, bool> getSupportedReferenceData() {
        return {"models": True, "classifiers": True, "operations": True};
    }

    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        return AzureDocumentIntelligenceReferences::get(rest, type, action_opts);
    }
}
}
