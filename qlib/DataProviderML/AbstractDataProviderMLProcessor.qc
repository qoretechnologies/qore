# -*- mode: qore; indent-tabs-mode: nil -*-
#! Common DataProviderML processor base class

/** AbstractDataProviderMLProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProviderML module
public namespace DataProviderML {
#! Base class for ML pipeline processors with DataProvider shape support
/**
    This class keeps the existing row-oriented \c processRecordImpl() contract
    used by DataProviderML processors while allowing the DataProvider pipeline
    to pass \c DataFrameBlock, \c BufferColumns, and \c HashOfLists blocks.

    Shaped input is converted to row records through
    \c DataProvider::AbstractDataProcessor::convertShape(); outputs are emitted
    as \c HashOfLists blocks so downstream shape-aware processors can continue
    in bulk mode.

    @since DataProviderML 1.3
*/
public class AbstractDataProviderMLProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Shared model registry backend choices for AI processors
        const ModelRegistryBackendTypes = (
            <AllowedValueInfo>{"value": "filesystem", "display_name": "Filesystem"},
            <AllowedValueInfo>{"value": "database", "display_name": "Database (SqlUtil)"},
            <AllowedValueInfo>{"value": "rest", "display_name": "REST API"},
        );

        #! Shared constructor options for loading tokenizer/model artifacts from QoreModelRegistry
        const ModelRegistryConstructorOptions = {
            "registry_backend_type": <DataProviderOptionInfo>{
                "display_name": "Registry Backend Type",
                "short_desc": "Storage backend type for the model registry",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The storage backend used for registry artifacts: `filesystem`, "
                    "`database`, or `rest`.",
                "allowed_values": ModelRegistryBackendTypes,
                "default_value": "filesystem",
            },
            "registry_path": <DataProviderOptionInfo>{
                "display_name": "Registry Path",
                "short_desc": "Filesystem path to the model registry",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Base directory for filesystem-backed model packages.",
            },
            "registry_datasource": <DataProviderOptionInfo>{
                "display_name": "Registry Datasource",
                "short_desc": "Database datasource for registry artifacts",
                "type": AbstractDataProviderTypeMap."any",
                "desc": "An AbstractDatasource object for database-backed registry storage.",
            },
            "registry_table_prefix": <DataProviderOptionInfo>{
                "display_name": "Registry Table Prefix",
                "short_desc": "Table prefix for database registry backend",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Prefix for database registry tables; default is `ml_`.",
            },
            "registry_rest_client": <DataProviderOptionInfo>{
                "display_name": "Registry REST Client",
                "short_desc": "REST client object for registry artifacts",
                "type": AbstractDataProviderTypeMap."any",
                "desc": "A REST client object with model registry endpoints.",
            },
            "registry_rest_base_path": <DataProviderOptionInfo>{
                "display_name": "Registry REST Base Path",
                "short_desc": "Base path for registry REST endpoints",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Base URL path for registry REST endpoints.",
            },
            "registry_tenant_id": <DataProviderOptionInfo>{
                "display_name": "Registry Tenant ID",
                "short_desc": "Tenant identifier for registry access",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Optional tenant identifier to scope registry model artifacts.",
            },
        };
    }

    #! Builds ModelRegistry constructor options from shared processor options
    /** @param copts checked processor options

        @return constructor options for @ref QoreModelRegistry::ModelRegistry

        @since DataProviderML 1.4
    */
    static hash<auto> buildModelRegistryOptions(hash<auto> copts) {
        return {
            "backend_type": copts.registry_backend_type ?? "filesystem",
            "path": copts.registry_path,
            "datasource": copts.registry_datasource,
            "table_prefix": copts.registry_table_prefix,
            "rest_client": copts.registry_rest_client,
            "rest_base_path": copts.registry_rest_base_path,
            "tenant_id": copts.registry_tenant_id ?? copts.tenant_id,
        };
    }

    #! Returns @ref True because this base handles shaped bulk input
    private bool supportsBulkApiImpl() {
        return True;
    }

    #! Returns supported input shapes from most to least preferred
    private list<DataProvider::DataProviderShape> supportedInputShapesImpl() {
        return (
            DataProvider::DataProviderShape::DataFrameBlock,
            DataProvider::DataProviderShape::BufferColumns,
            DataProvider::DataProviderShape::HashOfLists,
            DataProvider::DataProviderShape::RowRecords,
        );
    }

    #! Returns supported output shapes from most to least preferred
    private list<DataProvider::DataProviderShape> supportedOutputShapesImpl() {
        return (
            DataProvider::DataProviderShape::HashOfLists,
            DataProvider::DataProviderShape::RowRecords,
        );
    }

    #! Processes records in bulk hash-of-lists format
    private *hash<auto> processRecordsBulkImpl(hash<auto> records) {
        *auto result = processRecordsShapeImpl(records, DataProvider::DataProviderShape::HashOfLists);
        if (!exists result) {
            return;
        }
        if (result.typeCode() != NT_HASH) {
            throw "PROCESSOR-SHAPE-ERROR", sprintf("%s expected hash-of-lists output for bulk processing; got %y",
                self.className(), result.type());
        }
        return result;
    }

    #! Processes records in an explicitly negotiated pipeline shape
    private *auto processRecordsShapeImpl(auto records, DataProvider::DataProviderShape shape) {
        if (shape == DataProvider::DataProviderShape::RowRecords) {
            return processRecordImpl(records);
        }

        list<auto> rows = DataProvider::AbstractDataProcessor::convertShape(
            records, shape, DataProvider::DataProviderShape::RowRecords);
        *list<auto> results = processRows(rows);
        if (!results) {
            return;
        }
        return DataProvider::AbstractDataProcessor::recordsToHashList(results);
    }

    #! Processes a list of row records with the subclass row processor
    private *list<auto> processRows(list<auto> records) {
        *list<auto> results;
        foreach auto record in (records) {
            *auto result = processRecordImpl(record);
            if (exists result) {
                if (result.typeCode() == NT_LIST) {
                    results += result;
                } else {
                    push results, result;
                }
            }
        }
        return results;
    }
}
}
