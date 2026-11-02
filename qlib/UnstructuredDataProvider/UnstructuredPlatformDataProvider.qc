# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnstructuredDataProvider module: the Platform API subtree

/** UnstructuredPlatformDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace UnstructuredDataProvider {
#! The Unstructured Platform API subtree
/** Hosts the actions generated from the vendored Platform API schema, plus the hand-written
    @ref UnstructuredDataProvider::UnstructuredConnectorFieldsDataProvider "UnstructuredConnectorFieldsDataProvider" that answers what a connector type needs configuring.

    The Platform API is a separate service from the partition API on a separate host, but takes the same
    API key, so one connection serves both; see
    @ref UnstructuredRestClient::UnstructuredPlatformRestClientIo "UnstructuredPlatformRestClientIo".

    @since %UnstructuredDataProvider 1.0
*/
public class UnstructuredPlatformDataProvider inherits RestSchemaActions::RestSchemaActionSetDataProvider {
    public {
        #! The name of this provider, and the first segment of every platform action path
        const ProviderName = "platform";

        #! The name of the child provider holding the record-based tables
        const TablesChildName = "tables";

        #! The name of the child provider holding the event providers
        const EventsChildName = "events";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": ProviderName,
            "type": "UnstructuredPlatformDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_observers": True,
        };
    }

    #! Creates the object from a Platform API REST client
    constructor(*RestClient::AbstractRestClient rest)
            : RestSchemaActionSetDataProvider(UnstructuredPlatformSchema::getActionSet(), rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderName;
    }

    #! Returns the names of the child providers
    private *list<string> getChildProviderNamesImpl() {
        return (RestSchemaActionSetDataProvider::getChildProviderNamesImpl() ?? ())
            + (UnstructuredConnectorFieldsDataProvider::ProviderName, TablesChildName,
                EventsChildName);
    }

    #! Returns the given child provider
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        if (name == UnstructuredConnectorFieldsDataProvider::ProviderName) {
            return new UnstructuredConnectorFieldsDataProvider();
        }
        if (name == EventsChildName) {
            return new UnstructuredPlatformEventsDataProvider(
                cast<*UnstructuredRestClient::UnstructuredPlatformRestClientIo>(rest));
        }
        if (name == TablesChildName) {
            return new RestSchemaActions::RestSchemaTablesDataProvider(TablesChildName,
                UnstructuredPlatformSchema::getActionSet(),
                UnstructuredPlatformRecordTables::Tables, rest);
        }
        return RestSchemaActionSetDataProvider::getChildProviderImpl(name);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Container for the Unstructured Platform API event providers
public class UnstructuredPlatformEventsDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": UnstructuredPlatformDataProvider::EventsChildName,
            "type": "UnstructuredPlatformEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };
    }

    private {
        #! The Platform API client
        *UnstructuredRestClient::UnstructuredPlatformRestClientIo rest;
    }

    constructor(*UnstructuredRestClient::UnstructuredPlatformRestClientIo rest) {
        self.rest = rest;
    }

    string getName() {
        return UnstructuredPlatformDataProvider::EventsChildName;
    }

    private *list<string> getChildProviderNamesImpl() {
        return (UnstructuredJobWatchDataProvider::ProviderName,);
    }

    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        if (name == UnstructuredJobWatchDataProvider::ProviderName) {
            return new UnstructuredJobWatchDataProvider(rest);
        }
    }

    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return (UnstructuredJobWatchDataProvider::ProviderSummaryInfo,);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Request type for @ref UnstructuredConnectorFieldsDataProvider
public class UnstructuredConnectorFieldsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "connector_type": {
                "display_name": "Connector Type",
                "short_desc": "The connector type to describe",
                "desc": "The connector type whose configuration fields to return, as given to "
                    "`create-source` or `create-destination`.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "s3",
                "allowed_values": UnstructuredPlatformManifest::ConnectorTypeValues,
                "allowed_values_creatable": True,
            },
            "direction": {
                "display_name": "Direction",
                "short_desc": "Whether to describe the source or destination form of the type",
                "desc": "Several connector types exist in both directions and take different "
                    "settings in each - an S3 source declares `recursive`, an S3 destination does "
                    "not.  Naming the direction picks the right one.",
                "type": AbstractDataProviderTypeMap."*softstring",
                "default_value": "source",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "source", "display_name": "Source"},
                    <AllowedValueInfo>{"value": "destination", "display_name": "Destination"},
                ),
                "preselected": True,
            },
        };
    }

    constructor() : HashDataType("UnstructuredConnectorFieldsRequest") {
        addQoreFields(Fields);
    }
}

