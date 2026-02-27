# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksSecretsDataProvider class definition

/*  DatabricksSecretsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Databricks secrets collection data provider
public class DatabricksSecretsDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "secrets",
            "desc": "Databricks secrets data provider; list secret scopes and secrets within a scope",
            "type": "DatabricksSecretsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "scope": <DataProviderOptionInfo>{
                "display_name": "Scope",
                "short_desc": "Secret scope name",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The secret scope name; if provided, lists secrets in the scope; otherwise "
                    "lists all secret scopes",
            },
        };
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest)
            : DatabricksDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "secrets";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Databricks secrets data provider; list secret scopes and secrets within a scope";
    }

    #! Returns the record type
    /** @note Returns \c NOTHING because the record type varies based on whether scopes or secrets
        are listed
    */
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return;
    }

    #! Searches for secrets or secret scopes
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        list<auto> records;

        if (search_options.scope) {
            # List secrets within a scope
            hash<auto> response = doGet("api/2.0/secrets/list",
                {"scope": search_options.scope});
            records = response.secrets ?? ();
        } else {
            # List all secret scopes
            hash<auto> response = doGet("api/2.0/secrets/scopes/list");
            records = response.scopes ?? ();
        }

        return new DefaultRecordIterator(new ListHashIterator(records),
            where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DatabricksDataProvider
