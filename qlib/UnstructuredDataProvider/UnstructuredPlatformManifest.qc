# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnstructuredDataProvider module: Platform API action manifest

/** UnstructuredPlatformManifest.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The operations the application exports from the Unstructured Platform API
/** Action IDs are a product commitment: an upstream rename is reported as drift and never renames an
    action here.

    @par What the overlay supplies

    The schema types every field correctly, but three things it does not describe have to be supplied
    here, and all three were found by generating the actions rather than by reading the document:

    - Every operation declares \c unstructured-api-key as a parameter.  The connection owns that
      credential, so it is ignored on all of them; without that, every action in the application would
      ask the user to re-enter the API key the connection already holds.
    - A property written as <tt>anyOf: [enum, string]</tt> loses the enum's values.  Unstructured writes
      the connector types and workflow types that way, which is the vendor saying "one of these, or
      anything future" - so the overlay restores the values and marks them creatable.
    - Two listing filters are declared as bare strings although the enum exists as a component:
      \c status on the job listing and \c sort_direction on the workflow listing.

    @since %UnstructuredDataProvider 1.0
*/
public class UnstructuredPlatformManifest {
    public {
        #! Action groups
        const GroupWorkflows = "Workflows";
        const GroupJobs = "Jobs";
        const GroupConnectors = "Connectors";
        const GroupNotifications = "Notifications";

        #! Reference data names
        const RefSource = "unstructured-sources";
        const RefDestination = "unstructured-destinations";
        const RefWorkflow = "unstructured-workflows";
        const RefTemplate = "unstructured-templates";
        const RefChannel = "unstructured-channels";
        const RefJob = "unstructured-jobs";
        const RefJobOutputFile = "unstructured-job-output-files";
        const RefJobOutputNode = "unstructured-job-output-nodes";

        #! The API key is supplied by the connection and must never be an action option
        const ApiKeyOverlay = <RestSchemaFieldOverlayInfo>{
            "ignore": True,
            "connection_option": "apikey",
        };

        #! Source connector types, restored from the schema's own SourceConnectorType enum
        /** The property is written as <tt>anyOf: [SourceConnectorType, string]</tt>, so the values are
            lost in generation; the string branch is what makes them creatable.
        */
        const SourceTypeValues = (
            <AllowedValueInfo>{"value": "airtable",           "display_name": "Airtable"},
            <AllowedValueInfo>{"value": "astradb",            "display_name": "DataStax Astra DB"},
            <AllowedValueInfo>{"value": "box",                "display_name": "Box"},
            <AllowedValueInfo>{"value": "confluence",         "display_name": "Confluence"},
            <AllowedValueInfo>{"value": "couchbase",          "display_name": "Couchbase"},
            <AllowedValueInfo>{"value": "databricks_volumes", "display_name": "Databricks Volumes"},
            <AllowedValueInfo>{"value": "dropbox",            "display_name": "Dropbox"},
            <AllowedValueInfo>{"value": "elasticsearch",      "display_name": "Elasticsearch"},
            <AllowedValueInfo>{"value": "github",             "display_name": "GitHub"},
            <AllowedValueInfo>{"value": "gitlab",             "display_name": "GitLab"},
            <AllowedValueInfo>{"value": "gcs",                "display_name": "Google Cloud Storage"},
            <AllowedValueInfo>{"value": "google_drive",       "display_name": "Google Drive"},
            <AllowedValueInfo>{"value": "kafka-cloud",        "display_name": "Confluent Kafka"},
            <AllowedValueInfo>{"value": "local",              "display_name": "Local Files"},
            <AllowedValueInfo>{"value": "mongodb",            "display_name": "MongoDB"},
            <AllowedValueInfo>{"value": "onedrive",           "display_name": "OneDrive"},
            <AllowedValueInfo>{"value": "outlook",            "display_name": "Outlook"},
            <AllowedValueInfo>{"value": "postgres",           "display_name": "PostgreSQL"},
            <AllowedValueInfo>{"value": "salesforce",         "display_name": "Salesforce"},
            <AllowedValueInfo>{"value": "sftp",               "display_name": "SFTP"},
            <AllowedValueInfo>{"value": "sharepoint",         "display_name": "SharePoint"},
            <AllowedValueInfo>{"value": "slack",              "display_name": "Slack"},
            <AllowedValueInfo>{"value": "snowflake",          "display_name": "Snowflake"},
            <AllowedValueInfo>{"value": "teradata",           "display_name": "Teradata"},
            <AllowedValueInfo>{"value": "vastdb",             "display_name": "VAST DB"},
            <AllowedValueInfo>{"value": "jira",               "display_name": "Jira"},
            <AllowedValueInfo>{"value": "zendesk",            "display_name": "Zendesk"},
            <AllowedValueInfo>{"value": "s3",                 "display_name": "Amazon S3"},
            <AllowedValueInfo>{"value": "azure",              "display_name": "Azure Blob Storage"},
        );

        #! Destination connector types, restored from the schema's own DestinationConnectorType enum
        const DestinationTypeValues = (
            <AllowedValueInfo>{"value": "unstructured_blob_storage",      "display_name": "Unstructured Blob Storage"},
            <AllowedValueInfo>{"value": "astradb",                        "display_name": "DataStax Astra DB"},
            <AllowedValueInfo>{"value": "chroma",                         "display_name": "Chroma"},
            <AllowedValueInfo>{"value": "couchbase",                      "display_name": "Couchbase"},
            <AllowedValueInfo>{"value": "databricks_volumes",             "display_name": "Databricks Volumes"},
            <AllowedValueInfo>{"value": "databricks_volume_delta_tables", "display_name": "Databricks Delta Tables"},
            <AllowedValueInfo>{"value": "delta_table",                    "display_name": "Delta Table"},
            <AllowedValueInfo>{"value": "dropbox",                        "display_name": "Dropbox"},
            <AllowedValueInfo>{"value": "elasticsearch",                  "display_name": "Elasticsearch"},
            <AllowedValueInfo>{"value": "gcs",                            "display_name": "Google Cloud Storage"},
            <AllowedValueInfo>{"value": "kafka-cloud",                    "display_name": "Confluent Kafka"},
            <AllowedValueInfo>{"value": "kdbai",                          "display_name": "KDB.AI"},
            <AllowedValueInfo>{"value": "local",                          "display_name": "Local Files"},
            <AllowedValueInfo>{"value": "milvus",                         "display_name": "Milvus"},
            <AllowedValueInfo>{"value": "mongodb",                        "display_name": "MongoDB"},
            <AllowedValueInfo>{"value": "motherduck",                     "display_name": "MotherDuck"},
            <AllowedValueInfo>{"value": "neo4j",                          "display_name": "Neo4j"},
            <AllowedValueInfo>{"value": "onedrive",                       "display_name": "OneDrive"},
            <AllowedValueInfo>{"value": "pinecone",                       "display_name": "Pinecone"},
            <AllowedValueInfo>{"value": "postgres",                       "display_name": "PostgreSQL"},
            <AllowedValueInfo>{"value": "redis",                          "display_name": "Redis"},
            <AllowedValueInfo>{"value": "qdrant-cloud",                   "display_name": "Qdrant Cloud"},
            <AllowedValueInfo>{"value": "sftp",                           "display_name": "SFTP"},
            <AllowedValueInfo>{"value": "singlestore",                    "display_name": "SingleStore"},
            <AllowedValueInfo>{"value": "snowflake",                      "display_name": "Snowflake"},
            <AllowedValueInfo>{"value": "teradata",                       "display_name": "Teradata"},
            <AllowedValueInfo>{"value": "vastdb",                         "display_name": "VAST DB"},
            <AllowedValueInfo>{"value": "weaviate-cloud",                 "display_name": "Weaviate Cloud"},
            <AllowedValueInfo>{"value": "ibm_watsonx_s3",                 "display_name": "IBM watsonx.data"},
            <AllowedValueInfo>{"value": "s3",                             "display_name": "Amazon S3"},
            <AllowedValueInfo>{"value": "azure",                          "display_name": "Azure Blob Storage"},
        );

        #! Source connector type values keyed by value, for merging the two enums
        const SourceTypeValueMap = map {$1.value: True}, SourceTypeValues;

        #! Every connector type in either direction, with each type listed once
        /** Seventeen types exist in both directions - \c s3, \c azure, \c mongodb and the rest - so
            concatenating the two enums would offer each of them twice in the same control.  The
            direction is chosen separately.
        */
        const ConnectorTypeValues = SourceTypeValues
            + (select DestinationTypeValues, !SourceTypeValueMap{$1.value});

        #! Workflow types, restored from the schema's own WorkflowType enum
        const WorkflowTypeValues = (
            <AllowedValueInfo>{"value": "basic", "display_name": "Basic"},
            <AllowedValueInfo>{"value": "advanced", "display_name": "Advanced"},
            <AllowedValueInfo>{"value": "platinum", "display_name": "Platinum"},
            <AllowedValueInfo>{"value": "custom", "display_name": "Custom"},
        );

        #! Job statuses, restored from the schema's own JobStatus enum
        /** The job listing declares \c status as a bare string, so the values are supplied here.
        */
        const JobStatusValues = (
            <AllowedValueInfo>{"value": "SCHEDULED", "display_name": "Scheduled"},
            <AllowedValueInfo>{"value": "IN_PROGRESS", "display_name": "In Progress"},
            <AllowedValueInfo>{"value": "COMPLETED", "display_name": "Completed"},
            <AllowedValueInfo>{"value": "STOPPED", "display_name": "Stopped"},
            <AllowedValueInfo>{"value": "FAILED", "display_name": "Failed"},
            <AllowedValueInfo>{"value": "REJECTED", "display_name": "Rejected"},
        );

        #! Notification channel types, from the schema's own ChannelType enum
        const ChannelTypeValues = (
            <AllowedValueInfo>{"value": "webhook", "display_name": "Webhook"},
            <AllowedValueInfo>{"value": "email", "display_name": "Email"},
        );

        #! Workflow fields that can be sorted on
        /** The listing declares \c sort_by as a free string and the server accepts any value, so these
            are the sortable fields of the workflow record itself, offered as a creatable list rather
            than enforced.
        */
        const WorkflowSortValues = (
            <AllowedValueInfo>{"value": "created_at", "display_name": "Created"},
            <AllowedValueInfo>{"value": "updated_at", "display_name": "Updated"},
            <AllowedValueInfo>{"value": "name", "display_name": "Name"},
            <AllowedValueInfo>{"value": "id", "display_name": "ID"},
        );

        #! Notification event types, from the schema's own NotificationEventType enum
        const NotificationEventValues = (
            <AllowedValueInfo>{"value": "job.scheduled", "display_name": "Job Scheduled"},
            <AllowedValueInfo>{"value": "job.in_progress", "display_name": "Job In Progress"},
            <AllowedValueInfo>{"value": "job.progress", "display_name": "Job Progress"},
            <AllowedValueInfo>{"value": "job.completed", "display_name": "Job Completed"},
            <AllowedValueInfo>{"value": "job.stopped", "display_name": "Job Stopped"},
            <AllowedValueInfo>{"value": "job.failed", "display_name": "Job Failed"},
        );

        #! Sort directions, restored from the schema's own SortDirection enum
        const SortDirectionValues = (
            <AllowedValueInfo>{"value": "asc", "display_name": "Ascending"},
            <AllowedValueInfo>{"value": "desc", "display_name": "Descending"},
        );

        #! Dropdowns backed by the application's own listing actions
        const ReferenceData = {
            RefSource: <RestSchemaReferenceDataInfo>{
                "name": RefSource,
                "action": "list-sources",
                "value_field": "id",
                "display_field": "name",
                "desc": "source connectors defined in this Unstructured account",
            },
            RefDestination: <RestSchemaReferenceDataInfo>{
                "name": RefDestination,
                "action": "list-destinations",
                "value_field": "id",
                "display_field": "name",
                "desc": "destination connectors defined in this Unstructured account",
            },
            RefWorkflow: <RestSchemaReferenceDataInfo>{
                "name": RefWorkflow,
                "action": "list-workflows",
                "value_field": "id",
                "display_field": "name",
                "desc": "workflows defined in this Unstructured account",
            },
            RefTemplate: <RestSchemaReferenceDataInfo>{
                "name": RefTemplate,
                "action": "list-templates",
                "value_field": "id",
                "display_field": "name",
                "desc": "workflow templates available to this Unstructured account",
            },
            RefJob: <RestSchemaReferenceDataInfo>{
                "name": RefJob,
                "action": "list-jobs",
                "value_field": "id",
                "display_field": "workflow_name",
                "desc": "workflow runs in this Unstructured account",
            },
            RefJobOutputFile: <RestSchemaReferenceDataInfo>{
                "name": RefJobOutputFile,
                "action": "get-job",
                "options_from": {"job_id": "job_id"},
                "records_path": "output_node_files",
                "value_field": "file_id",
                "desc": "output files produced by the selected job",
            },
            RefJobOutputNode: <RestSchemaReferenceDataInfo>{
                "name": RefJobOutputNode,
                "action": "get-job",
                "options_from": {"job_id": "job_id"},
                "records_path": "output_node_files",
                "value_field": "node_id",
                "display_field": "node_subtype",
                "desc": "processing nodes that produced output in the selected job",
            },
            RefChannel: <RestSchemaReferenceDataInfo>{
                "name": RefChannel,
                "action": "list-notification-channels",
                "value_field": "id",
                "display_field": "name",
                "desc": "notification channels defined in this Unstructured account",
            },
        };

        #! The exported operations, in presentation order
        const Manifest = (
            # ------------------------------------------------------------------ workflows
            <RestSchemaActionInfo>{
                "action": "list-workflows",
                "method": "GET",
                "path": "/api/v1/workflows/",
                "display_name": "List Workflows",
                "short_desc": "List the workflows in this Unstructured account",
                "desc": "Lists workflows, with optional filtering by state, by the connectors they "
                    "use, and by when they were created.\n\n"
                    "A workflow ties a source connector to a destination connector through a chain of "
                    "processing nodes, and is what actually runs when documents are ingested.",
                "groups": (GroupWorkflows,),
                "field_order": ("name", "status", "source_id", "destination_id", "created_since",
                    "created_before", "sort_by", "sort_direction", "page", "page_size"),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Return only workflows whose name matches",
                        "preselected": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "State",
                        "short_desc": "Return only workflows in this state",
                        "desc": "Return only workflows in this state.  An `inactive` workflow is not "
                            "run on its schedule.",
                        "allowed_values": (
                            <AllowedValueInfo>{"value": "active", "display_name": "Active"},
                            <AllowedValueInfo>{"value": "inactive", "display_name": "Inactive"},
                        ),
                        "allowed_values_creatable": True,
                        "preselected": True,
                    },
                    "source_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Source Connector",
                        "short_desc": "Return only workflows reading from this source connector",
                        "ref_data": RefSource,
                    },
                    "destination_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Destination Connector",
                        "short_desc": "Return only workflows writing to this destination connector",
                        "ref_data": RefDestination,
                    },
                    "sort_by": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sort By",
                        "short_desc": "The field to sort the results on",
                        "allowed_values": WorkflowSortValues,
                        "allowed_values_creatable": True,
                    },
                    "sort_direction": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sort Direction",
                        "short_desc": "The direction to sort in",
                        "allowed_values": SortDirectionValues,
                        "allowed_values_creatable": True,
                    },
                    "show_only_soft_deleted": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Only Deleted",
                        "short_desc": "Return only workflows that have been deleted",
                    },
                    # no listing of node configurations exists anywhere in the Platform API, so
                    # this is the one identifier in the application with no dropdown behind it
                    "dag_node_configuration_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Node Configuration ID",
                        "short_desc": "Return only workflows using this node configuration",
                        "desc": "Return only workflows using this node configuration.\n\n"
                            "Unstructured publishes no listing of node configurations, so this "
                            "value has to come from a workflow that already uses it - the "
                            "`workflow_nodes` of a workflow returned by **Get Workflow**.",
                    },
                    "created_since": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Created Since",
                        "short_desc": "Return only workflows created on or after this date",
                    },
                    "created_before": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Created Before",
                        "short_desc": "Return only workflows created before this date",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "The number of workflows to return per page",
                        "example_value": 100,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-workflow",
                "method": "GET",
                "path": "/api/v1/workflows/{workflow_id}",
                "display_name": "Get Workflow",
                "short_desc": "Get one workflow by ID",
                "desc": "Returns one workflow, including its processing nodes, its schedule and the "
                    "connectors it is wired to.",
                "groups": (GroupWorkflows,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "workflow_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow",
                        "short_desc": "The workflow to return",
                        "ref_data": RefWorkflow,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-workflow",
                "method": "POST",
                "path": "/api/v1/workflows/",
                "display_name": "Create Workflow",
                "short_desc": "Create a workflow",
                "desc": "Creates a workflow that reads from a source connector, processes documents "
                    "through a chain of nodes, and writes to a destination connector.\n\n"
                    "There are two ways to describe the processing: pick a `workflow_type`, which "
                    "applies Unstructured's own preset chain, or supply `workflow_nodes` to build the "
                    "chain yourself.  A `schedule` makes the workflow recurring; without one it runs "
                    "only when it is run explicitly.",
                "groups": (GroupWorkflows,),
                "field_order": ("name", "workflow_type", "source_id", "destination_id", "schedule",
                    "template_id", "workflow_nodes", "reprocess_all"),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "A name for the workflow",
                        "example_value": "Contracts to Pinecone",
                    },
                    "workflow_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow Type",
                        "short_desc": "The preset processing chain to apply",
                        "desc": "The preset processing chain to apply.  Supply `workflow_nodes` "
                            "instead to build the chain yourself.",
                        "allowed_values": WorkflowTypeValues,
                        "allowed_values_creatable": True,
                    },
                    "source_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Source Connector",
                        "short_desc": "The connector documents are read from",
                        "ref_data": RefSource,
                        "preselected": True,
                    },
                    "destination_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Destination Connector",
                        "short_desc": "The connector results are written to",
                        "ref_data": RefDestination,
                        "preselected": True,
                    },
                    "template_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Template",
                        "short_desc": "The workflow template to build from",
                        "ref_data": RefTemplate,
                    },
                    "schedule": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Schedule",
                        "short_desc": "How often the workflow runs by itself",
                        "desc": "How often the workflow runs on its own.  Leave unset for a workflow "
                            "that runs only when it is run explicitly.",
                        "allowed_values": (
                            <AllowedValueInfo>{"value": "every 15 minutes",
                                "display_name": "Every 15 Minutes"},
                            <AllowedValueInfo>{"value": "every hour", "display_name": "Hourly"},
                            <AllowedValueInfo>{"value": "every 6 hours", "display_name": "Every 6 Hours"},
                            <AllowedValueInfo>{"value": "daily", "display_name": "Daily"},
                            <AllowedValueInfo>{"value": "weekly", "display_name": "Weekly"},
                            <AllowedValueInfo>{"value": "monthly", "display_name": "Monthly"},
                        ),
                        "allowed_values_creatable": True,
                    },
                    "reprocess_all": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reprocess All",
                        "short_desc": "Reprocess documents that have already been processed",
                    },
                    "workflow_nodes": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Processing Nodes",
                        "short_desc": "The processing chain to run, described node by node",
                        "desc": "The processing chain to run, described node by node.  Supply this "
                            "instead of `workflow_type` to build the chain yourself.",
                    },
                    "key": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Stable Key",
                        "short_desc": "A stable identifier for exporting between environments",
                        "desc": "A stable identifier that survives being exported to another "
                            "environment, where the generated ID would not.\n\n"
                            "Derived from `name` when left unset, and immutable once set.  Up to 63 "
                            "characters: lowercase letters, digits and hyphens, with no leading, "
                            "trailing or repeated hyphens.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-workflow",
                "method": "PUT",
                "path": "/api/v1/workflows/{workflow_id}",
                "display_name": "Update Workflow",
                "short_desc": "Update a workflow",
                "desc": "Updates a workflow.  Use `state` to activate or deactivate it without "
                    "deleting it: an `inactive` workflow keeps its definition but does not run on its "
                    "schedule.",
                "groups": (GroupWorkflows,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "workflow_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow",
                        "short_desc": "The workflow to update",
                        "ref_data": RefWorkflow,
                    },
                    "workflow_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow Type",
                        "short_desc": "The preset processing chain to apply",
                        "allowed_values": WorkflowTypeValues,
                        "allowed_values_creatable": True,
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "display_name": "State",
                        "short_desc": "Whether the workflow runs on its schedule",
                        "allowed_values": (
                            <AllowedValueInfo>{"value": "active", "display_name": "Active"},
                            <AllowedValueInfo>{"value": "inactive", "display_name": "Inactive"},
                        ),
                        "allowed_values_creatable": True,
                        "preselected": True,
                    },
                    "source_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Source Connector",
                        "short_desc": "The connector documents are read from",
                        "ref_data": RefSource,
                    },
                    "destination_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Destination Connector",
                        "short_desc": "The connector results are written to",
                        "ref_data": RefDestination,
                    },
                    "template_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Template",
                        "short_desc": "The workflow template to rebuild from",
                        "ref_data": RefTemplate,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-workflow",
                "method": "DELETE",
                "path": "/api/v1/workflows/{workflow_id}",
                "display_name": "Delete Workflow",
                "short_desc": "Delete a workflow",
                "desc": "Deletes a workflow.  The connectors it used are not deleted.",
                "groups": (GroupWorkflows,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "workflow_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow",
                        "short_desc": "The workflow to delete",
                        "ref_data": RefWorkflow,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "run-workflow",
                "method": "POST",
                "path": "/api/v1/workflows/{workflow_id}/run",
                "display_name": "Run Workflow",
                "short_desc": "Run a workflow now and return the job it started",
                "desc": "Runs a workflow immediately, whatever its schedule says, and returns the "
                    "job that was started.\n\n"
                    "The job runs asynchronously: use `get-job` to follow it, or the **Job Status "
                    "Change** event to be told when it finishes.",
                "groups": (GroupWorkflows, GroupJobs),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "workflow_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow",
                        "short_desc": "The workflow to run",
                        "ref_data": RefWorkflow,
                    },
                    "input_files": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Input Files",
                        "short_desc": "Restrict the run to these file IDs",
                        "desc": "Restrict the run to these file IDs.  Leave unset to process "
                            "everything the source connector offers.",
                    },
                },
            },

            # ------------------------------------------------------------------ jobs
            <RestSchemaActionInfo>{
                "action": "list-jobs",
                "method": "GET",
                "path": "/api/v1/jobs/",
                "display_name": "List Jobs",
                "short_desc": "List workflow runs",
                "desc": "Lists jobs, optionally narrowed to one workflow or one status.\n\n"
                    "A job is one run of a workflow, scheduled or explicit.",
                "groups": (GroupJobs,),
                "field_order": ("workflow_id", "status"),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "workflow_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow",
                        "short_desc": "Return only jobs for this workflow",
                        "ref_data": RefWorkflow,
                        "preselected": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Return only jobs in this status",
                        "allowed_values": JobStatusValues,
                        "allowed_values_creatable": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-job",
                "method": "GET",
                "path": "/api/v1/jobs/{job_id}",
                "display_name": "Get Job",
                "short_desc": "Get one job by ID",
                "desc": "Returns one job, including its status and runtime.",
                "groups": (GroupJobs,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Job",
                        "short_desc": "The job to return",
                        "ref_data": RefJob,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-job-details",
                "method": "GET",
                "path": "/api/v1/jobs/{job_id}/details",
                "display_name": "Get Job Details",
                "short_desc": "Get a job's per-node processing detail",
                "desc": "Returns how far each processing node in the workflow has got for this job.  "
                    "Use it to see where a long-running job is spending its time, and which node "
                    "failed when one did.",
                "groups": (GroupJobs,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Job",
                        "short_desc": "The job to describe",
                        "ref_data": RefJob,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-job-failed-files",
                "method": "GET",
                "path": "/api/v1/jobs/{job_id}/failed-files",
                "display_name": "Get Job Failed Files",
                "short_desc": "List the files a job could not process",
                "desc": "Lists the files this job could not process, with the error for each.\n\n"
                    "A job that completes with errors still writes everything that succeeded, so this "
                    "is how to find out what is missing from the destination.",
                "groups": (GroupJobs,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Job",
                        "short_desc": "The job to report on",
                        "ref_data": RefJob,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "download-job-output",
                "method": "GET",
                "path": "/api/v1/jobs/{job_id}/download",
                "display_name": "Download Job Output",
                "short_desc": "Download a processed file from a job",
                "desc": "Downloads one output file produced by a job.\n\n"
                    "A job reports the files it produced as `output_node_files`, so choosing the "
                    "job first fills the file and node lists from it.",
                "groups": (GroupJobs,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Job",
                        "short_desc": "The job that produced the file",
                        "ref_data": RefJob,
                        "preselected": True,
                    },
                    "file_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Output File",
                        "short_desc": "The output file to download",
                        "desc": "The output file to download; the list is the `output_node_files` "
                            "of the selected job.",
                        "ref_data": RefJobOutputFile,
                        "depends_on": "job_id",
                    },
                    "node_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Processing Node",
                        "short_desc": "The processing node that produced the file",
                        "desc": "The processing node that produced the file.  Defaults to the last "
                            "node in the workflow, which is the one that writes the output.",
                        "ref_data": RefJobOutputNode,
                        "depends_on": "job_id",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "cancel-job",
                "method": "POST",
                "path": "/api/v1/jobs/{job_id}/cancel",
                "display_name": "Cancel Job",
                "short_desc": "Cancel a running job",
                "desc": "Cancels a job that is scheduled or in progress.  Documents already written "
                    "to the destination stay there.",
                "groups": (GroupJobs,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Job",
                        "short_desc": "The job to cancel",
                        "ref_data": RefJob,
                    },
                },
            },

            # ------------------------------------------------------------------ source connectors
            <RestSchemaActionInfo>{
                "action": "list-sources",
                "method": "GET",
                "path": "/api/v1/sources/",
                "display_name": "List Source Connectors",
                "short_desc": "List the source connectors in this account",
                "desc": "Lists source connectors, optionally narrowed to one connector type.\n\n"
                    "A source connector is where a workflow reads documents from.",
                "groups": (GroupConnectors,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "source_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector Type",
                        "short_desc": "Return only connectors of this type",
                        "allowed_values": SourceTypeValues,
                        "allowed_values_creatable": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-source",
                "method": "GET",
                "path": "/api/v1/sources/{source_id}",
                "display_name": "Get Source Connector",
                "short_desc": "Get one source connector by ID",
                "desc": "Returns one source connector and its configuration.  Secrets in the "
                    "configuration are returned redacted.",
                "groups": (GroupConnectors,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "source_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Source Connector",
                        "short_desc": "The connector to return",
                        "ref_data": RefSource,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-source",
                "method": "POST",
                "path": "/api/v1/sources/",
                "display_name": "Create Source Connector",
                "short_desc": "Create a source connector",
                "desc": "Creates a source connector for a workflow to read documents from.\n\n"
                    "`config` is specific to the connector type chosen in `type` - an S3 connector "
                    "takes a bucket URL and credentials, a Confluence connector a site URL and an API "
                    "token.  Use **Get Connector Configuration Fields** to see what a given type "
                    "requires before filling this in.",
                "groups": (GroupConnectors,),
                "field_order": ("name", "type", "config"),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "A name for the connector",
                        "example_value": "Contracts bucket",
                    },
                    "type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector Type",
                        "short_desc": "The kind of system documents are read from",
                        "allowed_values": SourceTypeValues,
                        "allowed_values_creatable": True,
                    },
                    "config": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Configuration",
                        "short_desc": "Connector settings for the chosen type",
                        "desc": "Connector settings, whose fields depend on `type`.  **Get Connector "
                            "Configuration Fields** lists them for any type.",
                        "sensitive": True,
                    },
                    "key": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Stable Key",
                        "short_desc": "A stable identifier for exporting between environments",
                        "desc": "A stable identifier that survives being exported to another "
                            "environment, where the generated ID would not.\n\n"
                            "Derived from `name` when left unset, and immutable once set.  Up to 63 "
                            "characters: lowercase letters, digits and hyphens, with no leading, "
                            "trailing or repeated hyphens.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-source",
                "method": "PUT",
                "path": "/api/v1/sources/{source_id}",
                "display_name": "Update Source Connector",
                "short_desc": "Update a source connector's configuration",
                "desc": "Updates a source connector's configuration.  The connector type cannot be "
                    "changed; create a new connector instead.",
                "groups": (GroupConnectors,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "source_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Source Connector",
                        "short_desc": "The connector to update",
                        "ref_data": RefSource,
                    },
                    "config": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Configuration",
                        "short_desc": "Connector settings for the connector's type",
                        "sensitive": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-source",
                "method": "DELETE",
                "path": "/api/v1/sources/{source_id}",
                "display_name": "Delete Source Connector",
                "short_desc": "Delete a source connector",
                "desc": "Deletes a source connector.  A workflow still using it will fail on its "
                    "next run.",
                "groups": (GroupConnectors,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "source_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Source Connector",
                        "short_desc": "The connector to delete",
                        "ref_data": RefSource,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "check-source",
                "method": "POST",
                "path": "/api/v1/sources/{source_id}/connection-check",
                "display_name": "Check Source Connector",
                "short_desc": "Start a connection check for a source connector",
                "desc": "Asks Unstructured to try connecting to the source and report whether the "
                    "credentials and location work.  The check runs asynchronously; read the result "
                    "with **Get Source Connector Check**.",
                "groups": (GroupConnectors,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "source_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Source Connector",
                        "short_desc": "The connector to check",
                        "ref_data": RefSource,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-source-check",
                "method": "GET",
                "path": "/api/v1/sources/{source_id}/connection-check",
                "display_name": "Get Source Connector Check",
                "short_desc": "Read the latest connection check for a source connector",
                "desc": "Returns the most recent connection check for the connector, with its status "
                    "and any error reported.",
                "groups": (GroupConnectors,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "source_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Source Connector",
                        "short_desc": "The connector whose check to read",
                        "ref_data": RefSource,
                    },
                },
            },

            # ------------------------------------------------------------- destination connectors
            <RestSchemaActionInfo>{
                "action": "list-destinations",
                "method": "GET",
                "path": "/api/v1/destinations/",
                "display_name": "List Destination Connectors",
                "short_desc": "List the destination connectors in this account",
                "desc": "Lists destination connectors, optionally narrowed to one connector type.\n\n"
                    "A destination connector is where a workflow writes its processed output - "
                    "typically a vector store for retrieval.",
                "groups": (GroupConnectors,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "destination_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector Type",
                        "short_desc": "Return only connectors of this type",
                        "allowed_values": DestinationTypeValues,
                        "allowed_values_creatable": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-destination",
                "method": "GET",
                "path": "/api/v1/destinations/{destination_id}",
                "display_name": "Get Destination Connector",
                "short_desc": "Get one destination connector by ID",
                "desc": "Returns one destination connector and its configuration.  Secrets in the "
                    "configuration are returned redacted.",
                "groups": (GroupConnectors,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "destination_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Destination Connector",
                        "short_desc": "The connector to return",
                        "ref_data": RefDestination,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-destination",
                "method": "POST",
                "path": "/api/v1/destinations/",
                "display_name": "Create Destination Connector",
                "short_desc": "Create a destination connector",
                "desc": "Creates a destination connector for a workflow to write its output to.\n\n"
                    "`config` is specific to the connector type chosen in `type` - a Pinecone "
                    "connector takes an index and an API key, an S3 connector a bucket URL.  Use "
                    "**Get Connector Configuration Fields** to see what a given type requires.",
                "groups": (GroupConnectors,),
                "field_order": ("name", "type", "config"),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "A name for the connector",
                        "example_value": "Contracts index",
                    },
                    "type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector Type",
                        "short_desc": "The kind of system output is written to",
                        "allowed_values": DestinationTypeValues,
                        "allowed_values_creatable": True,
                    },
                    "config": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Configuration",
                        "short_desc": "Connector settings for the chosen type",
                        "desc": "Connector settings, whose fields depend on `type`.  **Get Connector "
                            "Configuration Fields** lists them for any type.",
                        "sensitive": True,
                    },
                    "key": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Stable Key",
                        "short_desc": "A stable identifier for exporting between environments",
                        "desc": "A stable identifier that survives being exported to another "
                            "environment, where the generated ID would not.\n\n"
                            "Derived from `name` when left unset, and immutable once set.  Up to 63 "
                            "characters: lowercase letters, digits and hyphens, with no leading, "
                            "trailing or repeated hyphens.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-destination",
                "method": "PUT",
                "path": "/api/v1/destinations/{destination_id}",
                "display_name": "Update Destination Connector",
                "short_desc": "Update a destination connector's configuration",
                "desc": "Updates a destination connector's configuration.  The connector type cannot "
                    "be changed; create a new connector instead.",
                "groups": (GroupConnectors,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "destination_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Destination Connector",
                        "short_desc": "The connector to update",
                        "ref_data": RefDestination,
                    },
                    "config": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Configuration",
                        "short_desc": "Connector settings for the connector's type",
                        "sensitive": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-destination",
                "method": "DELETE",
                "path": "/api/v1/destinations/{destination_id}",
                "display_name": "Delete Destination Connector",
                "short_desc": "Delete a destination connector",
                "desc": "Deletes a destination connector.  A workflow still using it will fail on "
                    "its next run.",
                "groups": (GroupConnectors,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "destination_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Destination Connector",
                        "short_desc": "The connector to delete",
                        "ref_data": RefDestination,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "check-destination",
                "method": "POST",
                "path": "/api/v1/destinations/{destination_id}/connection-check",
                "display_name": "Check Destination Connector",
                "short_desc": "Start a connection check for a destination connector",
                "desc": "Asks Unstructured to try connecting to the destination and report whether "
                    "the credentials and location work.  The check runs asynchronously; read the "
                    "result with **Get Destination Connector Check**.",
                "groups": (GroupConnectors,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "destination_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Destination Connector",
                        "short_desc": "The connector to check",
                        "ref_data": RefDestination,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-destination-check",
                "method": "GET",
                "path": "/api/v1/destinations/{destination_id}/connection-check",
                "display_name": "Get Destination Connector Check",
                "short_desc": "Read the latest connection check for a destination connector",
                "desc": "Returns the most recent connection check for the connector, with its status "
                    "and any error reported.",
                "groups": (GroupConnectors,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "destination_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Destination Connector",
                        "short_desc": "The connector whose check to read",
                        "ref_data": RefDestination,
                    },
                },
            },

            # ------------------------------------------------------------- notification channels
            <RestSchemaActionInfo>{
                "action": "list-notification-channels",
                "method": "GET",
                "path": "/api/v1/notifications/channels",
                "display_name": "List Notification Channels",
                "short_desc": "List the notification channels in this account",
                "desc": "Lists notification channels of one kind.  A channel is where Unstructured "
                    "sends job notifications: a webhook URL, or an email address.",
                "groups": (GroupNotifications,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "channel_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Channel Type",
                        "short_desc": "The kind of channel to list",
                        "desc": "The kind of channel to list.  Channels of different kinds are "
                            "listed separately.",
                        "allowed_values": ChannelTypeValues,
                        "allowed_values_creatable": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-notification-channel",
                "method": "GET",
                "path": "/api/v1/notifications/channels/{channel_id}",
                "display_name": "Get Notification Channel",
                "short_desc": "Get one notification channel by ID",
                "desc": "Returns one notification channel and the events it is subscribed to.",
                "groups": (GroupNotifications,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "channel_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Channel",
                        "short_desc": "The channel to return",
                        "ref_data": RefChannel,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-notification-channel",
                "method": "POST",
                "path": "/api/v1/notifications/channels",
                "display_name": "Create Notification Channel",
                "short_desc": "Create a notification channel",
                "desc": "Creates a channel Unstructured sends job notifications to.\n\n"
                    "The settings depend on the kind of channel: a webhook channel takes a `url` "
                    "and a signing `secret`, an email channel takes an `email_config`.  Both take "
                    "`channel_type`, `description` and the `event_types` to subscribe to.\n\n"
                    "A channel receives nothing until it has been verified; use **Verify "
                    "Notification Channel** with the code Unstructured sends.",
                "groups": (GroupNotifications,),
                # NOTE the request body is a oneOf discriminated on channel_type, with a different
                # schema per kind, so there is no single field set to flatten it to; the body is
                # supplied whole
                "flatten_body": False,
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "body": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Channel",
                        "short_desc": "The channel to create",
                        "desc": "The channel to create.\n\n"
                            "For a webhook channel: `channel_type` `webhook`, a `url` to post to, "
                            "an optional signing `secret`, and the `event_types` to subscribe to.  "
                            "For an email channel: `channel_type` `email`, an `email_config` "
                            "naming the recipients, and the same `event_types`.\n\n"
                            "Supplied whole because Unstructured describes the two kinds as "
                            "different request bodies rather than one with optional fields.",
                        "preselected": True,
                        # the secret is a credential wherever it is typed; the update action marks
                        # its own copy the same way
                        "fields": {
                            "secret": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Signing Secret",
                                "short_desc": "The secret webhook payloads are signed with",
                                "desc": "The secret Unstructured signs webhook payloads with, 24 to "
                                    "75 bytes.",
                                "sensitive": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-notification-channel",
                "method": "PATCH",
                "path": "/api/v1/notifications/channels/{channel_id}",
                "display_name": "Update Notification Channel",
                "short_desc": "Update a notification channel",
                "desc": "Updates a notification channel's destination or the events it is subscribed "
                    "to.",
                "groups": (GroupNotifications,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "channel_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Channel",
                        "short_desc": "The channel to update",
                        "ref_data": RefChannel,
                    },
                    "channel_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Channel Type",
                        "short_desc": "How notifications are delivered",
                        "desc": "How notifications are delivered.  A channel's type cannot be "
                            "changed once it is created; this only names it.",
                        "allowed_values": ChannelTypeValues,
                        "allowed_values_creatable": True,
                    },
                    "event_types": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Event Types",
                        "short_desc": "The job events to be notified about",
                        "desc": "The job events this channel is notified about; at least one is "
                            "required.",
                        # a list field enumerates what its ELEMENTS may be: "allowed_values" here
                        # would make each event a one-element list and a two-event subscription a
                        # list of lists
                        "element_allowed_values": NotificationEventValues,
                        "element_allowed_values_creatable": True,
                        "preselected": True,
                    },
                    "enabled": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Enabled",
                        "short_desc": "Whether the channel receives notifications",
                        "desc": "Whether the channel receives notifications.  A disabled channel "
                            "keeps its definition and its subscriptions.",
                        "preselected": True,
                    },
                    "url": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Webhook URL",
                        "short_desc": "Where webhook notifications are posted",
                        "desc": "Where webhook notifications are posted.  HTTPS is required.",
                        "example_value": "https://example.com/unstructured-hook",
                    },
                    "secret": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Signing Secret",
                        "short_desc": "The secret webhook payloads are signed with",
                        "desc": "The secret Unstructured signs webhook payloads with, 24 to 75 "
                            "bytes.  Send an empty string to drop a secret already set.",
                        "sensitive": True,
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "A description of the channel",
                        "desc": "A description of the channel, up to 255 characters.",
                    },
                    "email_config": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Email Settings",
                        "short_desc": "Recipients and reply-to for an email channel",
                        "desc": "Additional recipients and the reply-to address for an email "
                            "channel.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-notification-channel",
                "method": "DELETE",
                "path": "/api/v1/notifications/channels/{channel_id}",
                "display_name": "Delete Notification Channel",
                "short_desc": "Delete a notification channel",
                "desc": "Deletes a notification channel.  Workflows subscribed through it stop being "
                    "notified.",
                "groups": (GroupNotifications,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "channel_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Channel",
                        "short_desc": "The channel to delete",
                        "ref_data": RefChannel,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "verify-notification-channel",
                "method": "POST",
                "path": "/api/v1/notifications/channels/{channel_id}/verify",
                "display_name": "Verify Notification Channel",
                "short_desc": "Send a verification message to a notification channel",
                "desc": "Confirms a channel with the code Unstructured sent to it.  A channel does "
                    "not receive job notifications until it has been verified.",
                "groups": (GroupNotifications,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "channel_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Channel",
                        "short_desc": "The channel to verify",
                        "ref_data": RefChannel,
                    },
                    "code": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Verification Code",
                        "short_desc": "The six-digit code Unstructured sent to the channel",
                        "desc": "The six-digit code Unstructured sent to the channel's address.",
                        "example_value": "123456",
                    },
                },
            },

            # ------------------------------------------------------------------ templates
            <RestSchemaActionInfo>{
                "action": "list-templates",
                "method": "GET",
                "path": "/api/v1/templates/",
                "display_name": "List Workflow Templates",
                "short_desc": "List the workflow templates available to this account",
                "desc": "Lists workflow templates - Unstructured's ready-made processing chains, "
                    "which a workflow can be built from instead of describing its nodes by hand.",
                "groups": (GroupWorkflows,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-template",
                "method": "GET",
                "path": "/api/v1/templates/{template_id}",
                "display_name": "Get Workflow Template",
                "short_desc": "Get one workflow template by ID",
                "desc": "Returns one workflow template and the processing chain it describes.",
                "groups": (GroupWorkflows,),
                "fields": {
                    "unstructured-api-key": ApiKeyOverlay,
                    "template_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Template",
                        "short_desc": "The template to return",
                        "ref_data": RefTemplate,
                    },
                },
            },
        );
    }
}
}
