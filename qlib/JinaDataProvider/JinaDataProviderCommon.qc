# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JinaDataProviderCommon class definition

/** JinaDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace JinaDataProvider {
public class JinaDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        const DefaultJinaUrl = "https://api.jina.ai";
        const DefaultJinaApiVersion = "v1";

        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "Jina AI API Key",
                "short_desc": "Jina AI API key for Bearer authentication",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Jina AI API key. Required unless `restclient` supplies authentication. "
                    "Get a key from the [Jina AI dashboard](https://jina.ai/api-dashboard/key-manager).",
                "sensitive": True,
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "Server URL",
                "short_desc": "The URL of the Jina AI REST API",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The URL of the Jina AI REST API.",
                "default_value": DefaultJinaUrl,
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
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "The options to use when creating a REST client",
                "type": AbstractDataProviderType::get(AutoHashType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Advanced transport options for the REST client.",
            },
            "api": <DataProviderOptionInfo>{
                "display_name": "API Version",
                "short_desc": "Jina API version segment",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The API version segment used for `/v1/*` routing.",
                "default_value": DefaultJinaApiVersion,
                "allowed_values": (<AllowedValueInfo>{"value": "v1", "display_name": "V1"},),
            },
        };

        #! Legacy retry constants retained for compatibility; actions propagate errors without retrying
        const RetrySet = {
            "SOCKET-SSL-ERROR": True,
            "SOCKET-CLOSED": True,
        };

        #! Legacy retry limit; actions do not retry automatically
        const MaxIoRetries = 5;
    }

    private {
        *RestClientIo::RestClientIo rest;
    }

    #! Copies an authenticated client without changing its connection path
    /** @param rest the transport to copy */
    constructor(RestClientIo::RestClientIo rest) {
        self.rest = cast<RestClientIo::RestClientIo>(rest.copyWithUrl(rest.getUrl().toString()));
        self.rest.clearConnectionPath();
    }

    #! Creates an authenticated provider transport
    /** @param options the ConstructorOptions values
        @throw CONSTRUCTOR-ERROR an option is invalid or authentication is missing
    */
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = cast<RestClientIo::RestClientIo>(
                copts.restclient.copyWithUrl(copts.restclient.getUrl().toString()));
        }
        if (!rest) {
            if (!copts.apikey.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "missing required 'apikey' argument for Jina AI API communication";
            }
            hash<auto> opts = (copts.restclient_options ?? {}) + {
                "url": copts.url,
                "api": copts.api,
                "apikey": copts.apikey,
            };
            rest = new JinaRestClient::JinaRestClientIo(opts);
        }
        rest.clearConnectionPath();
    }

    #! Returns a schema action using this provider's authenticated transport
    /** @param action the stable action name
        @return the action provider, including Jina model dropdown support
    */
    private JinaSchemaActionDataProvider getSchemaProvider(string action) {
        RestSchemaActionSet aset = JinaSchema::getActionSet();
        return new JinaSchemaActionDataProvider(aset, aset.getOperationEx(action), rest);
    }

    #! Returns the reference lists supported by an existing embedding or reranking provider
    /** @return the reference list names */
    *hash<string, bool> getSupportedReferenceData() {
        if (JinaSchema::getActionSet().getOperation(getName())) {
            return getSchemaProvider(getName()).getSupportedReferenceData();
        }
    }

    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> opts) {
        return getSchemaProvider(getName()).getReferenceData(type, opts);
    }
}

#! Schema action provider with model IDs suitable for Jina inference endpoints
/** The model catalog returns IDs prefixed with <tt>jina-ai/</tt>, while inference requests use short IDs.
    Dropdowns also intersect the live catalog with the models declared by the selected operation.
*/
public class JinaSchemaActionDataProvider inherits RestSchemaActionDataProvider {
    #! Creates a schema action using an authenticated Jina transport
    /** @param action_set the immutable application action set
        @param op the selected schema operation
        @param rest the authenticated REST client
    */
    constructor(RestSchemaActionSet action_set, RestSchemaActionOperation op,
            RestClient::AbstractRestClient rest) : RestSchemaActionDataProvider(action_set, op, rest) {
    }

    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> opts) {
        *list<hash<AllowedValueInfo>> values = RestSchemaActionDataProvider::getReferenceDataImpl(type, opts);
        if (!(type =~ /-models$/)) {
            return values;
        }
        *AbstractDataField model = getRequestType().getFields().model;
        *list<hash<AllowedValueInfo>> supported = model ? model.getAllowedValues() : NOTHING;
        hash<string, bool> model_names = map {$1.value: True}, supported ?? ();
        list<hash<AllowedValueInfo>> rv = ();
        foreach hash<AllowedValueInfo> value in (values ?? ()) {
            string id = value.value;
            id =~ s/^jina-ai\///;
            if (model_names && !model_names{id}) {
                continue;
            }
            value.value = id;
            rv += value;
        }
        return rv;
    }
}
}
