# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore VertexAiDataProvider common base class

/** VertexAiDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the VertexAiDataProvider module
public namespace VertexAiDataProvider {
#! Common base for Vertex AI data providers
/** Resolves a GCP access token via @ref gcpauthintro "GcpAuth" before
    every request so cached tokens are refreshed automatically when within
    the safety window.
*/
public class VertexAiDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        const DefaultRegion = "us-central1";
        const DefaultScopes = ("https://www.googleapis.com/auth/cloud-platform",);

        const ConstructorOptions = {
            "gcp_project_id": <DataProviderOptionInfo>{
                "display_name": "GCP Project ID",
                "short_desc": "GCP project owning the Vertex AI workload",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "GCP project ID (required)",
            },
            "gcp_region": <DataProviderOptionInfo>{
                "display_name": "GCP Region",
                "short_desc": "Vertex AI region",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Vertex AI region (e.g. `us-central1`, `europe-west4`); "
                    "the URL is built as "
                    "`https://{region}-aiplatform.googleapis.com`",
                "default_value": DefaultRegion,
            },
            "gcp_sa_json_path": <DataProviderOptionInfo>{
                "display_name": "Service-Account JSON Path",
                "short_desc": "Path to a GCP service-account JSON key file",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Path to a GCP service-account JSON key file; when "
                    "omitted, Application Default Credentials resolution runs "
                    "(`GOOGLE_APPLICATION_CREDENTIALS` env var, "
                    "`~/.config/gcloud/application_default_credentials.json`, "
                    "or the GCE/GKE/Cloud-Run metadata server)",
            },
            "gcp_scopes": <DataProviderOptionInfo>{
                "display_name": "GCP OAuth2 Scopes",
                "short_desc": "OAuth2 scopes to request",
                "type": AbstractDataProviderType::get(SoftAutoListOrNothingType),
                "desc": "OAuth2 scopes to request; default is "
                    "`https://www.googleapis.com/auth/cloud-platform`",
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "Pre-built RestClientIo to use",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo"), NOTHING, {
                        DTT_ClientOnly: True,
                    }),
                "desc": "Pre-built RestClientIo; when provided, the GCP "
                    "options are ignored",
            },
        };
    }

    private {
        *RestClientIo::RestClientIo rest;
        string project_id;
        string region = DefaultRegion;
        *string sa_json_path;
        list<string> scopes;
    }

    constructor(RestClientIo::RestClientIo rest, string project_id,
            *string region, *string sa_json_path, *list<string> scopes) {
        self.rest = rest;
        self.project_id = project_id;
        if (region.val()) {
            self.region = region;
        }
        self.sa_json_path = sa_json_path;
        self.scopes = scopes ?? DefaultScopes;
    }

    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (!copts.gcp_project_id.val()) {
            throw "CONSTRUCTOR-ERROR",
                "VertexAiDataProvider requires 'gcp_project_id'";
        }
        project_id = copts.gcp_project_id;
        region = copts.gcp_region ?? DefaultRegion;
        sa_json_path = copts.gcp_sa_json_path;
        list<auto> raw_scopes = copts.gcp_scopes ?? DefaultScopes;
        scopes = ();
        foreach auto s in (raw_scopes) {
            push scopes, s;
        }
        if (copts.restclient) {
            rest = copts.restclient;
        } else {
            string url = sprintf("https://%s-aiplatform.googleapis.com", region);
            rest = new RestClientIo::RestClientIo({"url": url, "data": "json"});
        }
    }

    *RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    string getProjectId() { return project_id; }
    string getRegion()    { return region; }

    setLogger(*LoggerInterface logger) {
        LoggerWrapper::setLogger(logger);
    }

    #! Returns the Authorization header with a fresh GCP access token
    private hash<auto> getAuthHeader() {
        string token = GcpAuth::getAccessToken(sa_json_path, scopes);
        return {"Authorization": "Bearer " + token};
    }

    #! Returns the Vertex AI path for the given publisher model + action suffix
    /** @param publisher e.g. \c "google", \c "anthropic", \c "meta"
        @param model the model name (e.g. \c "gemini-2.0-flash",
            \c "text-embedding-005")
        @param suffix the action suffix (e.g. \c "generateContent",
            \c "predict")
    */
    private string getVertexPath(string publisher, string model, string suffix) {
        return sprintf(
            "/v1/projects/%s/locations/%s/publishers/%s/models/%s:%s",
            encode_url(project_id, True),
            encode_url(region, True),
            encode_url(publisher, True),
            encode_url(model, True),
            suffix);
    }
}
}
