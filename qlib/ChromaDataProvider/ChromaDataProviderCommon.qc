# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChromaDataProviderCommon class definition

/** ChromaDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace ChromaDataProvider {
public class ChromaDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        const DefaultChromaUrl = "http://localhost:8000";

        const ConstructorOptions = {
            "url": <DataProviderOptionInfo>{
                "display_name": "Server URL",
                "short_desc": "Chroma server URL",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "URL of the Chroma server.",
                "default_value": DefaultChromaUrl,
            },
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "Optional Bearer token (Chroma Cloud / proxied)",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Optional API key for Chroma Cloud / reverse-proxied deployments.",
                "sensitive": True,
            },
            "tenant": <DataProviderOptionInfo>{
                "display_name": "Tenant",
                "short_desc": "Chroma tenant",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Chroma tenant name.",
                "default_value": "default_tenant",
            },
            "database": <DataProviderOptionInfo>{
                "display_name": "Database",
                "short_desc": "Chroma database",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Chroma database name.",
                "default_value": "default_database",
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo"), NOTHING, {
                        DTT_ClientOnly: True,
                    }),
                "desc": "The REST client object to use",
            },
        };
    }

    private {
        *RestClientIo::RestClientIo rest;
        string tenant = "default_tenant";
        string database = "default_database";
    }

    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        }
        if (copts.tenant.val()) {
            tenant = copts.tenant;
        }
        if (copts.database.val()) {
            database = copts.database;
        }
        if (!rest) {
            hash<auto!> opts = {
                "url": copts.url ?? DefaultChromaUrl,
            };
            if (copts.apikey.val()) {
                opts.apikey = copts.apikey;
            }
            rest = new ChromaRestClient::ChromaRestClientIo(opts);
        }
    }

    private string collectionPath(string collection_id, string subpath) {
        return sprintf("/api/v2/tenants/%s/databases/%s/collections/%s%s",
            tenant, database, collection_id, subpath);
    }
}
}
