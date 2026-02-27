# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksUnityCatalogDataProvider class definition

/*  DatabricksUnityCatalogDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Databricks Unity Catalog collection data provider
public class DatabricksUnityCatalogDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "unity-catalog",
            "desc": "Databricks Unity Catalog data provider; list catalogs, schemas, and tables",
            "type": "DatabricksUnityCatalogDataProvider",
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
            "resource_type": <DataProviderOptionInfo>{
                "display_name": "Resource Type",
                "short_desc": "Type of Unity Catalog resource to list",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The type of Unity Catalog resource to list (required)",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "catalogs",
                        "desc": "List Unity Catalog catalogs",
                    },
                    <AllowedValueInfo>{
                        "value": "schemas",
                        "desc": "List schemas within a catalog (requires `catalog_name`)",
                    },
                    <AllowedValueInfo>{
                        "value": "tables",
                        "desc": "List tables within a schema (requires `catalog_name` and `schema_name`)",
                    },
                ),
            },
            "catalog_name": <DataProviderOptionInfo>{
                "display_name": "Catalog Name",
                "short_desc": "Catalog name for schemas and tables",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The catalog name; required when listing schemas or tables",
            },
            "schema_name": <DataProviderOptionInfo>{
                "display_name": "Schema Name",
                "short_desc": "Schema name for tables",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The schema name; required when listing tables",
            },
        };
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest)
            : DatabricksDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "unity-catalog";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Databricks Unity Catalog data provider; list catalogs, schemas, and tables";
    }

    #! Returns the record type
    /** @note Returns \c NOTHING because the record type varies based on `resource_type`
    */
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return;
    }

    #! Searches for Unity Catalog resources
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        list<auto> records;

        switch (search_options.resource_type) {
            case "catalogs": {
                hash<auto> response = doGet("api/2.1/unity-catalog/catalogs");
                records = response.catalogs ?? ();
                break;
            }
            case "schemas": {
                if (!search_options.catalog_name) {
                    throw "DATABRICKS-SEARCH-ERROR",
                        "catalog_name is required when listing schemas";
                }
                hash<auto> response = doGet("api/2.1/unity-catalog/schemas",
                    {"catalog_name": search_options.catalog_name});
                records = response.schemas ?? ();
                break;
            }
            case "tables": {
                if (!search_options.catalog_name) {
                    throw "DATABRICKS-SEARCH-ERROR",
                        "catalog_name is required when listing tables";
                }
                if (!search_options.schema_name) {
                    throw "DATABRICKS-SEARCH-ERROR",
                        "schema_name is required when listing tables";
                }
                hash<auto> response = doGet("api/2.1/unity-catalog/tables", {
                    "catalog_name": search_options.catalog_name,
                    "schema_name": search_options.schema_name,
                });
                records = response.tables ?? ();
                break;
            }
            default:
                throw "DATABRICKS-SEARCH-ERROR",
                    sprintf("unknown resource_type %y; expected: catalogs, schemas, or tables",
                        search_options.resource_type);
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
