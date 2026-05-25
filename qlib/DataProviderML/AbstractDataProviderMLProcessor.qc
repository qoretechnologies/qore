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
    \c DataProvider::AbstractDataProcessor::convertShape(); bulk outputs are
    converted back to the negotiated input shape when possible so downstream
    shape-aware processors can continue in columnar bulk mode.

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

    private {
        #! Preferred shape for pending flush output
        DataProvider::DataProviderShape pending_output_shape = DataProvider::DataProviderShape::RowRecords;

        #! True if the pending output shape has been set for buffered records
        bool pending_output_shape_set = False;
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

    #! Returns the task declared by a registry manifest execution plan
    /** @param manifest model registry version manifest

        @return the normalized task name, or @ref NOTHING when no task is declared

        @since DataProviderML 1.4
    */
    static *string getRegistryExecutionTask(
            hash<QoreModelRegistry::ModelVersionInfo> manifest) {
        if (manifest.task.val()) {
            return manifest.task;
        }
        if (!manifest.execution) {
            return NOTHING;
        }
        if (manifest.execution.task.val()) {
            return manifest.execution.task;
        }
        if (manifest.execution.plan) {
            if (manifest.execution.plan.task.val()) {
                return string(manifest.execution.plan.task);
            }
            if (manifest.execution.plan.type.val()) {
                string type = string(manifest.execution.plan.type).lwr();
                type =~ s/^onnx[\/:]//;
                return type;
            }
        }
        return NOTHING;
    }

    #! Returns the artifact name declared by a registry execution plan
    /** @param manifest model registry version manifest

        @return the plan artifact name, or @ref NOTHING when none is declared

        @since DataProviderML 1.4
    */
    static *string getRegistryExecutionPlanArtifactName(
            hash<QoreModelRegistry::ModelVersionInfo> manifest) {
        if (manifest.execution && manifest.execution.plan) {
            if (manifest.execution.plan.artifact_name.val()) {
                return string(manifest.execution.plan.artifact_name);
            }
            if (manifest.execution.plan.model_artifact_name.val()) {
                return string(manifest.execution.plan.model_artifact_name);
            }
        }
        return NOTHING;
    }

    #! Returns output tensor names declared by a registry execution plan
    /** @param manifest model registry version manifest
        @param plan_keys task-specific output-name keys to consult in addition
            to generic \c output_names and \c output_name keys

        @return selected output tensor names, or @ref NOTHING when no safe
            selection is declared

        @since DataProviderML 1.4
    */
    static *list<string> getRegistryExecutionPlanOutputNames(
            hash<QoreModelRegistry::ModelVersionInfo> manifest,
            list<string> plan_keys = ()) {
        if (!manifest.execution) {
            return NOTHING;
        }

        if (manifest.execution.plan) {
            hash<auto> plan = manifest.execution.plan;
            list<string> candidate_keys = ("output_names", "output_name") + plan_keys;
            *list<string> names = getExecutionPlanOutputNamesValue(plan, candidate_keys);
            if (names) {
                return names;
            }

            foreach string section in (("output", "postprocess")) {
                if (!plan.hasKey(section) || plan{section}.typeCode() != NT_HASH) {
                    continue;
                }
                hash<auto> subplan = plan{section};
                names = getExecutionPlanOutputNamesValue(
                    subplan, ("names", "name", "output_names", "output_name") + plan_keys);
                if (names) {
                    return names;
                }
            }

            names = getSingleOutputSchemaName(plan.output_schema);
            if (names) {
                return names;
            }
        }

        return getSingleOutputSchemaName(manifest.execution.output_schema);
    }

    #! Verifies a registry execution plan task for a task-aware processor
    /** @param manifest model registry version manifest
        @param expected_task task required by the processor
        @param processor_name processor action name for error messages
        @param required if @ref True, missing or raw task metadata is rejected

        @throw CONSTRUCTOR-ERROR if the manifest task is incompatible

        @since DataProviderML 1.4
    */
    static validateRegistryExecutionTask(hash<QoreModelRegistry::ModelVersionInfo> manifest,
            string expected_task, string processor_name, bool required) {
        *string task = getRegistryExecutionTask(manifest);
        if (task.val() && task != "raw" && task != expected_task) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("%s requires a registry execution plan with task %y, but "
                    "model %y version %y declares task %y",
                    processor_name, expected_task, manifest.name, manifest.version_id, task);
        }
        if (required && (!task.val() || task == "raw")) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("%s inferred ONNX mode from registry_model_name, but model %y "
                    "version %y does not declare an %s execution plan; register the "
                    "package with task: %y or provide the processor mode explicitly",
                    processor_name, manifest.name, manifest.version_id, expected_task,
                    expected_task);
        }
    }

    #! Reads output names from a hash using candidate keys
    private static *list<string> getExecutionPlanOutputNamesValue(hash<auto> data,
            list<string> candidate_keys) {
        foreach string key in (candidate_keys) {
            if (!data.hasKey(key)) {
                continue;
            }
            list<string> names = DataProvider::AbstractDataProvider::parseFieldList(data{key});
            if (names) {
                return names;
            }
        }
        return NOTHING;
    }

    #! Returns a single output name from schema metadata when it is unambiguous
    private static *list<string> getSingleOutputSchemaName(*auto output_schema) {
        if (!exists output_schema || output_schema.typeCode() != NT_HASH) {
            return NOTHING;
        }
        if (!output_schema.hasKey("tensors") || output_schema.tensors.typeCode() != NT_LIST
                || output_schema.tensors.lsize() != 1) {
            return NOTHING;
        }
        auto tensor = output_schema.tensors[0];
        if (tensor.typeCode() != NT_HASH || !tensor.name.val()) {
            return NOTHING;
        }
        return (string(tensor.name),);
    }

    #! Records the shape of accumulated input for shape-preserving flushes
    private updatePendingOutputShape(DataProvider::DataProviderShape shape) {
        if (!pending_output_shape_set) {
            pending_output_shape = cast<enum<DataProvider::DataProviderShape>>(shape);
            pending_output_shape_set = True;
            return;
        }
        if (pending_output_shape != shape) {
            pending_output_shape = cast<enum<DataProvider::DataProviderShape>>(
                DataProvider::DataProviderShape::RowRecords);
        }
    }

    #! Clears pending flush shape state
    private clearPendingOutputShape() {
        pending_output_shape = cast<enum<DataProvider::DataProviderShape>>(
            DataProvider::DataProviderShape::RowRecords);
        pending_output_shape_set = False;
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
            DataProvider::DataProviderShape::DataFrameBlock,
            DataProvider::DataProviderShape::BufferColumns,
            DataProvider::DataProviderShape::HashOfLists,
            DataProvider::DataProviderShape::RowRecords,
        );
    }

    #! Converts hash-of-lists bulk output to the requested shape unless the output is already shaped
    /** @param data optional hash-of-lists processor output
        @param shape requested output shape
        @param already_shaped if @ref True, \a data is already in its final shape and is returned unchanged

        @return the converted bulk output, the unchanged value, or @ref NOTHING if \a data is @ref NOTHING

        @since DataProviderML 1.3
    */
    static *auto convertBulkOutputShape(*auto data, DataProvider::DataProviderShape shape, bool already_shaped = False) {
        if (!exists data) {
            return;
        }
        if (already_shaped || shape == DataProvider::DataProviderShape::HashOfLists
                || shape == DataProvider::DataProviderShape::RowRecords || data.typeCode() != NT_HASH) {
            return data;
        }
        return DataProvider::AbstractDataProcessor::convertHashListResultShape(cast<hash<auto>>(data),
            cast<enum<DataProvider::DataProviderShape>>(shape));
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
        updatePendingOutputShape(shape);
        if (shape == DataProvider::DataProviderShape::RowRecords) {
            return processRecordImpl(records);
        }

        list<auto> rows = DataProvider::AbstractDataProcessor::convertShape(
            records, shape, DataProvider::DataProviderShape::RowRecords);
        *list<auto> results = processRows(rows);
        if (!results) {
            return;
        }
        hash<auto> bulk = DataProvider::AbstractDataProcessor::recordsToHashList(results);
        return AbstractDataProviderMLProcessor::convertBulkOutputShape(bulk, shape);
    }

    #! Flushes pending records in the negotiated pipeline shape
    private flushImpl(code enqueue) {
        *list<auto> results = flushRecordsImpl();
        DataProvider::DataProviderShape output_shape = pending_output_shape;
        clearPendingOutputShape();
        DataProvider::AbstractDataProcessor::emitRecordListShape(results, output_shape, enqueue);
    }

    #! Default no-op flush implementation for stateless processors
    private *list<auto> flushRecordsImpl() {
        clearPendingOutputShape();
        return;
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
