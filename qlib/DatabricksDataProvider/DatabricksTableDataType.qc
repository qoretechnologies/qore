# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksDataProvider module definition

/*  DatabricksTableDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Data type for Databricks Unity Catalog table records
public class DatabricksTableDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Table name",
                "desc": "The name of the table",
                "required": True,
            },
            "catalog_name": {
                "type": StringOrNothingType,
                "display_name": "Catalog Name",
                "short_desc": "Parent catalog name",
                "desc": "The name of the parent catalog that contains this table",
            },
            "schema_name": {
                "type": StringOrNothingType,
                "display_name": "Schema Name",
                "short_desc": "Parent schema name",
                "desc": "The name of the parent schema that contains this table",
            },
            "full_name": {
                "type": StringOrNothingType,
                "display_name": "Full Name",
                "short_desc": "Fully qualified table name",
                "desc": "The fully qualified name of the table in the format "
                    "`catalog_name.schema_name.table_name`",
            },
            "table_type": {
                "type": StringOrNothingType,
                "display_name": "Table Type",
                "short_desc": "Type of the table",
                "desc": "The type of the table",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "MANAGED",
                        "display_name": "Managed",
                        "desc": "A managed table where Databricks manages both data and metadata",
                    },
                    <AllowedValueInfo>{
                        "value": "EXTERNAL",
                        "display_name": "External",
                        "desc": "An external table where data is stored outside Databricks",
                    },
                    <AllowedValueInfo>{
                        "value": "VIEW",
                        "display_name": "View",
                        "desc": "A virtual table defined by a SQL query",
                    },
                    <AllowedValueInfo>{
                        "value": "MATERIALIZED_VIEW",
                        "display_name": "Materialized View",
                        "desc": "A precomputed view that stores results physically",
                    },
                    <AllowedValueInfo>{
                        "value": "STREAMING_TABLE",
                        "display_name": "Streaming Table",
                        "desc": "A table that processes streaming data incrementally",
                    },
                ),
            },
            "data_source_format": {
                "type": StringOrNothingType,
                "display_name": "Data Source Format",
                "short_desc": "Storage format of the table data",
                "desc": "The storage format of the table data (e.g., `DELTA`, `PARQUET`, `CSV`, "
                    "`JSON`)",
            },
            "owner": {
                "type": StringOrNothingType,
                "display_name": "Owner",
                "short_desc": "Table owner",
                "desc": "The owner of the table",
            },
            "comment": {
                "type": StringOrNothingType,
                "display_name": "Comment",
                "short_desc": "Table description",
                "desc": "A user-provided description or comment for the table",
            },
            "columns": {
                "type": AutoListOrNothingType,
                "display_name": "Columns",
                "short_desc": "Column definitions for the table",
                "desc": "The list of column definitions for the table, including name, type, and "
                    "other metadata",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace DatabricksDataProvider
