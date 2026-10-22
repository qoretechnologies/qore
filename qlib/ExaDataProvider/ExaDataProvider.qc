# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ExaDataProvider class definitions

/** ExaDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace ExaDataProvider {
#! Common base for Exa data providers
public class ExaDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        const DefaultExaUrl = "https://api.exa.ai";

        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "Exa API Key",
                "short_desc": "Exa API key (x-api-key header)",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Exa API key — sent as the `x-api-key` header.",
                "required": True,
                "sensitive": True,
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "Server URL",
                "short_desc": "Exa server URL",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "URL of the Exa REST API.",
                "default_value": DefaultExaUrl,
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
    }

    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        } else {
            if (!copts.apikey.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "missing required 'apikey' argument for Exa API communication";
            }
            rest = new ExaRestClient::ExaRestClientIo({
                "url": copts.url,
                "apikey": copts.apikey,
            });
        }
    }
}

#! The root of the Exa application
/** Every action is generated from the vendored Exa Public API description, so the root is the action set
    itself; see @ref ExaDataProvider::ExaManifest "ExaManifest" for what is exported and why.

    @since %ExaDataProvider 1.0
*/
public class ExaDataProvider inherits RestSchemaActions::RestSchemaActionSetDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "exa",
            "type": "ExaDataProvider",
            "supports_children": True,
            "constructor_options": ExaDataProviderCommon::ConstructorOptions,
            "children_can_support_apis": True,
        };
    }

    #! Creates the object from a REST client
    constructor(*RestClient::AbstractRestClient rest)
            : RestSchemaActionSetDataProvider(ExaSchema::getActionSet(), rest) {
    }

    #! Creates the object from options
    constructor(*hash<auto> options)
            : RestSchemaActionSetDataProvider(ExaSchema::getActionSet(),
                ExaDataProvider::getClient(options)) {
    }

    string getName() { return ProviderInfo.name; }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns a client built from constructor options
    private static ExaRestClient::ExaRestClientIo getClient(*hash<auto> options) {
        if (options.restclient) {
            return options.restclient;
        }
        return new ExaRestClient::ExaRestClientIo(options - "restclient");
    }
}

public class ExaDataProviderFactory inherits AbstractDataProviderFactory {
    private {
        static Class cls = new Class("ExaDataProvider");
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "exa",
            "desc": "Exa neural-search data provider factory",
            "children_can_support_apis": True,
        };
    }
    private hash<DataProviderFactoryInfo> getInfoImpl() { return FactoryInfo; }
    private hash<DataProviderInfo> getProviderInfoImpl() { return ExaDataProvider::ProviderInfo; }
    private Class getClassImpl() { return cls; }
}









}