#! One configuration field of a connector type
public class UnstructuredConnectorFieldDataType inherits HashDataType {
    private {
        const Fields = {
            "name": {
                "display_name": "Field Name",
                "short_desc": "The configuration key",
                "desc": "The key to use in the connector's `config` hash.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "type": {
                "display_name": "Type",
                "short_desc": "The Qore type the field accepts",
                "desc": "The type the field accepts, as a %Qore type name.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "required": {
                "display_name": "Required",
                "short_desc": "Whether the connector cannot be created without it",
                "desc": "`True` when the connector cannot be created without this field.",
                "type": AbstractDataProviderTypeMap."bool",
            },
            "desc": {
                "display_name": "Description",
                "short_desc": "What the field is for",
                "desc": "What the field is for, as the vendor's schema describes it.",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() : HashDataType("UnstructuredConnectorField") {
        addQoreFields(Fields);
    }
}

#! Response type for @ref UnstructuredConnectorFieldsDataProvider
public class UnstructuredConnectorFieldsResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "connector_type": {
                "display_name": "Connector Type",
                "short_desc": "The connector type described",
                "desc": "The connector type these fields belong to.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "schema": {
                "display_name": "Schema Name",
                "short_desc": "The vendor schema the fields came from",
                "desc": "Name of the component schema in Unstructured's own API description that "
                    "declares these fields.\n\n"
                    "Unset for a connector type Unstructured accepts but does not describe, in "
                    "which case `fields` is empty; the connector can still be created, but its "
                    "settings have to come from Unstructured's own documentation.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "fields": {
                "display_name": "Fields",
                "short_desc": "The configuration fields the connector type accepts",
                "desc": "The keys to supply in the connector's `config` hash.",
                "type": new ListDataType("UnstructuredConnectorFieldList",
                    new UnstructuredConnectorFieldDataType(), True),
            },
        };
    }

    constructor() : HashDataType("UnstructuredConnectorFieldsResponse") {
        addQoreFields(Fields);
    }
}

