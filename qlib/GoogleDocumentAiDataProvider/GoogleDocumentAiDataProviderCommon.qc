# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GoogleDocumentAiDataProviderCommon class definition

/** GoogleDocumentAiDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace GoogleDocumentAiDataProvider {
public class GoogleDocumentAiDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        const DefaultRegion = "us";

        const ConstructorOptions = {
            "gcp_project_id": <DataProviderOptionInfo>{
                "display_name": "GCP Project ID",
                "short_desc": "GCP project that owns the Document AI processors",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "GCP project ID — used to build the processor URL.",
                "required": True,
                "example_value": "my-gcp-project-123",
            },
            "gcp_region": <DataProviderOptionInfo>{
                "display_name": "GCP Region",
                "short_desc": "Document AI processor region",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Document AI region (e.g. `us`, `eu`).  Determines the "
                    "regional endpoint hostname.",
                "default_value": DefaultRegion,
            },
            "gcp_sa_json_path": <DataProviderOptionInfo>{
                "display_name": "Service-Account JSON Path",
                "short_desc": "Path to a service-account JSON key file",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Explicit path to a GCP service-account JSON file for "
                    "JWT-bearer auth.  When omitted, Application Default "
                    "Credentials are used.",
                "sensitive": True,
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("GoogleDocumentAiRestClient::GoogleDocumentAiRestClientIo"),
                    NOTHING, {DTT_ClientOnly: True}),
                "desc": "The Google Document AI REST client object to use",
            },
        };

        const Scopes = ("https://www.googleapis.com/auth/cloud-platform",);
    }

    private {
        *GoogleDocumentAiRestClient::GoogleDocumentAiRestClientIo rest;
        string project_id;
        string region = DefaultRegion;
        *string sa_json_path;
    }

    constructor(GoogleDocumentAiRestClient::GoogleDocumentAiRestClientIo rest,
            string project_id, *string region, *string sa_json_path) {
        self.rest = rest;
        self.project_id = project_id;
        if (region.val()) {
            self.region = region;
        }
        self.sa_json_path = sa_json_path;
    }

    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (!copts.gcp_project_id.val()) {
            throw "CONSTRUCTOR-ERROR",
                "missing required 'gcp_project_id' argument for Google Document AI";
        }
        project_id = copts.gcp_project_id;
        if (copts.gcp_region.val()) {
            region = copts.gcp_region;
        }
        sa_json_path = copts.gcp_sa_json_path;
        if (copts.restclient) {
            rest = copts.restclient;
        } else {
            rest = new GoogleDocumentAiRestClient::GoogleDocumentAiRestClientIo({
                "gcp_region": region,
            });
        }
    }

    #! Posts a Document AI request with a fresh GCP access token
    private hash<auto> doDocAiRequest(string path, hash<auto> body) {
        string token = GcpAuth::getAccessToken(sa_json_path, Scopes);
        return rest.restPost(path, body, {"Authorization": "Bearer " + token}).body;
    }
}
}
