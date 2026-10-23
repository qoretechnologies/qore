# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AzureOpenAiDataProvider root class

/** AzureOpenAiDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Azure OpenAI root data provider
public class AzureOpenAiDataProvider inherits AzureOpenAiDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "azure-openai",
            "type": "AzureOpenAiDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };
    }

    constructor(RestClientIo::RestClientIo rest) : AzureOpenAiDataProviderCommon(rest) {
    }

    constructor(*hash<auto> options) : AzureOpenAiDataProviderCommon(options) {
    }

    string getName() {
        return ProviderInfo.name;
    }

    private *list<string> getChildProviderNamesImpl() {
        list<string> names = ();
        foreach RestSchemaActionSet actions in (AzureOpenAiSchema::getActionSets()) {
            names += actions.getActionNames();
        }
        return names;
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *RestSchemaActionSet actions = AzureOpenAiSchema::findActionSet(name);
        if (!actions) {
            return;
        }
        return new AzureOpenAiSchemaActionDataProvider(actions, actions.getOperationEx(name), rest);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Binds the Foundry project prefix into this action's schema namespace without changing the shared schema
class AzureOpenAiSchemaActionDataProvider inherits RestSchemaActionDataProvider {
    private {
        #! Complete schema root, immutable after construction
        string api_root;
    }

    constructor(RestSchemaActionSet actions, RestSchemaActionOperation op, AbstractRestClient client)
            : RestSchemaActionDataProvider(actions, op, client) {
        api_root = RestClient::rest_request_path(<RestClient::RestRequestTarget>{
            "path": actions.getSchema().getBasePath()}, client.getConnectionPath());
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return RestSchemaActionDataProvider::doRequestImpl(req, request_options + {"base_path": api_root});
    }
}

}
