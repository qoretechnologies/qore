# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksCreateSecretScopeDataProvider class definition

/*  DatabricksCreateSecretScopeDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DatabricksDataProvider module
public namespace DatabricksDataProvider {

#! Request data type for the create-secret-scope action
public class DatabricksCreateSecretScopeRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "scope": {
                "type": StringType,
                "display_name": "Scope Name",
                "short_desc": "The name of the secret scope",
                "desc": "The name of the secret scope to create",
                "required": True,
            },
            "scope_backend_type": {
                "type": StringOrNothingType,
                "display_name": "Backend Type",
                "short_desc": "The backend type for the scope",
                "desc": "The backend type for storing secrets in this scope",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "DATABRICKS", "desc": "Databricks-managed secret storage"},
                    <AllowedValueInfo>{"value": "AZURE_KEYVAULT", "desc": "Azure Key Vault-backed secret storage"},
                ),
            },
            "initial_manage_principal": {
                "type": StringOrNothingType,
                "display_name": "Initial Manage Principal",
                "short_desc": "Initial principal with manage permission",
                "desc": "The principal that has initial manage permission on the secret scope; "
                    "defaults to the requesting user if not specified",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Databricks create secret scope action data provider
public class DatabricksCreateSecretScopeDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-secret-scope",
            "desc": "Create a new secret scope in Databricks",
            "type": "DatabricksCreateSecretScopeDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        const RequestType = new DatabricksCreateSecretScopeRequestDataType();

        #! Response type
        const ResponseType = AbstractDataProviderType::get(AutoHashType);
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest) : DatabricksDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return "create-secret-scope";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Create a new secret scope in Databricks";
    }

    #! Makes a request and returns the response
    /** @param req the request body
        @param request_options the request options

        @return an empty hash on success
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doPost("api/2.0/secrets/scopes/create", req);
        return {};
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DatabricksDataProvider