#! Answers what a given connector type needs configuring
/** Unstructured types a connector's \c config as a union over one schema per connector type, so a form
    cannot show the right fields until the type is chosen and no single static option list can describe
    them.  This action reads the answer out of the vendored schema, so that a caller building a connector
    can ask what to supply before supplying it.

    It makes no HTTP request: the answer is already in the pinned document.

    @note Unstructured accepts six source connector types its own API description declares no
    configuration schema for - \c airtable, \c github, \c gitlab, \c local, \c sftp and \c vastdb.  For
    those the answer is an empty field list rather than an error: the connector can still be created, but
    its settings have to come from Unstructured's documentation.

    @since %UnstructuredDataProvider 1.0
*/
public class UnstructuredConnectorFieldsDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! The name of this provider
        const ProviderName = "connector-fields";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": ProviderName,
            "type": "UnstructuredConnectorFieldsDataProvider",
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        static UnstructuredConnectorFieldsRequestDataType RequestType =
            new UnstructuredConnectorFieldsRequestDataType();
        static UnstructuredConnectorFieldsResponseDataType ResponseType =
            new UnstructuredConnectorFieldsResponseDataType();

        #! Connector types whose schema is not named after the type
        /** Most connector configuration schemas are named for their type, but the vendor does not spell
            every one the same way, so the exceptions are listed rather than guessed at.
        */
        const SchemaNameMap = {
            "kafka-cloud": "KafkaCloud",
            "qdrant-cloud": "QdrantCloud",
            "weaviate-cloud": "Weaviate",
            "azure_ai_search": "AzureAISearch",
            "databricks_volumes": "DatabricksVolumes",
            "databricks_volume_delta_tables": "DatabricksVDT",
            "delta_table": "DeltaTable",
            "google_drive": "GoogleDrive",
            "ibm_watsonx_s3": "IBMWatsonxS3",
            "astradb": "AstraDB",
            "gcs": "GCS",
            "s3": "S3",
            "mongodb": "MongoDB",
            "neo4j": "Neo4j",
            "kdbai": "KdbAI",
            "onedrive": "OneDrive",
            "sharepoint": "SharePoint",
            "postgres": "Postgres",
        };
    }

    string getName() {
        return ProviderName;
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the configuration fields the given connector type accepts
    /** @throw CONNECTOR-TYPE-ERROR the connector type is not one Unstructured declares in the given
        direction; if it exists in the other direction, the error says so
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string type = req.connector_type;
        bool source = (req.direction ?? "source") == "source";
        OpenApi3::OpenApi3Schema schema = UnstructuredPlatformSchema::getSchema();

        # the vendor writes the input form of a config schema with an "Input" suffix; try the plain
        # name too, because a type with no writable settings only has the one
        *string found;
        AbstractDataProviderType t;
        foreach string name in (UnstructuredConnectorFieldsDataProvider::getCandidateSchemas(type, source)) {
            try {
                t = schema.getTypeForReference(name);
                found = name;
                break;
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err != "INVALID-REFERENCE-TYPE") {
                    rethrow;
                }
            }
        }
        if (!found) {
            softlist<hash<AllowedValueInfo>> known = source
                ? UnstructuredPlatformManifest::SourceTypeValues
                : UnstructuredPlatformManifest::DestinationTypeValues;
            if (!(select known, $1.value == type)) {
                string dir = source ? "source" : "destination";
                string other_dir = source ? "destination" : "source";
                # the type control offers every type in either direction, so picking one that only
                # exists in the other direction is an ordinary mistake worth naming precisely
                softlist<hash<AllowedValueInfo>> other = source
                    ? UnstructuredPlatformManifest::DestinationTypeValues
                    : UnstructuredPlatformManifest::SourceTypeValues;
                if (select other, $1.value == type) {
                    throw "CONNECTOR-TYPE-ERROR", sprintf("connector type %y is a %s connector type, "
                        "not a %s one; set \"direction\" to %y", type, other_dir, dir, other_dir);
                }
                throw "CONNECTOR-TYPE-ERROR", sprintf("connector type %y is not a %s connector type "
                    "Unstructured declares; known types: %y", type, dir, (map $1.value, known));
            }
            # the type is real but Unstructured's own description does not declare a configuration
            # schema for it; saying so is more useful than failing, because the connector can still
            # be created - its settings just have to come from Unstructured's documentation
            return {
                "connector_type": type,
                "schema": NOTHING,
                "fields": (),
            };
        }

        # NOTE parentheses, not braces: "map {k: v, ...}, list" is the hash map operator, which takes
        # a single key-value pair and returns one hash rather than a list of them
        list<hash<auto>> fields = map (
            "name": $1.key,
            "type": $1.value.getType().getName(),
            "required": $1.value.isMandatory(),
            "desc": $1.value.getDescription(),
        ), (t.getFields() ?? {}).pairIterator();

        return {
            "connector_type": type,
            "schema": found,
            "fields": fields,
        };
    }

    #! Returns the component schema names to try for a connector type, most specific first
    private static list<string> getCandidateSchemas(string type, bool source) {
        string base = SchemaNameMap{type} ?? UnstructuredConnectorFieldsDataProvider::camelCase(type);
        string dir = source ? "Source" : "Destination";
        return (
            base + dir + "ConnectorConfigInput",
            base + dir + "ConnectorConfig",
            base + "ConnectorConfigInput",
            base + "ConnectorConfig",
        );
    }

    #! Converts a snake_case connector type to the CamelCase the schema names use
    private static string camelCase(string type) {
        return foldl $1 + $2, (map $1.substr(0, 1).upr() + $1.substr(1), type.split("_"));
    }
}
}
