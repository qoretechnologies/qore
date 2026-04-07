# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantTablesDataProvider class definition

/** QdrantTablesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the QdrantDataProvider module
public namespace QdrantDataProvider {
#! Parent data provider listing Qdrant collections as record-based child providers
/** Each child is a @ref QdrantCollectionTableDataProvider providing DPAT_FIND-based vector search
*/
public class QdrantTablesDataProvider inherits QdrantDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "tables",
            "desc": "Qdrant tables data provider; provides record-based access to Qdrant collections for "
                "vector similarity search with record streaming",
            "type": "QdrantTablesDataProvider",
            "constructor_options": QdrantDataProvider::ConstructorOptions,
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
        checkOptions("CONSTRUCTOR-ERROR", QdrantDataProvider::ConstructorOptions, options);
        rest = QdrantDataProvider::getRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(RestClientIo::RestClientIo rest) : QdrantDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Qdrant tables data provider for `%s`", rest.getSafeUrl());
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map <DataProviderSummaryInfo>{
            "name": $1,
            "desc": sprintf("Qdrant collection %y", $1),
            "type": "QdrantCollectionTableDataProvider",
            "supports_read": QdrantCollectionTableDataProvider::ProviderInfo.supports_read,
            "supports_create": QdrantCollectionTableDataProvider::ProviderInfo.supports_create,
            "supports_update": QdrantCollectionTableDataProvider::ProviderInfo.supports_update,
            "supports_upsert": QdrantCollectionTableDataProvider::ProviderInfo.supports_upsert,
            "supports_delete": QdrantCollectionTableDataProvider::ProviderInfo.supports_delete,
            "supports_native_search":
                QdrantCollectionTableDataProvider::ProviderInfo.supports_native_search,
            "supports_bulk_create": QdrantCollectionTableDataProvider::ProviderInfo.supports_bulk_create,
            "supports_bulk_upsert": QdrantCollectionTableDataProvider::ProviderInfo.supports_bulk_upsert,
            "supports_children": False,
            "has_record": QdrantCollectionTableDataProvider::ProviderInfo.has_record,
            "children_can_support_records": False,
        }, getChildProviderNames();
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        *list<auto> collections = doQdrantGet("/collections").collections;
        return map $1.name, collections;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        # verify the collection exists
        try {
            hash<auto> resp = rest.restGet(collectionPath(name));
            if (resp.status_code == 404) {
                return;
            }
        } catch (hash<ExceptionInfo> ex) {
            return;
        }
        return new QdrantCollectionTableDataProvider(rest, name);
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
