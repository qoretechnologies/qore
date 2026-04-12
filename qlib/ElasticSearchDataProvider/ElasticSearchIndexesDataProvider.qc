# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElasticSearchDataProvider module definition

/** ElasticSearchIndexesDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ElasticSearchDataProvider module
public namespace ElasticSearchDataProvider {
#! The ElasticSearch indexes root for providing record-based data providers for each index
public class ElasticSearchIndexesDataProvider inherits ElasticSearchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "indexes",
            "desc": "ElasticSearch indexes data provider; provdes table-like access to ElasticSearch indices",
            "type": "ElasticSearchIndexesDataProvider",
            "constructor_options": ElasticSearchDataProvider::ConstructorOptions,
            "supports_children": True,
            "children_can_support_records": True,
            "children_identical": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", ElasticSearchDataProvider::ConstructorOptions, options);
        rest = ElasticSearchDataProvider::getRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(RestClient::AbstractRestClient rest) : ElasticSearchDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("ElasticSearch data %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map <DataProviderSummaryInfo>{
            "name": $1,
            "desc": sprintf("ElasticSearch index %y", $1),
            "type": "ElasticSearchIndexTableDataProvider",
            "supports_read": ElasticSearchIndexTableDataProvider::ProviderInfo.supports_read,
            "supports_create": ElasticSearchIndexTableDataProvider::ProviderInfo.supports_create,
            "supports_update": ElasticSearchIndexTableDataProvider::ProviderInfo.supports_update,
            "supports_upsert": ElasticSearchIndexTableDataProvider::ProviderInfo.supports_upsert,
            "supports_delete": ElasticSearchIndexTableDataProvider::ProviderInfo.supports_delete,
            "supports_native_search": ElasticSearchIndexTableDataProvider::ProviderInfo.supports_native_search,
            "supports_bulk_read": ElasticSearchIndexTableDataProvider::ProviderInfo.supports_bulk_read,
            "supports_bulk_create": ElasticSearchIndexTableDataProvider::ProviderInfo.supports_bulk_create,
            "supports_bulk_upsert": ElasticSearchIndexTableDataProvider::ProviderInfo.supports_bulk_upsert,
            "supports_request": ElasticSearchIndexTableDataProvider::ProviderInfo.supports_request,
            "supports_children": False,
            "transaction_management": ElasticSearchIndexTableDataProvider::ProviderInfo.transaction_management,
            "has_record": ElasticSearchIndexTableDataProvider::ProviderInfo.has_record,
            "children_can_support_records": False,
        }, getChildProviderNames();
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys getAllIndices();
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *hash<auto> index = getIndex(name);
        if (index) {
            return new ElasticSearchIndexTableDataProvider(rest, name, index);
        }
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns a hash of indices
    /** @return a hash of indices
    */
    private *hash<auto> getAllIndices() {
        try {
            return rest.restGet("*").body;
        } catch (hash<ExceptionInfo> ex) {
            rethrow ex.err, ex.desc, mapRestErrorInfo(ex);
        }
    }

    #! Returns a hash of the given index or @ref nothing
    /** @return a hash of the given index or @ref nothing
    */
    private *hash<auto> getIndex(string name) {
        try {
            return rest.restGet(name).body{name};
        } catch (hash<ExceptionInfo> ex) {
            if (ex.arg.typeCode() == NT_HASH && ex.arg.status_code == 404) {
                return;
            }
            rethrow ex.err, ex.desc, mapRestErrorInfo(ex);
        }
    }

    #! Map a @ref RestClient::RestResponseInfo error arg to the legacy ex arg shape
    /** Historically @ref ElasticSearchIndexesDataProvider exceptions reported REST error
        context via \c "request-uri" / \c "response-code" / \c "response-body" keys (populated
        from @ref RestClient::RestClient::get() "RestClient::get()"'s \a info reference param).
        The async @ref RestClientIo::RestClientIo "RestClientIo" transport instead throws
        @ref RestClient::REST-RESPONSE-ERROR with a @ref RestClient::RestResponseInfo hashdecl
        in \c ex.arg; translate to the legacy shape so callers do not need to change.
    */
    private static hash<auto> mapRestErrorInfo(hash<ExceptionInfo> ex) {
        if (ex.arg.typeCode() != NT_HASH) {
            return {};
        }
        return {
            "request-uri": ex.arg.request_path,
            "response-code": ex.arg.status_code,
            "response-body": ex.arg.body,
        };
    }
}
}
