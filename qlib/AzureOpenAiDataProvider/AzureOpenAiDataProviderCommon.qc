# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AzureOpenAiDataProvider common base class

/** AzureOpenAiDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the AzureOpenAiDataProvider module
public namespace AzureOpenAiDataProvider {
#! Common base for Azure OpenAI data providers
/** Resolves the Azure auth header (api-key, entra-client-credentials, or
    entra-managed-identity) on every call so cached Entra tokens are
    refreshed transparently by @ref azureauthintro "AzureAuth".
*/
public class AzureOpenAiDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        const DefaultApiVersion = "2024-10-21";

        const ConstructorOptions = {
            "url": <DataProviderOptionInfo>{
                "display_name": "Resource Endpoint",
                "short_desc": "Azure OpenAI resource endpoint",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Azure OpenAI resource endpoint, e.g. "
                    "`https://my-resource.openai.azure.com`",
            },
            "azure_api_version": <DataProviderOptionInfo>{
                "display_name": "API Version",
                "short_desc": "Azure OpenAI REST API version",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Azure OpenAI REST API version (e.g. `2024-10-21`)",
                "default_value": DefaultApiVersion,
            },
            "azure_auth_mode": <DataProviderOptionInfo>{
                "display_name": "Auth Mode",
                "short_desc": "Authentication mode",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Authentication mode: "
                    "`api_key`, `entra_client_credentials`, or "
                    "`entra_managed_identity`",
                "default_value": "api_key",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "api_key",
                        "display_name": "API Key"},
                    <AllowedValueInfo>{"value": "entra_client_credentials",
                        "display_name": "Entra Client Credentials"},
                    <AllowedValueInfo>{"value": "entra_managed_identity",
                        "display_name": "Entra Managed Identity"},
                ),
            },
            "api_key": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "Azure OpenAI resource api-key",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Azure OpenAI resource api-key (when "
                    "`azure_auth_mode` = `api_key`)",
                "sensitive": True,
            },
            "azure_tenant_id": <DataProviderOptionInfo>{
                "display_name": "Entra Tenant ID",
                "short_desc": "Entra tenant ID",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Entra tenant ID (GUID or domain). Required for "
                    "`entra_client_credentials`",
            },
            "azure_client_id": <DataProviderOptionInfo>{
                "display_name": "Entra Client ID",
                "short_desc": "Entra app client ID / managed-identity client ID",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Entra app client ID for client_credentials, or "
                    "optional user-assigned managed-identity client ID",
            },
            "azure_client_secret": <DataProviderOptionInfo>{
                "display_name": "Entra Client Secret",
                "short_desc": "Entra app client secret",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Entra app client secret. Required for "
                    "`entra_client_credentials`",
                "sensitive": True,
            },
            "azure_entra_scope": <DataProviderOptionInfo>{
                "display_name": "Entra Scope",
                "short_desc": "OAuth2 scope",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "OAuth2 scope; default "
                    "`https://cognitiveservices.azure.com/.default`",
                "default_value": "https://cognitiveservices.azure.com/.default",
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "Pre-built RestClientIo to use",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo"), NOTHING, {
                        DTT_ClientOnly: True,
                    }),
                "desc": "Pre-built RestClientIo; when provided, the auth "
                    "options are ignored",
            },
        };
    }

    private {
        *RestClientIo::RestClientIo rest;
        string base_url;
        string api_version = DefaultApiVersion;
        string auth_mode = "api_key";
        *string api_key;
        *string tenant_id;
        *string client_id;
        *string client_secret;
        string entra_scope = "https://cognitiveservices.azure.com/.default";
    }

    #! Creates the object from a pre-built RestClientIo
    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
        base_url = rest.getURL();
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
            base_url = rest.getURL();
            return;
        }
        if (!copts.url.val()) {
            throw "CONSTRUCTOR-ERROR",
                "AzureOpenAiDataProvider requires 'url' when no 'restclient' is supplied";
        }
        base_url = copts.url;
        api_version = copts.azure_api_version ?? DefaultApiVersion;
        auth_mode = copts.azure_auth_mode ?? "api_key";
        api_key = copts.api_key;
        tenant_id = copts.azure_tenant_id;
        client_id = copts.azure_client_id;
        client_secret = copts.azure_client_secret;
        entra_scope = copts.azure_entra_scope
            ?? "https://cognitiveservices.azure.com/.default";

        if (auth_mode == "api_key" && !api_key.val()) {
            throw "CONSTRUCTOR-ERROR",
                "AzureOpenAiDataProvider auth_mode='api_key' requires 'api_key'";
        }
        if (auth_mode == "entra_client_credentials"
                && (!tenant_id.val() || !client_id.val() || !client_secret.val())) {
            throw "CONSTRUCTOR-ERROR",
                "AzureOpenAiDataProvider auth_mode='entra_client_credentials' "
                "requires azure_tenant_id, azure_client_id, azure_client_secret";
        }

        rest = new RestClientIo::RestClientIo({"url": base_url, "data": "json"});
    }

    #! Returns the REST client object
    *RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    setLogger(*LoggerInterface logger) {
        LoggerWrapper::setLogger(logger);
    }

    #! Returns the auth header for the configured mode (refreshes Entra tokens automatically)
    private hash<auto> getAuthHeader() {
        switch (auth_mode) {
            case "api_key":
                return {"api-key": api_key};
            case "entra_client_credentials": {
                string token = AzureAuth::getEntraTokenClientCredentials(
                    tenant_id, client_id, client_secret, entra_scope);
                return {"Authorization": "Bearer " + token};
            }
            case "entra_managed_identity": {
                string resource = entra_scope;
                resource =~ s/\/\.default$//;
                string token = AzureAuth::getEntraTokenManagedIdentity(
                    resource, client_id);
                return {"Authorization": "Bearer " + token};
            }
            default:
                throw "AZURE-OPENAI-AUTH-ERROR",
                    sprintf("unknown azure_auth_mode %y", auth_mode);
        }
    }

    #! Returns the deployment URL path for the given deployment + suffix
    private string getDeploymentPath(string deployment, string suffix) {
        return sprintf("/openai/deployments/%s/%s?api-version=%s",
            encode_url(deployment, True),
            suffix,
            encode_url(api_version, True));
    }

    #! Returns the configured API version
    string getApiVersion() {
        return api_version;
    }
}
}
