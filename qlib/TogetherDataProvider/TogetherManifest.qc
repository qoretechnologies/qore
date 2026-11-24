# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TogetherManifest class definition

/** TogetherManifest.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace TogetherDataProvider {
#! Reviewed Together AI operations, presentation, and resource references
/** All completed-response REST operations from the pinned official schema are exported. The two
    WebSocket upgrades are excluded; streaming chat remains available through TogetherLlmProvider.
    The v1 and v2 manifests preserve their own server prefixes. Resource provisioning, training,
    evaluations, and inference can incur charges. Dropdowns fetch one page and accept custom IDs.
*/
public class TogetherManifest {
    public {
        #! Resource dropdown declarations
        const hash<string, hash<RestSchemaReferenceDataInfo>> ReferenceData = {
            "deployments": <RestSchemaReferenceDataInfo>{
                "name": "deployments",
                "action": "list-deployments",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("data",),
            },
            "project-secrets": <RestSchemaReferenceDataInfo>{
                "name": "project-secrets",
                "action": "list-project-secrets",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("data",),
            },
            "project-volumes": <RestSchemaReferenceDataInfo>{
                "name": "project-volumes",
                "action": "list-project-volumes",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("data",),
            },
            "voices": <RestSchemaReferenceDataInfo>{
                "name": "voices",
                "action": "list-voices",
                "records_path": ("data",),
                "filter_from": {
                    "model": "model",
                },
                "children_path": ("voices",),
                "value_field": "name",
                "display_field": "name",
                "desc": "Voices available for the selected speech model.",
            },
            "models": <RestSchemaReferenceDataInfo>{
                "name": "models",
                "action": "list-models",
                "value_field": "id",
                "display_field": "display_name",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
            },
            "model-upload-jobs": <RestSchemaReferenceDataInfo>{
                "name": "model-upload-jobs",
                "action": "list-model-upload-jobs",
                "value_field": "job_id",
                "display_field": "job_id",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("data",),
            },
            "files": <RestSchemaReferenceDataInfo>{
                "name": "files",
                "action": "list-files",
                "value_field": "id",
                "display_field": "filename",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("data",),
            },
            "fine-tunes": <RestSchemaReferenceDataInfo>{
                "name": "fine-tunes",
                "action": "list-fine-tunes",
                "value_field": "id",
                "display_field": "id",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("data",),
            },
            "gpu-clusters": <RestSchemaReferenceDataInfo>{
                "name": "gpu-clusters",
                "action": "list-gpu-clusters",
                "value_field": "cluster_id",
                "display_field": "cluster_id",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("clusters",),
            },
            "regions-and-corresponding-supported-driver-versions": <RestSchemaReferenceDataInfo>{
                "name": "regions-and-corresponding-supported-driver-versions",
                "action": "list-regions-and-corresponding-supported-driver-versions",
                "value_field": "name",
                "display_field": "name",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("regions",),
            },
            "shared-volumes": <RestSchemaReferenceDataInfo>{
                "name": "shared-volumes",
                "action": "list-shared-volumes",
                "value_field": "volume_id",
                "display_field": "volume_id",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("volumes",),
            },
            "endpoints": <RestSchemaReferenceDataInfo>{
                "name": "endpoints",
                "action": "list-endpoints",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("data",),
            },
            "adapters-on-endpoint": <RestSchemaReferenceDataInfo>{
                "name": "adapters-on-endpoint",
                "action": "list-adapters-on-endpoint",
                "value_field": "model_id",
                "display_field": "model_id",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("data",),
                "options_from": {
                    "endpointId": "endpointId",
                },
            },
            "hardware": <RestSchemaReferenceDataInfo>{
                "name": "hardware",
                "action": "list-hardware",
                "value_field": "id",
                "display_field": "id",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("data",),
            },
            "batches": <RestSchemaReferenceDataInfo>{
                "name": "batches",
                "action": "list-batches",
                "value_field": "id",
                "display_field": "id",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
            },
            "evaluations": <RestSchemaReferenceDataInfo>{
                "name": "evaluations",
                "action": "list-evaluations",
                "value_field": "workflow_id",
                "display_field": "workflow_id",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
            },
            "model-resources": <RestSchemaReferenceDataInfo>{
                "name": "model-resources",
                "action": "list-model-resources",
                "value_field": "id",
                "display_field": "id",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("data",),
            },
            "training-sessions": <RestSchemaReferenceDataInfo>{
                "name": "training-sessions",
                "action": "list-training-sessions",
                "value_field": "id",
                "display_field": "display_name",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("data",),
            },
            "addons": <RestSchemaReferenceDataInfo>{
                "name": "addons",
                "action": "list-addons",
                "value_field": "name",
                "display_field": "name",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("add_ons",),
                "options_from": {
                    "cluster_id": "cluster_id",
                },
            },
            "remediations": <RestSchemaReferenceDataInfo>{
                "name": "remediations",
                "action": "list-remediations",
                "value_field": "id",
                "display_field": "id",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("remediations",),
                "options_from": {
                    "cluster_id": "cluster_id",
                    "instance_id": "instance_id",
                },
            },
            "passive-health-check-alerts": <RestSchemaReferenceDataInfo>{
                "name": "passive-health-check-alerts",
                "action": "list-passive-health-check-alerts",
                "value_field": "cluster_id",
                "display_field": "cluster_id",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("alerts",),
            },
            "fine-tune-models": <RestSchemaReferenceDataInfo>{
                "name": "fine-tune-models",
                "action": "list-fine-tune-models",
                "records_path": ("detailed_models",),
                "value_field": "name",
                "display_field": "name",
                "desc": "Base models supported for fine-tuning.",
            },
            "chat-models": <RestSchemaReferenceDataInfo>{
                "name": "chat-models",
                "action": "list-models",
                "value_field": "id",
                "display_field": "display_name",
                "record_filter": {
                    "type": "chat",
                },
                "desc": "Models for this task. Dedicated model names can also be entered directly.",
            },
            "embedding-models": <RestSchemaReferenceDataInfo>{
                "name": "embedding-models",
                "action": "list-models",
                "value_field": "id",
                "display_field": "display_name",
                "record_filter": {
                    "type": "embedding",
                },
                "desc": "Models for this task. Dedicated model names can also be entered directly.",
            },
            "image-models": <RestSchemaReferenceDataInfo>{
                "name": "image-models",
                "action": "list-models",
                "value_field": "id",
                "display_field": "display_name",
                "record_filter": {
                    "type": "image",
                },
                "desc": "Models for this task. Dedicated model names can also be entered directly.",
            },
            "rerank-models": <RestSchemaReferenceDataInfo>{
                "name": "rerank-models",
                "action": "list-models",
                "value_field": "id",
                "display_field": "display_name",
                "record_filter": {
                    "type": "rerank",
                },
                "desc": "Models for this task. Dedicated model names can also be entered directly.",
            },
            "language-models": <RestSchemaReferenceDataInfo>{
                "name": "language-models",
                "action": "list-models",
                "value_field": "id",
                "display_field": "display_name",
                "record_filter": {
                    "type": "language",
                },
                "desc": "Models for this task. Dedicated model names can also be entered directly.",
            },
            "projects": <RestSchemaReferenceDataInfo>{
                "name": "projects",
                "action": "get-api-key-identity",
                "single_record": True,
                "value_field": "project_id",
                "display_field": "project_name",
                "desc": "The scope of the API key. Enter another accessible identifier directly if needed.",
            },
            "organizations": <RestSchemaReferenceDataInfo>{
                "name": "organizations",
                "action": "get-api-key-identity",
                "single_record": True,
                "value_field": "organization_id",
                "display_field": "organization_name",
                "desc": "The scope of the API key. Enter another accessible identifier directly if needed.",
            },
            "speech-models": <RestSchemaReferenceDataInfo>{
                "name": "speech-models",
                "action": "list-voices",
                "records_path": ("data",),
                "value_field": "model",
                "display_field": "model",
                "desc": "Models with available speech voices.",
            },
            "code-sessions": <RestSchemaReferenceDataInfo>{
                "name": "code-sessions",
                "action": "list-active-sessions",
                "records_path": ("data", "sessions",),
                "value_field": "id",
                "desc": "Active code interpreter sessions.",
            },
            "gpu-regions": <RestSchemaReferenceDataInfo>{
                "name": "gpu-regions",
                "action": "list-regions-and-corresponding-supported-driver-versions",
                "value_field": "name",
                "display_field": "name",
                "desc": "Choose an existing resource. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("regions",),
            },
            "nvidia-version-id": <RestSchemaReferenceDataInfo>{
                "name": "nvidia-version-id",
                "action": "list-regions-and-corresponding-supported-driver-versions",
                "records_path": ("regions",),
                "filter_from": {
                    "name": "region",
                },
                "children_path": ("driver_versions",),
                "value_field": "id",
                "desc": "Software versions supported in the selected GPU region.",
            },
            "cuda-version": <RestSchemaReferenceDataInfo>{
                "name": "cuda-version",
                "action": "list-regions-and-corresponding-supported-driver-versions",
                "records_path": ("regions",),
                "filter_from": {
                    "name": "region",
                },
                "children_path": ("driver_versions",),
                "value_field": "cuda_version",
                "desc": "Software versions supported in the selected GPU region.",
            },
            "nvidia-driver-version": <RestSchemaReferenceDataInfo>{
                "name": "nvidia-driver-version",
                "action": "list-regions-and-corresponding-supported-driver-versions",
                "records_path": ("regions",),
                "filter_from": {
                    "name": "region",
                },
                "children_path": ("driver_versions",),
                "value_field": "nvidia_driver_version",
                "desc": "Software versions supported in the selected GPU region.",
            },
        };

        #! Stable action identities and their reviewed contracts
        const list<hash<RestSchemaActionInfo>> Manifest = (
            <RestSchemaActionInfo>{
                "action": "list-deployments",
                "method": "GET",
                "path": "/deployments",
                "display_name": "List Deployments",
                "short_desc": "Get the list of deployments",
                "desc": "Get a list of all deployments in your project.",
                "groups": ("Containers",),
            },
            <RestSchemaActionInfo>{
                "action": "create-new-deployment",
                "method": "POST",
                "path": "/deployments",
                "display_name": "Create New Deployment",
                "short_desc": "Create a new deployment",
                "desc": "Create a new deployment with specified configuration. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Containers",),
                "fields": {
                    "args": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Args",
                        "short_desc": "Args overrides the container's CMD",
                    },
                    "autoscaling": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Autoscaling",
                        "short_desc": "Autoscaling configuration",
                        "fields": {
                            "model": <RestSchemaFieldOverlayInfo>{
                                "ref_data": "models",
                                "supports_custom_values": True,
                            },
                        },
                    },
                    "command": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Command",
                        "short_desc": "Command overrides the container's ENTRYPOINT",
                    },
                    "cpu": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cpu",
                        "short_desc": "Cpu",
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "Description is an optional human-readable description of your deployment",
                    },
                    "environment_variables": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Environment Variables",
                        "short_desc": "Environment Variables",
                        "fields": {
                            "value_from_secret": <RestSchemaFieldOverlayInfo>{
                                "ref_data": "project-secrets",
                                "supports_custom_values": True,
                            },
                        },
                    },
                    "gpu_count": <RestSchemaFieldOverlayInfo>{
                        "display_name": "GPU Count",
                        "short_desc": "GPUCount is the number of GPUs to allocate per container instance",
                    },
                    "gpu_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "GPU Type",
                        "short_desc": "GPUType specifies the GPU hardware to use (e.g., \"h100-80gb\")",
                        "preselected": True,
                        "required": True,
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "h100-80gb",
                            "display_name": "H100 80gb",
                        }, <AllowedValueInfo>{
                            "value": "h100-40gb-mig",
                            "display_name": "H100 40gb Mig",
                        }, <AllowedValueInfo>{
                            "value": "h200-140gb",
                            "display_name": "H200 140gb",
                        }, <AllowedValueInfo>{
                            "value": "b200-192gb",
                            "display_name": "B200 192gb",
                        },),
                    },
                    "health_check_path": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Health Check Path",
                        "short_desc": "HealthCheckPath is the HTTP path for health checks (e.g., \"/health\")",
                    },
                    "image": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Image",
                        "short_desc": "Image is the container image to deploy from registry.together.ai",
                        "preselected": True,
                        "required": True,
                    },
                    "max_replicas": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Max Replicas",
                        "short_desc": "MaxReplicas is the maximum number of container instances",
                    },
                    "memory": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Memory",
                        "short_desc": "Memory",
                    },
                    "min_replicas": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Min Replicas",
                        "short_desc": "MinReplicas is the minimum number of container instances to run",
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name is the unique identifier for your deployment",
                        "preselected": True,
                        "required": True,
                    },
                    "port": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Port",
                        "short_desc": "Port",
                    },
                    "storage": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Storage",
                        "short_desc": "Storage",
                    },
                    "termination_grace_period_seconds": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Termination Grace Period Seconds",
                        "short_desc": "Termination Grace Period Seconds",
                    },
                    "volumes": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Volumes",
                        "short_desc": "Volumes is a list of volume mounts to attach to the container",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-deployment",
                "method": "DELETE",
                "path": "/deployments/{id}",
                "display_name": "Delete Deployment",
                "short_desc": "Delete a deployment",
                "desc": "Delete an existing deployment. **Deletion is permanent.** Check the selected resource before running this action.",
                "groups": ("Containers",),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "deployments",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-deployment-by-id-or-name",
                "method": "GET",
                "path": "/deployments/{id}",
                "display_name": "Get Deployment By ID Or Name",
                "short_desc": "Get a deployment by ID or name",
                "desc": "Retrieve details of a specific deployment by its ID or name.",
                "groups": ("Containers",),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "deployments",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-deployment",
                "method": "PATCH",
                "path": "/deployments/{id}",
                "display_name": "Update Deployment",
                "short_desc": "Update a deployment",
                "desc": "Update an existing deployment configuration. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Containers",),
                "fields": {
                    "args": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Args",
                        "short_desc": "Args overrides the container's CMD",
                    },
                    "autoscaling": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Autoscaling",
                        "short_desc": "Autoscaling configuration for the deployment",
                        "fields": {
                            "model": <RestSchemaFieldOverlayInfo>{
                                "ref_data": "models",
                                "supports_custom_values": True,
                            },
                        },
                    },
                    "command": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Command",
                        "short_desc": "Command overrides the container's ENTRYPOINT",
                    },
                    "cpu": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cpu",
                        "short_desc": "Cpu",
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "Description is an optional human-readable description of your deployment",
                    },
                    "environment_variables": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Environment Variables",
                        "short_desc": "Environment Variables",
                        "fields": {
                            "value_from_secret": <RestSchemaFieldOverlayInfo>{
                                "ref_data": "project-secrets",
                                "supports_custom_values": True,
                            },
                        },
                    },
                    "gpu_count": <RestSchemaFieldOverlayInfo>{
                        "display_name": "GPU Count",
                        "short_desc": "GPUCount is the number of GPUs to allocate per container instance",
                    },
                    "gpu_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "GPU Type",
                        "short_desc": "GPUType specifies the GPU hardware to use (e.g., \"h100-80gb\")",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "h100-80gb",
                            "display_name": "H100 80gb",
                        }, <AllowedValueInfo>{
                            "value": "h100-40gb-mig",
                            "display_name": "H100 40gb Mig",
                        }, <AllowedValueInfo>{
                            "value": "h200-140gb",
                            "display_name": "H200 140gb",
                        }, <AllowedValueInfo>{
                            "value": "b200-192gb",
                            "display_name": "B200 192gb",
                        },),
                    },
                    "health_check_path": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Health Check Path",
                        "short_desc": "HealthCheckPath is the HTTP path for health checks (e.g., \"/health\")",
                    },
                    "image": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Image",
                        "short_desc": "Image is the container image to deploy from registry.together.ai",
                    },
                    "max_replicas": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Max Replicas",
                        "short_desc": "MaxReplicas is the maximum number of replicas that can be scaled up to",
                    },
                    "memory": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Memory",
                        "short_desc": "Memory",
                    },
                    "min_replicas": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Min Replicas",
                        "short_desc": "MinReplicas is the minimum number of replicas to run",
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name is the new unique identifier for your deployment",
                    },
                    "port": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Port",
                        "short_desc": "Port",
                    },
                    "storage": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Storage",
                        "short_desc": "Storage",
                    },
                    "termination_grace_period_seconds": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Termination Grace Period Seconds",
                        "short_desc": "Termination Grace Period Seconds",
                    },
                    "volumes": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Volumes",
                        "short_desc": "Volumes is a list of volume mounts to attach to the container",
                    },
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "deployments",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-logs-for-deployment",
                "method": "GET",
                "path": "/deployments/{id}/logs",
                "display_name": "Get Logs For Deployment",
                "short_desc": "Get logs for a deployment",
                "desc": "Retrieve logs from a deployment, optionally filtered by replica ID.",
                "groups": ("Containers",),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "deployments",
                        "supports_custom_values": True,
                    },
                    "replica_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Replica ID",
                        "short_desc": "Value for Replica ID",
                    },
                    "version": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Version",
                        "short_desc": "Value for Version",
                    },
                    "revision": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Revision",
                        "short_desc": "Value for Revision",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-project-secrets",
                "method": "GET",
                "path": "/deployments/secrets",
                "display_name": "List Project Secrets",
                "short_desc": "Get the list of project secrets",
                "desc": "Retrieve all secrets in your project.",
                "groups": ("Containers",),
            },
            <RestSchemaActionInfo>{
                "action": "create-new-secret",
                "method": "POST",
                "path": "/deployments/secrets",
                "display_name": "Create New Secret",
                "short_desc": "Create a new secret",
                "desc": "Create a new secret to store sensitive configuration values. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Containers",),
                "fields": {
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "Description",
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name is the unique identifier for the secret",
                        "preselected": True,
                        "required": True,
                    },
                    "project_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Project ID",
                        "short_desc": "Project ID",
                        "ref_data": "projects",
                        "supports_custom_values": True,
                    },
                    "value": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Value",
                        "short_desc": "Value",
                        "preselected": True,
                        "required": True,
                        "sensitive": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-secret",
                "method": "DELETE",
                "path": "/deployments/secrets/{id}",
                "display_name": "Delete Secret",
                "short_desc": "Delete a secret",
                "desc": "Delete an existing secret. **Deletion is permanent.** Check the selected resource before running this action.",
                "groups": ("Containers",),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "project-secrets",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-secret-by-id-or-name",
                "method": "GET",
                "path": "/deployments/secrets/{id}",
                "display_name": "Get Secret By ID Or Name",
                "short_desc": "Get a secret by ID or name",
                "desc": "Retrieve details of a specific secret by its ID or name.",
                "groups": ("Containers",),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "project-secrets",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-secret",
                "method": "PATCH",
                "path": "/deployments/secrets/{id}",
                "display_name": "Update Secret",
                "short_desc": "Update a secret",
                "desc": "Update an existing secret's value or metadata. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Containers",),
                "fields": {
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "Description",
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name is the new unique identifier for the secret",
                    },
                    "project_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Project ID",
                        "short_desc": "Project ID",
                        "ref_data": "projects",
                        "supports_custom_values": True,
                    },
                    "value": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Value",
                        "short_desc": "Value is the new sensitive data to store securely",
                        "sensitive": True,
                    },
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "project-secrets",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "download-storage-file",
                "method": "GET",
                "path": "/deployments/storage/{filename}",
                "display_name": "Download Storage File",
                "short_desc": "Download a file",
                "desc": "Download a file by redirecting to a signed URL.",
                "groups": ("Containers",),
                "fields": {
                    "filename": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filename",
                        "short_desc": "Value for Filename",
                        "preselected": True,
                        "required": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-file-download-url",
                "method": "GET",
                "path": "/deployments/storage/{filename}/url",
                "display_name": "Get File Download URL",
                "short_desc": "Get a file download URL",
                "desc": "Create a signed URL for downloading a deployment storage file.",
                "groups": ("Containers",),
                "fields": {
                    "filename": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filename",
                        "short_desc": "Value for Filename",
                        "preselected": True,
                        "required": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-project-volumes",
                "method": "GET",
                "path": "/deployments/storage/volumes",
                "display_name": "List Project Volumes",
                "short_desc": "Get the list of project volumes",
                "desc": "Retrieve all volumes in your project.",
                "groups": ("Containers",),
            },
            <RestSchemaActionInfo>{
                "action": "create-new-volume",
                "method": "POST",
                "path": "/deployments/storage/volumes",
                "display_name": "Create New Volume",
                "short_desc": "Create a new volume",
                "desc": "Create a new volume to preload files in deployments. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Containers",),
                "fields": {
                    "content": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Content",
                        "short_desc": "Content specifies the content configuration for this volume",
                        "preselected": True,
                        "required": True,
                        "fields": {
                            "type": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (<AllowedValueInfo>{
                                    "value": "files",
                                    "display_name": "Files",
                                },),
                            },
                        },
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name is the unique identifier for the volume within the project",
                        "preselected": True,
                        "required": True,
                    },
                    "type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Type",
                        "short_desc": "Type is the volume type (currently only \"readOnly\" is supported)",
                        "preselected": True,
                        "required": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-volume",
                "method": "DELETE",
                "path": "/deployments/storage/volumes/{id}",
                "display_name": "Delete Volume",
                "short_desc": "Delete a volume",
                "desc": "Delete an existing volume. **Deletion is permanent.** Check the selected resource before running this action.",
                "groups": ("Containers",),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "project-volumes",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-volume-by-id-or-name",
                "method": "GET",
                "path": "/deployments/storage/volumes/{id}",
                "display_name": "Get Volume By ID Or Name",
                "short_desc": "Get a volume by ID or name",
                "desc": "Retrieve details of a specific volume by its ID or name.",
                "groups": ("Containers",),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "project-volumes",
                        "supports_custom_values": True,
                    },
                    "version": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Version",
                        "short_desc": "Value for Version",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-volume",
                "method": "PATCH",
                "path": "/deployments/storage/volumes/{id}",
                "display_name": "Update Volume",
                "short_desc": "Update a volume",
                "desc": "Update an existing volume's configuration or contents. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Containers",),
                "fields": {
                    "content": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Content",
                        "short_desc": "Content specifies the new content to preload to this volume",
                        "fields": {
                            "type": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (<AllowedValueInfo>{
                                    "value": "files",
                                    "display_name": "Files",
                                },),
                            },
                        },
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name is the new unique identifier for the volume within the project",
                    },
                    "type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Type",
                        "short_desc": "Type is the new volume type (currently only \"readOnly\" is supported)",
                    },
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "project-volumes",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-voices",
                "method": "GET",
                "path": "/voices",
                "display_name": "List Voices",
                "short_desc": "Fetch available voices for each model",
                "desc": "Fetch available voices for each model.",
                "groups": ("Inference",),
                "operation_id": "fetchVoices",
            },
            <RestSchemaActionInfo>{
                "action": "chat-completion",
                "method": "POST",
                "mutates_state": False,
                "path": "/chat/completions",
                "response_content_type": "application/json",
                "display_name": "Chat Completion",
                "short_desc": "Create chat completion",
                "desc": "Generate a model response for a given chat conversation. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Inference",),
                "operation_id": "chat-completions",
                "response_fields": {
                    "created": <RestSchemaFieldOverlayInfo>{
                        "codec": "unix-seconds",
                    },
                },
                "fields": {
                    "messages": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Messages",
                        "short_desc": "A list of messages comprising the conversation so far",
                        "preselected": True,
                        "required": True,
                        "fields": {
                            "tool_calls": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "type": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (<AllowedValueInfo>{
                                            "value": "function",
                                            "display_name": "Function",
                                        },),
                                    },
                                },
                            },
                        },
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "The name of the model to query",
                        "preselected": True,
                        "required": True,
                        "ref_data": "chat-models",
                        "supports_custom_values": True,
                    },
                    "max_tokens": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Max Tokens",
                        "short_desc": "The maximum number of tokens to generate",
                        "preselected": True,
                    },
                    "stop": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Stop",
                        "short_desc": "A list of string sequences that truncate (stop) inference text output",
                    },
                    "temperature": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Temperature",
                        "short_desc": "Temperature",
                        "preselected": True,
                    },
                    "top_p": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Top P",
                        "short_desc": "Top P",
                    },
                    "top_k": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Top K",
                        "short_desc": "Top K",
                    },
                    "context_length_exceeded_behavior": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Context Length Exceeded Behavior",
                        "short_desc": "Context Length Exceeded Behavior",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "truncate",
                            "display_name": "Truncate",
                        }, <AllowedValueInfo>{
                            "value": "error",
                            "display_name": "Error",
                        },),
                    },
                    "repetition_penalty": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Repetition Penalty",
                        "short_desc": "Repetition Penalty",
                    },
                    "stream": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "logprobs": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Logprobs",
                        "short_desc": "Logprobs",
                    },
                    "echo": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Echo",
                        "short_desc": "If true, the response contains the prompt",
                    },
                    "n": <RestSchemaFieldOverlayInfo>{
                        "display_name": "N",
                        "short_desc": "The number of completions to generate for each prompt",
                        "preselected": True,
                    },
                    "min_p": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Min P",
                        "short_desc": "Min P",
                    },
                    "presence_penalty": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Presence Penalty",
                        "short_desc": "Presence Penalty",
                    },
                    "frequency_penalty": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Frequency Penalty",
                        "short_desc": "Frequency Penalty",
                    },
                    "logit_bias": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Logit Bias",
                        "short_desc": "Adjusts the likelihood of specific tokens appearing in the generated output",
                        "example_value": {
                            "1024": -10.5,
                            "105": 21.4,
                        },
                    },
                    "seed": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Seed",
                        "short_desc": "Seed value for reproducibility",
                        "example_value": 42,
                    },
                    "function_call": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Function Call",
                        "short_desc": "Value for Function Call",
                    },
                    "response_format": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Response Format",
                        "short_desc": "An object specifying the format that the model must output",
                        "preselected": True,
                    },
                    "tools": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Tools",
                        "short_desc": "A list of tools the model may call",
                    },
                    "tool_choice": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Tool Choice",
                        "short_desc": "Controls which (if any) function is called by the model",
                        "fields": {
                            "type": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (<AllowedValueInfo>{
                                    "value": "function",
                                    "display_name": "Function",
                                },),
                            },
                        },
                    },
                    "compliance": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Compliance",
                        "short_desc": "Value for Compliance",
                    },
                    "chat_template_kwargs": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Chat Template Kwargs",
                        "short_desc": "Additional configuration to pass to model engine",
                    },
                    "safety_model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Safety Model",
                        "short_desc": "The name of the moderation model used to validate tokens",
                        "example_value": "safety_model_name",
                    },
                    "reasoning_effort": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reasoning Effort",
                        "short_desc": "Reasoning Effort",
                        "example_value": "medium",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "low",
                            "display_name": "Low",
                        }, <AllowedValueInfo>{
                            "value": "medium",
                            "display_name": "Medium",
                        }, <AllowedValueInfo>{
                            "value": "high",
                            "display_name": "High",
                        },),
                    },
                    "reasoning": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reasoning",
                        "short_desc": "Reasoning",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "text-completion",
                "method": "POST",
                "mutates_state": False,
                "path": "/completions",
                "response_content_type": "application/json",
                "display_name": "Text Completion",
                "short_desc": "Create completion",
                "desc": "Generate text completions for a given prompt using a language, code, or image model. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Inference",),
                "operation_id": "completions",
                "fields": {
                    "prompt": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Prompt",
                        "short_desc": "A string providing context for the model to complete",
                        "preselected": True,
                        "required": True,
                        "example_value": "<s>[INST] What is the capital of France? [/INST]",
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "The name of the model to query",
                        "preselected": True,
                        "required": True,
                        "example_value": "mistralai/Mixtral-8x7B-Instruct-v0.1",
                        "ref_data": "language-models",
                        "supports_custom_values": True,
                    },
                    "max_tokens": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Max Tokens",
                        "short_desc": "The maximum number of tokens to generate",
                        "preselected": True,
                    },
                    "stop": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Stop",
                        "short_desc": "A list of string sequences that truncate (stop) inference text output",
                    },
                    "temperature": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Temperature",
                        "short_desc": "Temperature",
                        "preselected": True,
                    },
                    "top_p": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Top P",
                        "short_desc": "Top P",
                    },
                    "top_k": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Top K",
                        "short_desc": "Top K",
                    },
                    "repetition_penalty": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Repetition Penalty",
                        "short_desc": "Repetition Penalty",
                    },
                    "stream": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "logprobs": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Logprobs",
                        "short_desc": "Logprobs",
                    },
                    "echo": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Echo",
                        "short_desc": "If true, the response contains the prompt",
                    },
                    "n": <RestSchemaFieldOverlayInfo>{
                        "display_name": "N",
                        "short_desc": "The number of completions to generate for each prompt",
                        "preselected": True,
                    },
                    "safety_model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Safety Model",
                        "short_desc": "The name of the moderation model used to validate tokens",
                        "example_value": "safety_model_name",
                    },
                    "min_p": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Min P",
                        "short_desc": "Min P",
                    },
                    "presence_penalty": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Presence Penalty",
                        "short_desc": "Presence Penalty",
                    },
                    "frequency_penalty": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Frequency Penalty",
                        "short_desc": "Frequency Penalty",
                    },
                    "logit_bias": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Logit Bias",
                        "short_desc": "Adjusts the likelihood of specific tokens appearing in the generated output",
                        "example_value": {
                            "1024": -10.5,
                            "105": 21.4,
                        },
                    },
                    "seed": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Seed",
                        "short_desc": "Seed value for reproducibility",
                        "example_value": 42,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-embeddings",
                "method": "POST",
                "mutates_state": False,
                "path": "/embeddings",
                "display_name": "Create Embeddings",
                "short_desc": "Create embedding",
                "desc": "Generate vector embeddings for one or more text inputs. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Inference",),
                "operation_id": "embeddings",
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "The name of the embedding model to use",
                        "preselected": True,
                        "required": True,
                        "example_value": "togethercomputer/m2-bert-80M-8k-retrieval",
                        "ref_data": "embedding-models",
                        "supports_custom_values": True,
                    },
                    "input": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Input",
                        "short_desc": "Value for Input",
                        "preselected": True,
                        "required": True,
                        "example_value": "Our solar system orbits the Milky Way galaxy at about 515,000 mph",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-models",
                "method": "GET",
                "path": "/models",
                "display_name": "List Models",
                "short_desc": "List all models",
                "desc": "Lists all of Together's open-source models and metadata including pricing, chat template, and context.",
                "groups": ("Models",),
                "operation_id": "models",
                "fields": {
                    "dedicated": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dedicated",
                        "short_desc": "Value for Dedicated",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "upload-model",
                "method": "POST",
                "path": "/models",
                "display_name": "Upload Model",
                "short_desc": "Upload a custom model or adapter",
                "desc": "Upload a custom model or adapter from Hugging Face or S3. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Models",),
                "operation_id": "uploadModel",
                "fields": {
                    "model_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model Name",
                        "short_desc": "The name to give to your uploaded model",
                        "preselected": True,
                        "required": True,
                        "example_value": "Qwen2.5-72B-Instruct",
                    },
                    "model_source": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model Source",
                        "short_desc": "The source location of the model (Hugging Face repo or S3 path)",
                        "preselected": True,
                        "required": True,
                        "example_value": "unsloth/Qwen2.5-72B-Instruct",
                    },
                    "model_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model Type",
                        "short_desc": "Whether the model is a full model or an adapter",
                        "example_value": "model",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "model",
                            "display_name": "Model",
                        }, <AllowedValueInfo>{
                            "value": "adapter",
                            "display_name": "Adapter",
                        },),
                    },
                    "hf_token": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Hugging Face Token",
                        "short_desc": "Hugging Face token (if uploading from Hugging Face)",
                        "example_value": "hf_examplehuggingfacetoken",
                        "sensitive": True,
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "A description of your model",
                        "example_value": "Finetuned Qwen2.5-72B-Instruct by Unsloth",
                    },
                    "base_model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Base Model",
                        "short_desc": "Base Model",
                        "example_value": "Qwen/Qwen2.5-72B-Instruct",
                    },
                    "lora_model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "LoRA Model",
                        "short_desc": "LoRA Model",
                        "example_value": "my_username/Qwen2.5-72B-Instruct-lora",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-model-upload-job",
                "method": "GET",
                "path": "/jobs/{jobId}",
                "display_name": "Get Model Upload Job",
                "short_desc": "Get job status",
                "desc": "Get the status of a specific job.",
                "groups": ("Models",),
                "operation_id": "getJob",
                "fields": {
                    "jobId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Job ID",
                        "short_desc": "Value for Job ID",
                        "preselected": True,
                        "required": True,
                        "example_value": "job-a15dad11-8d8e-4007-97c5-a211304de284",
                        "ref_data": "model-upload-jobs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-model-upload-jobs",
                "method": "GET",
                "path": "/jobs",
                "display_name": "List Model Upload Jobs",
                "short_desc": "List all jobs",
                "desc": "List all jobs and their statuses.",
                "groups": ("Models",),
                "operation_id": "listJobs",
            },
            <RestSchemaActionInfo>{
                "action": "generate-image",
                "method": "POST",
                "mutates_state": False,
                "path": "/images/generations",
                "display_name": "Generate Image",
                "short_desc": "Create image",
                "desc": "Use an image model to generate an image for a given prompt. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Inference",),
                "fields": {
                    "prompt": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Prompt",
                        "short_desc": "A description of the desired images",
                        "preselected": True,
                        "required": True,
                        "example_value": "cat floating in space, cinematic",
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "The model to use for image generation",
                        "preselected": True,
                        "required": True,
                        "example_value": "black-forest-labs/FLUX.1-schnell",
                        "ref_data": "image-models",
                        "supports_custom_values": True,
                    },
                    "steps": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Steps",
                        "short_desc": "Number of generation steps",
                        "preselected": True,
                    },
                    "image_url": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Image URL",
                        "short_desc": "URL of an image to use for image models that support it",
                    },
                    "seed": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Seed",
                        "short_desc": "Seed used for generation",
                    },
                    "n": <RestSchemaFieldOverlayInfo>{
                        "display_name": "N",
                        "short_desc": "Number of image results to generate",
                        "preselected": True,
                    },
                    "height": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Height",
                        "short_desc": "Height of the image to generate in number of pixels",
                        "preselected": True,
                    },
                    "width": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Width",
                        "short_desc": "Width of the image to generate in number of pixels",
                        "preselected": True,
                    },
                    "negative_prompt": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Negative Prompt",
                        "short_desc": "The prompt or prompts not to guide the image generation",
                    },
                    "response_format": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Response Format",
                        "short_desc": "Format of the image response",
                        "preselected": True,
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "base64",
                            "display_name": "Base64",
                        }, <AllowedValueInfo>{
                            "value": "url",
                            "display_name": "URL",
                        },),
                    },
                    "guidance_scale": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Guidance Scale",
                        "short_desc": "Adjusts the alignment of the generated image with the input prompt",
                    },
                    "output_format": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Output Format",
                        "short_desc": "The format of the image response",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "jpeg",
                            "display_name": "Jpeg",
                        }, <AllowedValueInfo>{
                            "value": "png",
                            "display_name": "Png",
                        },),
                    },
                    "image_loras": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Image Loras",
                        "short_desc": "Image Loras",
                    },
                    "reference_images": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reference Images",
                        "short_desc": "Reference Images",
                    },
                    "disable_safety_checker": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Disable Safety Checker",
                        "short_desc": "If true, disables the safety checker for image generation",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-files",
                "method": "GET",
                "path": "/files",
                "display_name": "List Files",
                "short_desc": "List all files",
                "desc": "List the metadata for all uploaded data files.",
                "groups": ("Files",),
            },
            <RestSchemaActionInfo>{
                "action": "get-file",
                "method": "GET",
                "path": "/files/{id}",
                "display_name": "Get File",
                "short_desc": "Retrieve file metadata",
                "desc": "Retrieve the metadata for a single uploaded data file.",
                "groups": ("Files",),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "files",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-file",
                "method": "DELETE",
                "path": "/files/{id}",
                "display_name": "Delete File",
                "short_desc": "Delete a file",
                "desc": "Delete a previously uploaded data file. **Deletion is permanent.** Check the selected resource before running this action.",
                "groups": ("Files",),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "files",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "download-file",
                "method": "GET",
                "path": "/files/{id}/content",
                "display_name": "Download File",
                "short_desc": "Get file contents",
                "desc": "Get the contents of a single uploaded data file.",
                "groups": ("Files",),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "files",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "upload-file",
                "method": "POST",
                "path": "/files/upload",
                "display_name": "Upload File",
                "short_desc": "Upload a file",
                "desc": "Upload a file with specified purpose, file name, and file type.",
                "groups": ("Files",),
                "fields": {
                    "purpose": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Purpose",
                        "short_desc": "The purpose of the file",
                        "preselected": True,
                        "required": True,
                        "example_value": "fine-tune",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "fine-tune",
                            "display_name": "Fine Tune",
                        }, <AllowedValueInfo>{
                            "value": "eval",
                            "display_name": "Eval",
                        }, <AllowedValueInfo>{
                            "value": "batch-api",
                            "display_name": "Batch API",
                        },),
                    },
                    "file_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File Name",
                        "short_desc": "The name of the file being uploaded",
                        "preselected": True,
                        "required": True,
                        "example_value": "dataset.csv",
                    },
                    "file_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File Type",
                        "short_desc": "The type of the file",
                        "example_value": "jsonl",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "csv",
                            "display_name": "Csv",
                        }, <AllowedValueInfo>{
                            "value": "jsonl",
                            "display_name": "Jsonl",
                        }, <AllowedValueInfo>{
                            "value": "parquet",
                            "display_name": "Parquet",
                        },),
                    },
                    "file": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File",
                        "short_desc": "The content of the file being uploaded",
                        "preselected": True,
                        "required": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-fine-tune",
                "method": "POST",
                "path": "/fine-tunes",
                "display_name": "Create Fine-Tune",
                "short_desc": "Create job",
                "desc": "Create a fine-tuning job with the provided model and training data. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Fine-Tuning",),
                "fields": {
                    "training_file": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Training File",
                        "short_desc": "File-ID of a training file uploaded to the Together API",
                        "preselected": True,
                        "required": True,
                        "ref_data": "files",
                        "supports_custom_values": True,
                    },
                    "validation_file": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Validation File",
                        "short_desc": "File-ID of a validation file uploaded to the Together API",
                        "ref_data": "files",
                        "supports_custom_values": True,
                    },
                    "packing": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Packing",
                        "short_desc": "Whether to use sequence packing for training",
                    },
                    "max_seq_length": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Max Seq Length",
                        "short_desc": "Maximum sequence length to use for training",
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Name of the base model to run fine-tune job on",
                        "preselected": True,
                        "required": True,
                        "ref_data": "fine-tune-models",
                        "supports_custom_values": True,
                    },
                    "n_epochs": <RestSchemaFieldOverlayInfo>{
                        "display_name": "N Epochs",
                        "short_desc": "N Epochs",
                    },
                    "n_checkpoints": <RestSchemaFieldOverlayInfo>{
                        "display_name": "N Checkpoints",
                        "short_desc": "Number of intermediate model versions saved during training for evaluation",
                    },
                    "n_evals": <RestSchemaFieldOverlayInfo>{
                        "display_name": "N Evals",
                        "short_desc": "Number of evaluations to be run on a given validation set during training",
                    },
                    "batch_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Batch Size",
                        "short_desc": "Batch Size",
                    },
                    "gradient_accumulation_steps": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Gradient Accumulation Steps",
                        "short_desc": "Number of steps to accumulate gradients before performing a weight update",
                    },
                    "learning_rate": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Learning Rate",
                        "short_desc": "Learning Rate",
                    },
                    "lr_scheduler": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Lr Scheduler",
                        "short_desc": "The learning rate scheduler to use",
                        "fields": {
                            "lr_scheduler_type": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (<AllowedValueInfo>{
                                    "value": "linear",
                                    "display_name": "Linear",
                                }, <AllowedValueInfo>{
                                    "value": "cosine",
                                    "display_name": "Cosine",
                                },),
                            },
                        },
                    },
                    "warmup_ratio": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Warmup Ratio",
                        "short_desc": "Warmup Ratio",
                    },
                    "max_grad_norm": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Max Grad Norm",
                        "short_desc": "Max gradient norm to be used for gradient clipping",
                    },
                    "weight_decay": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Weight Decay",
                        "short_desc": "Weight decay",
                    },
                    "random_seed": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Random Seed",
                        "short_desc": "Random seed for reproducible training",
                    },
                    "early_stopping_enabled": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Early Stopping Enabled",
                        "short_desc": "Whether to stop training early when validation loss stops improving",
                    },
                    "early_stopping_patience": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Early Stopping Patience",
                        "short_desc": "Early Stopping Patience",
                    },
                    "early_stopping_min_delta": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Early Stopping Min Delta",
                        "short_desc": "Early Stopping Min Delta",
                    },
                    "early_stopping_warmup_evals": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Early Stopping Warmup Evals",
                        "short_desc": "Number of initial evaluations excluded from the early-stopping decision",
                    },
                    "suffix": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Suffix",
                        "short_desc": "Suffix to add to your fine-tuned model name",
                    },
                    "wandb_api_key": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Wandb API Key",
                        "short_desc": "Integration key for tracking experiments and model metrics on W&B platform",
                        "sensitive": True,
                    },
                    "wandb_base_url": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Wandb Base URL",
                        "short_desc": "The base URL of a dedicated Weights & Biases instance",
                    },
                    "wandb_project_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Wandb Project Name",
                        "short_desc": "The Weights & Biases project for your run",
                    },
                    "wandb_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Wandb Name",
                        "short_desc": "The Weights & Biases name for your run",
                    },
                    "wandb_entity": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Wandb Entity",
                        "short_desc": "The Weights & Biases entity for your run",
                    },
                    "train_on_inputs": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Train On Inputs",
                        "short_desc": "Train On Inputs",
                    },
                    "training_method": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Training Method",
                        "short_desc": "The training method to use",
                    },
                    "training_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Training Type",
                        "short_desc": "The training type to use",
                    },
                    "multimodal_params": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Multimodal Params",
                        "short_desc": "Value for Multimodal Params",
                    },
                    "from_checkpoint": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From Checkpoint",
                        "short_desc": "The checkpoint identifier to continue training from a previous fine-tuning job",
                    },
                    "from_hf_model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From Hugging Face Model",
                        "short_desc": "The Hugging Face Hub repo to start training from",
                    },
                    "hf_model_revision": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Hugging Face Model Revision",
                        "short_desc": "The revision of the Hugging Face Hub model to continue training from",
                    },
                    "hf_api_token": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Hugging Face API Token",
                        "short_desc": "The API token for the Hugging Face Hub",
                        "sensitive": True,
                    },
                    "hf_output_repo_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Hugging Face Output Repo Name",
                        "short_desc": "The name of the Hugging Face repository to upload the fine-tuned model to",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-fine-tunes",
                "method": "GET",
                "path": "/fine-tunes",
                "display_name": "List Fine-Tunes",
                "short_desc": "List all jobs",
                "desc": "List the metadata for all fine-tuning jobs.",
                "groups": ("Fine-Tuning",),
            },
            <RestSchemaActionInfo>{
                "action": "estimate-fine-tune-price",
                "method": "POST",
                "mutates_state": False,
                "path": "/fine-tunes/estimate-price",
                "display_name": "Estimate Fine-Tune Price",
                "short_desc": "Estimate price",
                "desc": "Estimate the price of a fine-tuning job. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Fine-Tuning",),
                "fields": {
                    "training_file": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Training File",
                        "short_desc": "File-ID of a training file uploaded to the Together API",
                        "preselected": True,
                        "required": True,
                        "ref_data": "files",
                        "supports_custom_values": True,
                    },
                    "validation_file": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Validation File",
                        "short_desc": "File-ID of a validation file uploaded to the Together API",
                        "ref_data": "files",
                        "supports_custom_values": True,
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Name of the base model to run fine-tune job on",
                        "ref_data": "fine-tune-models",
                        "supports_custom_values": True,
                    },
                    "n_epochs": <RestSchemaFieldOverlayInfo>{
                        "display_name": "N Epochs",
                        "short_desc": "N Epochs",
                    },
                    "n_evals": <RestSchemaFieldOverlayInfo>{
                        "display_name": "N Evals",
                        "short_desc": "Number of evaluations to be run on a given validation set during training",
                    },
                    "training_method": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Training Method",
                        "short_desc": "The training method to use",
                    },
                    "training_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Training Type",
                        "short_desc": "The training type to use",
                    },
                    "from_checkpoint": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From Checkpoint",
                        "short_desc": "The checkpoint identifier to continue training from a previous fine-tuning job",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "preview-fine-tune-data",
                "method": "POST",
                "mutates_state": False,
                "path": "/fine-tunes/preview",
                "display_name": "Preview Fine-Tune Data",
                "short_desc": "Preview tokenized data",
                "desc": "Preview how sampled rows from a fine-tuning training file will be tokenized before packing. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Fine-Tuning",),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Name of the base model whose tokenizer and chat template will be used",
                        "preselected": True,
                        "required": True,
                        "ref_data": "fine-tune-models",
                        "supports_custom_values": True,
                    },
                    "training_file": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Training File",
                        "short_desc": "File-ID of the uploaded JSONL training file to sample for preview",
                        "preselected": True,
                        "required": True,
                        "ref_data": "files",
                        "supports_custom_values": True,
                    },
                    "training_method": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Training Method",
                        "short_desc": "Fine-tuning method to preview",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "sft",
                            "display_name": "SFT",
                        },),
                    },
                    "train_on_inputs": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Train On Inputs",
                        "short_desc": "Train On Inputs",
                    },
                    "top_k": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Top K",
                        "short_desc": "Maximum number of rows from the start of the training file to tokenize",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-fine-tune",
                "method": "GET",
                "path": "/fine-tunes/{id}",
                "display_name": "Get Fine-Tune",
                "short_desc": "List job",
                "desc": "List the metadata for a single fine-tuning job.",
                "groups": ("Fine-Tuning",),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "fine-tunes",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-fine-tune",
                "method": "DELETE",
                "path": "/fine-tunes/{id}",
                "display_name": "Delete Fine-Tune",
                "short_desc": "Delete a fine-tune job",
                "desc": "Delete a fine-tuning job. **Deletion is permanent.** Check the selected resource before running this action.",
                "groups": ("Fine-Tuning",),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "fine-tunes",
                        "supports_custom_values": True,
                    },
                    "force": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Force",
                        "short_desc": "Value for Force",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-fine-tune-events",
                "method": "GET",
                "path": "/fine-tunes/{id}/events",
                "display_name": "List Fine-Tune Events",
                "short_desc": "List job events",
                "desc": "List the events for a single fine-tuning job.",
                "groups": ("Fine-Tuning",),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "fine-tunes",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-fine-tune-checkpoints",
                "method": "GET",
                "path": "/fine-tunes/{id}/checkpoints",
                "display_name": "List Fine-Tune Checkpoints",
                "short_desc": "List checkpoints",
                "desc": "List the checkpoints for a single fine-tuning job.",
                "groups": ("Fine-Tuning",),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "fine-tunes",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "download-tokenized-dataset",
                "method": "GET",
                "path": "/fine-tunes/{id}/download-tokenized-dataset",
                "display_name": "Download Tokenized Dataset",
                "short_desc": "Download tokenized dataset",
                "desc": "Get a presigned URL for the tokenized dataset archive generated for a fine-tune job.",
                "groups": ("Fine-Tuning",),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "fine-tunes",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "download-model",
                "method": "GET",
                "path": "/finetune/download",
                "display_name": "Download Model",
                "short_desc": "Download model",
                "desc": "Receive a compressed fine-tuned model or checkpoint.",
                "groups": ("Fine-Tuning",),
                "fields": {
                    "ft_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Ft ID",
                        "short_desc": "Value for Ft ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "fine-tunes",
                        "supports_custom_values": True,
                    },
                    "checkpoint_step": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Checkpoint Step",
                        "short_desc": "Value for Checkpoint Step",
                    },
                    "checkpoint": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Checkpoint",
                        "short_desc": "Value for Checkpoint",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "merged",
                            "display_name": "Merged",
                        }, <AllowedValueInfo>{
                            "value": "adapter",
                            "display_name": "Adapter",
                        }, <AllowedValueInfo>{
                            "value": "model_output_path",
                            "display_name": "Model Output Path",
                        },),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "cancel-fine-tune",
                "method": "POST",
                "path": "/fine-tunes/{id}/cancel",
                "display_name": "Cancel Fine-Tune",
                "short_desc": "Cancel job",
                "desc": "Cancel a currently running fine-tuning job. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Fine-Tuning",),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "fine-tunes",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-fine-tune-metrics",
                "method": "GET",
                "path": "/fine-tunes/{id}/metrics",
                "display_name": "Get Fine-Tune Metrics",
                "short_desc": "Get metrics",
                "desc": "Retrieves recorded training metrics for a fine-tuning job in chronological order.",
                "groups": ("Fine-Tuning",),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "fine-tunes",
                        "supports_custom_values": True,
                    },
                    "global_step_from": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Global Step From",
                        "short_desc": "Return only metrics with global_step >= this value",
                    },
                    "global_step_to": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Global Step To",
                        "short_desc": "Return only metrics with global_step <= this value",
                    },
                    "logged_at_from": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Logged At From",
                        "short_desc": "Return only metrics logged at or after this ISO-8601 timestamp",
                    },
                    "logged_at_to": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Logged At To",
                        "short_desc": "Return only metrics logged at or before this ISO-8601 timestamp",
                    },
                    "resolution": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Resolution",
                        "short_desc": "Number of (uniformly sampled) train metrics to return",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-fine-tune-models",
                "method": "GET",
                "path": "/fine-tunes/models/supported",
                "display_name": "List Fine-Tune Models",
                "short_desc": "List supported models",
                "desc": "List models supported for fine-tuning.",
                "groups": ("Fine-Tuning",),
            },
            <RestSchemaActionInfo>{
                "action": "get-fine-tune-model-limits",
                "method": "GET",
                "path": "/fine-tunes/models/limits",
                "display_name": "Get Fine-Tune Model Limits",
                "short_desc": "Get model limits",
                "desc": "Get model limits for a specific fine-tuning model.",
                "groups": ("Fine-Tuning",),
                "fields": {
                    "model_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model Name",
                        "short_desc": "Value for Model Name",
                        "preselected": True,
                        "required": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "rerank",
                "method": "POST",
                "mutates_state": False,
                "path": "/rerank",
                "display_name": "Rerank Documents",
                "short_desc": "Create a rerank request",
                "desc": "Rerank a list of documents by relevance to a query. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Inference",),
                "operation_id": "rerank",
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "The model to be used for the rerank request",
                        "preselected": True,
                        "required": True,
                        "example_value": "Salesforce/Llama-Rank-V1",
                        "ref_data": "rerank-models",
                        "supports_custom_values": True,
                    },
                    "query": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Query",
                        "short_desc": "The search query to be used for ranking",
                        "preselected": True,
                        "required": True,
                        "example_value": "What animals can I find near Peru?",
                    },
                    "documents": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Documents",
                        "short_desc": "List of documents, which can be either strings or objects",
                        "preselected": True,
                        "required": True,
                        "example_value": ({
                            "title": "Llama",
                            "text": "The llama is a domesticated South American camelid, widely used as a meat and pack animal by Andean cultures since the pre-Columbian era.",
                        }, {
                            "title": "Panda",
                            "text": "The giant panda (Ailuropoda melanoleuca), also known as the panda bear or simply panda, is a bear species endemic to China.",
                        }, {
                            "title": "Guanaco",
                            "text": "The guanaco is a camelid native to South America, closely related to the llama. Guanacos are one of two wild South American camelids; the other species is the vicuña, which lives at higher elevations.",
                        }, {
                            "title": "Wild Bactrian camel",
                            "text": "The wild Bactrian camel (Camelus ferus) is an endangered species of camel endemic to Northwest China and southwestern Mongolia.",
                        },),
                    },
                    "top_n": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Top N",
                        "short_desc": "The number of top results to return",
                        "example_value": 2,
                    },
                    "return_documents": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Return Documents",
                        "short_desc": "Whether to return supplied documents with the response",
                        "example_value": True,
                    },
                    "rank_fields": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Rank Fields",
                        "short_desc": "List of keys in the JSON Object document to rank by",
                        "example_value": ("title", "text",),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "generate-speech",
                "method": "POST",
                "mutates_state": False,
                "path": "/audio/speech",
                "display_name": "Generate Speech",
                "short_desc": "Create audio generation request",
                "desc": "Generate audio from input text. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Inference",),
                "operation_id": "audio-speech",
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "The name of the model to query",
                        "preselected": True,
                        "required": True,
                        "example_value": "canopylabs/orpheus-3b-0.1-ft",
                        "ref_data": "speech-models",
                        "supports_custom_values": True,
                        "has_dependents": True,
                    },
                    "input": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Input",
                        "short_desc": "Input text to generate the audio for",
                        "preselected": True,
                        "required": True,
                    },
                    "voice": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Voice",
                        "short_desc": "The voice to use for generating the audio",
                        "preselected": True,
                        "required": True,
                        "ref_data": "voices",
                        "supports_custom_values": True,
                        "depends_on": ("model",),
                    },
                    "response_format": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Response Format",
                        "short_desc": "The format of audio output",
                        "preselected": True,
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "mp3",
                            "display_name": "Mp3",
                        }, <AllowedValueInfo>{
                            "value": "wav",
                            "display_name": "Wav",
                        }, <AllowedValueInfo>{
                            "value": "raw",
                            "display_name": "Raw",
                        },),
                    },
                    "language": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Language",
                        "short_desc": "Language or locale of input text",
                        "example_value": "en",
                    },
                    "response_encoding": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Response Encoding",
                        "short_desc": "Audio encoding of response",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "pcm_f32le",
                            "display_name": "Pcm F32le",
                        }, <AllowedValueInfo>{
                            "value": "pcm_s16le",
                            "display_name": "Pcm S16le",
                        }, <AllowedValueInfo>{
                            "value": "pcm_mulaw",
                            "display_name": "Pcm Mulaw",
                        }, <AllowedValueInfo>{
                            "value": "pcm_alaw",
                            "display_name": "Pcm Alaw",
                        },),
                    },
                    "sample_rate": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sample Rate",
                        "short_desc": "Sampling rate in Hz for the output audio",
                    },
                    "bit_rate": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Bit Rate",
                        "short_desc": "Bitrate of the MP3 audio output in bits per second",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": 32000,
                            "display_name": "32000",
                        }, <AllowedValueInfo>{
                            "value": 64000,
                            "display_name": "64000",
                        }, <AllowedValueInfo>{
                            "value": 96000,
                            "display_name": "96000",
                        }, <AllowedValueInfo>{
                            "value": 128000,
                            "display_name": "128000",
                        }, <AllowedValueInfo>{
                            "value": 192000,
                            "display_name": "192000",
                        },),
                    },
                    "stream": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "extra_params": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Extra Params",
                        "short_desc": "Extra Params",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "transcribe-audio",
                "method": "POST",
                "mutates_state": False,
                "path": "/audio/transcriptions",
                "display_name": "Transcribe Audio",
                "short_desc": "Create audio transcription request",
                "desc": "Transcribes audio into text. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Inference",),
                "operation_id": "audio-transcriptions",
                "fields": {
                    "file": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File",
                        "short_desc": "Audio file upload or public HTTP/HTTPS URL",
                        "preselected": True,
                        "required": True,
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Model to use for transcription",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "openai/whisper-large-v3",
                            "display_name": "Openai Whisper Large V3",
                        },),
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                    "language": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Language",
                        "short_desc": "Optional ISO 639-1 language code",
                        "example_value": "en",
                    },
                    "prompt": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Prompt",
                        "short_desc": "Optional text to bias decoding",
                    },
                    "response_format": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Response Format",
                        "short_desc": "The format of the response",
                        "preselected": True,
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "json",
                            "display_name": "JSON",
                        }, <AllowedValueInfo>{
                            "value": "verbose_json",
                            "display_name": "Verbose JSON",
                        },),
                    },
                    "temperature": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Temperature",
                        "short_desc": "Sampling temperature between 0.0 and 1.0",
                        "preselected": True,
                    },
                    "timestamp_granularities": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Timestamp Granularities",
                        "short_desc": "Controls level of timestamp detail in verbose_json",
                        "example_value": ("word", "segment",),
                    },
                    "diarize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Diarize",
                        "short_desc": "Whether to enable speaker diarization",
                    },
                    "min_speakers": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Min Speakers",
                        "short_desc": "Minimum number of speakers expected in the audio",
                    },
                    "max_speakers": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Max Speakers",
                        "short_desc": "Maximum number of speakers expected in the audio",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "translate-audio",
                "method": "POST",
                "mutates_state": False,
                "path": "/audio/translations",
                "display_name": "Translate Audio",
                "short_desc": "Create audio translation request",
                "desc": "Translates audio into English. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Inference",),
                "operation_id": "audio-translations",
                "fields": {
                    "file": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File",
                        "short_desc": "Audio file upload or public HTTP/HTTPS URL",
                        "preselected": True,
                        "required": True,
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Model to use for translation",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "openai/whisper-large-v3",
                            "display_name": "Openai Whisper Large V3",
                        },),
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                    "language": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Language",
                        "short_desc": "Target output language",
                        "example_value": "en",
                    },
                    "prompt": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Prompt",
                        "short_desc": "Optional text to bias decoding",
                    },
                    "response_format": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Response Format",
                        "short_desc": "The format of the response",
                        "preselected": True,
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "json",
                            "display_name": "JSON",
                        }, <AllowedValueInfo>{
                            "value": "verbose_json",
                            "display_name": "Verbose JSON",
                        },),
                    },
                    "temperature": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Temperature",
                        "short_desc": "Sampling temperature between 0.0 and 1.0",
                        "preselected": True,
                    },
                    "timestamp_granularities": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Timestamp Granularities",
                        "short_desc": "Controls level of timestamp detail in verbose_json",
                        "example_value": ("word", "segment",),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-gpu-clusters",
                "method": "GET",
                "path": "/compute/clusters",
                "display_name": "List GPU Clusters",
                "short_desc": "List all GPU clusters",
                "desc": "List all GPU clusters.",
                "groups": ("GPU Clusters",),
                "operation_id": "GPUClusterService_List",
                "fields": {
                    "projectId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Project ID",
                        "short_desc": "Value for Project ID",
                        "ref_data": "projects",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-gpu-cluster",
                "method": "POST",
                "path": "/compute/clusters",
                "display_name": "Create GPU Cluster",
                "short_desc": "Create a GPU cluster",
                "desc": "Create an Instant Cluster on Together's high-performance GPU clusters. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("GPU Clusters",),
                "operation_id": "GPUClusterService_Create",
                "fields": {
                    "cluster_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cluster Type",
                        "short_desc": "Type of cluster to create",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "KUBERNETES",
                            "display_name": "Kubernetes",
                        }, <AllowedValueInfo>{
                            "value": "SLURM",
                            "display_name": "Slurm",
                        },),
                    },
                    "region": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Region",
                        "short_desc": "Region to create the GPU cluster in",
                        "preselected": True,
                        "required": True,
                        "ref_data": "gpu-regions",
                        "supports_custom_values": True,
                        "has_dependents": True,
                    },
                    "gpu_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "GPU Type",
                        "short_desc": "Type of GPU to use in the cluster",
                        "preselected": True,
                        "required": True,
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "H100_SXM",
                            "display_name": "H100 Sxm",
                        }, <AllowedValueInfo>{
                            "value": "H200_SXM",
                            "display_name": "H200 Sxm",
                        }, <AllowedValueInfo>{
                            "value": "RTX_6000_PCI",
                            "display_name": "Rtx 6000 Pci",
                        }, <AllowedValueInfo>{
                            "value": "L40_PCIE",
                            "display_name": "L40 Pcie",
                        }, <AllowedValueInfo>{
                            "value": "B200_SXM",
                            "display_name": "B200 Sxm",
                        }, <AllowedValueInfo>{
                            "value": "H100_SXM_INF",
                            "display_name": "H100 Sxm Inf",
                        }, <AllowedValueInfo>{
                            "value": "B300_SXM",
                            "display_name": "B300 Sxm",
                        },),
                    },
                    "num_gpus": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Num Gpus",
                        "short_desc": "Number of GPUs to allocate in the cluster",
                        "preselected": True,
                        "required": True,
                    },
                    "cluster_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cluster Name",
                        "short_desc": "Name of the GPU cluster",
                        "preselected": True,
                        "required": True,
                    },
                    "duration_days": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Duration Days",
                        "short_desc": "Duration in days to keep the cluster running",
                    },
                    "shared_volume": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Shared Volume",
                        "short_desc": "Inline configuration to create a shared volume with the cluster creation",
                        "fields": {
                            "volume_name": <RestSchemaFieldOverlayInfo>{
                                "ref_data": "project-volumes",
                                "supports_custom_values": True,
                            },
                            "region": <RestSchemaFieldOverlayInfo>{
                                "ref_data": "gpu-regions",
                                "supports_custom_values": True,
                            },
                            "project_id": <RestSchemaFieldOverlayInfo>{
                                "ref_data": "projects",
                                "supports_custom_values": True,
                            },
                        },
                    },
                    "volume_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Volume ID",
                        "short_desc": "ID of an existing volume to use with the cluster creation",
                        "ref_data": "shared-volumes",
                        "supports_custom_values": True,
                    },
                    "billing_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Billing Type",
                        "short_desc": "Billing Type",
                        "preselected": True,
                        "required": True,
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "RESERVED",
                            "display_name": "Reserved",
                        }, <AllowedValueInfo>{
                            "value": "ON_DEMAND",
                            "display_name": "On Demand",
                        }, <AllowedValueInfo>{
                            "value": "SCHEDULED_CAPACITY",
                            "display_name": "Scheduled Capacity",
                        },),
                    },
                    "auto_scaled": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Auto Scaled",
                        "short_desc": "Whether GPU cluster should be auto-scaled based on the workload",
                    },
                    "auto_scale_max_gpus": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Auto Scale Max Gpus",
                        "short_desc": "Maximum number of GPUs to which the cluster can be auto-scaled up",
                    },
                    "slurm_shm_size_gib": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Slurm Shm Size Gib",
                        "short_desc": "Shared memory size in GiB for Slurm cluster",
                    },
                    "capacity_pool_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Capacity Pool ID",
                        "short_desc": "ID of the capacity pool to use for the cluster",
                    },
                    "reservation_start_time": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reservation Start Time",
                        "short_desc": "Reservation start time of the cluster",
                    },
                    "reservation_end_time": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reservation End Time",
                        "short_desc": "Reservation end time of the cluster",
                    },
                    "install_traefik": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Install Traefik",
                        "short_desc": "Whether to install Traefik ingress controller in the cluster",
                    },
                    "cuda_version": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cuda Version",
                        "short_desc": "Legacy CUDA selector for this cluster",
                        "ref_data": "cuda-version",
                        "supports_custom_values": True,
                        "depends_on": ("region",),
                    },
                    "nvidia_driver_version": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Nvidia Driver Version",
                        "short_desc": "Legacy NVIDIA driver selector for this cluster",
                        "ref_data": "nvidia-driver-version",
                        "supports_custom_values": True,
                        "depends_on": ("region",),
                    },
                    "nvidia_version_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Nvidia Version ID",
                        "short_desc": "Canonical region-specific NVIDIA version ID",
                        "ref_data": "nvidia-version-id",
                        "supports_custom_values": True,
                        "depends_on": ("region",),
                    },
                    "slurm_image": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Slurm Image",
                        "short_desc": "Custom Slurm image for Slurm clusters",
                    },
                    "oidc_config": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Oidc Config",
                        "short_desc": "Value for Oidc Config",
                    },
                    "project_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Project ID",
                        "short_desc": "Project ID for the cluster",
                        "ref_data": "projects",
                        "supports_custom_values": True,
                    },
                    "acceptance_tests_params": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Acceptance Tests Params",
                        "short_desc": "Acceptance Tests Params",
                        "fields": {
                            "dcgm_diag_level": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (<AllowedValueInfo>{
                                    "value": "DCGM_DIAG_LEVEL_SHORT",
                                    "display_name": "Dcgm Diag Level Short",
                                }, <AllowedValueInfo>{
                                    "value": "DCGM_DIAG_LEVEL_MEDIUM",
                                    "display_name": "Dcgm Diag Level Medium",
                                }, <AllowedValueInfo>{
                                    "value": "DCGM_DIAG_LEVEL_LONG",
                                    "display_name": "Dcgm Diag Level Long",
                                }, <AllowedValueInfo>{
                                    "value": "DCGM_DIAG_LEVEL_EXTENDED",
                                    "display_name": "Dcgm Diag Level Extended",
                                },),
                            },
                        },
                    },
                    "cluster_config": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cluster Config",
                        "short_desc": "Value for Cluster Config",
                        "fields": {
                            "load_balancer": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (<AllowedValueInfo>{
                                    "value": "NONE",
                                    "display_name": "None",
                                }, <AllowedValueInfo>{
                                    "value": "TRAEFIK",
                                    "display_name": "Traefik",
                                }, <AllowedValueInfo>{
                                    "value": "NGINX",
                                    "display_name": "Nginx",
                                }, <AllowedValueInfo>{
                                    "value": "ISTIO",
                                    "display_name": "Istio",
                                },),
                            },
                        },
                    },
                    "num_capacity_pool_gpus": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Num Capacity Pool Gpus",
                        "short_desc": "Number of GPUs to allocate from the capacity pool",
                    },
                    "auto_scale": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Auto Scale",
                        "short_desc": "Whether to enable auto-scaling for the cluster",
                    },
                    "num_preemptible_gpus": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Num Preemptible Gpus",
                        "short_desc": "Number of preemptible GPUs to request alongside on-demand capacity",
                    },
                    "num_reserved_gpus": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Num Reserved Gpus",
                        "short_desc": "Number of prepaid (PLG) reserved GPUs for this cluster",
                    },
                    "add_ons": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Add Ons",
                        "short_desc": "Add-ons to enable on the cluster at creation time",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-gpu-cluster-by-cluster-id",
                "method": "GET",
                "path": "/compute/clusters/{cluster_id}",
                "display_name": "Get GPU Cluster By Cluster ID",
                "short_desc": "Get GPU cluster by cluster ID",
                "desc": "Retrieve information about a specific GPU cluster.",
                "groups": ("GPU Clusters",),
                "operation_id": "GPUClusterService_Get",
                "fields": {
                    "cluster_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cluster ID",
                        "short_desc": "Value for Cluster ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "gpu-clusters",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-gpu-cluster",
                "method": "PUT",
                "path": "/compute/clusters/{cluster_id}",
                "display_name": "Update GPU Cluster",
                "short_desc": "Update a GPU cluster",
                "desc": "Update the configuration of an existing GPU cluster. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("GPU Clusters",),
                "operation_id": "GPUClusterService_Update",
                "fields": {
                    "cluster_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cluster Type",
                        "short_desc": "Type of cluster to update",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "KUBERNETES",
                            "display_name": "Kubernetes",
                        }, <AllowedValueInfo>{
                            "value": "SLURM",
                            "display_name": "Slurm",
                        },),
                    },
                    "num_gpus": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Num Gpus",
                        "short_desc": "Target GPU count for the cluster",
                    },
                    "reservation_end_time": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reservation End Time",
                        "short_desc": "Timestamp at which the cluster should be decommissioned",
                    },
                    "cluster_config": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cluster Config",
                        "short_desc": "Value for Cluster Config",
                        "fields": {
                            "load_balancer": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (<AllowedValueInfo>{
                                    "value": "NONE",
                                    "display_name": "None",
                                }, <AllowedValueInfo>{
                                    "value": "TRAEFIK",
                                    "display_name": "Traefik",
                                }, <AllowedValueInfo>{
                                    "value": "NGINX",
                                    "display_name": "Nginx",
                                }, <AllowedValueInfo>{
                                    "value": "ISTIO",
                                    "display_name": "Istio",
                                },),
                            },
                        },
                    },
                    "num_reserved_gpus": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Num Reserved Gpus",
                        "short_desc": "Number of reserved GPUs to update to",
                    },
                    "num_preemptible_gpus": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Num Preemptible Gpus",
                        "short_desc": "Updated desired number of preemptible GPUs for the cluster",
                    },
                    "num_capacity_pool_gpus": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Num Capacity Pool Gpus",
                        "short_desc": "Number of GPUs to draw from the cluster's capacity pool",
                    },
                    "add_ons": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Add Ons",
                        "short_desc": "Add-ons to update on the cluster",
                    },
                    "cluster_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cluster ID",
                        "short_desc": "Value for Cluster ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "gpu-clusters",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-gpu-cluster-by-cluster-id",
                "method": "DELETE",
                "path": "/compute/clusters/{cluster_id}",
                "display_name": "Delete GPU Cluster By Cluster ID",
                "short_desc": "Delete GPU cluster by cluster ID",
                "desc": "Delete a GPU cluster by cluster ID. **Deletion is permanent.** Check the selected resource before running this action.",
                "groups": ("GPU Clusters",),
                "operation_id": "GPUClusterService_Delete",
                "fields": {
                    "cluster_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cluster ID",
                        "short_desc": "Value for Cluster ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "gpu-clusters",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-regions-and-corresponding-supported-driver-versions",
                "method": "GET",
                "path": "/compute/regions",
                "display_name": "List Regions And Corresponding Supported Driver Versions",
                "short_desc": "List regions and corresponding supported driver versions",
                "desc": "List regions and corresponding supported driver versions.",
                "groups": ("GPU Clusters",),
                "operation_id": "RegionService_List",
            },
            <RestSchemaActionInfo>{
                "action": "list-shared-volumes",
                "method": "GET",
                "path": "/compute/clusters/storage/volumes",
                "display_name": "List Shared Volumes",
                "short_desc": "List all shared volumes",
                "desc": "List all shared volumes.",
                "groups": ("GPU Clusters",),
                "operation_id": "SharedVolumeService_List",
                "fields": {
                    "projectId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Project ID",
                        "short_desc": "Value for Project ID",
                        "ref_data": "projects",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-shared-volume",
                "method": "PUT",
                "path": "/compute/clusters/storage/volumes",
                "display_name": "Update Shared Volume",
                "short_desc": "Update a shared volume",
                "desc": "Update the configuration of an existing shared volume. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("GPU Clusters",),
                "operation_id": "SharedVolumeService_Update",
                "fields": {
                    "volume_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Volume ID",
                        "short_desc": "ID of the volume",
                        "preselected": True,
                        "required": True,
                        "ref_data": "shared-volumes",
                        "supports_custom_values": True,
                    },
                    "size_tib": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Size Tib",
                        "short_desc": "Size of the volume in TiB",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-shared-volume",
                "method": "POST",
                "path": "/compute/clusters/storage/volumes",
                "display_name": "Create Shared Volume",
                "short_desc": "Create a shared volume",
                "desc": "Instant Clusters supports long-lived, resizable in-DC shared storage with user data persistence. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("GPU Clusters",),
                "operation_id": "SharedVolumeService_Create",
                "fields": {
                    "volume_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Volume Name",
                        "short_desc": "User provided name of the volume",
                        "preselected": True,
                        "required": True,
                        "ref_data": "project-volumes",
                        "supports_custom_values": True,
                    },
                    "size_tib": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Size Tib",
                        "short_desc": "Volume size in whole tebibytes (TiB)",
                        "preselected": True,
                        "required": True,
                    },
                    "region": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Region",
                        "short_desc": "Region name",
                        "preselected": True,
                        "required": True,
                        "ref_data": "gpu-regions",
                        "supports_custom_values": True,
                    },
                    "is_lifecycle_independent": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Is Lifecycle Independent",
                        "short_desc": "Is Lifecycle Independent",
                    },
                    "project_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Project ID",
                        "short_desc": "Project ID that will own the volume",
                        "ref_data": "projects",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-shared-volume-by-id",
                "method": "GET",
                "path": "/compute/clusters/storage/volumes/{volume_id}",
                "display_name": "Get Shared Volume By ID",
                "short_desc": "Get a shared volume by ID",
                "desc": "Retrieve information about a specific shared volume.",
                "groups": ("GPU Clusters",),
                "operation_id": "SharedVolumeService_Get",
                "fields": {
                    "volume_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Volume ID",
                        "short_desc": "Value for Volume ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "shared-volumes",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-shared-volume-by-id",
                "method": "DELETE",
                "path": "/compute/clusters/storage/volumes/{volume_id}",
                "display_name": "Delete Shared Volume By ID",
                "short_desc": "Delete a shared volume by ID",
                "desc": "Delete a shared volume. **Deletion is permanent.** Check the selected resource before running this action.",
                "groups": ("GPU Clusters",),
                "operation_id": "SharedVolumeService_Delete",
                "fields": {
                    "volume_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Volume ID",
                        "short_desc": "Value for Volume ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "shared-volumes",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-available-availability-zones",
                "method": "GET",
                "path": "/clusters/availability-zones",
                "display_name": "List Available Availability Zones",
                "short_desc": "List all available availability zones",
                "desc": "List all available availability zones.",
                "groups": ("GPU Clusters",),
                "operation_id": "availabilityZones",
            },
            <RestSchemaActionInfo>{
                "action": "list-endpoints",
                "method": "GET",
                "path": "/endpoints",
                "display_name": "List Endpoints",
                "short_desc": "List all endpoints",
                "desc": "Returns a list of all endpoints associated with your account. This uses the v1 endpoint API. The Dedicated Inference actions manage the current project-scoped v2 resources.",
                "groups": ("Endpoints",),
                "operation_id": "listEndpoints",
                "fields": {
                    "type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Type",
                        "short_desc": "Value for Type",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "dedicated",
                            "display_name": "Dedicated",
                        }, <AllowedValueInfo>{
                            "value": "serverless",
                            "display_name": "Serverless",
                        },),
                    },
                    "usage_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Usage Type",
                        "short_desc": "Value for Usage Type",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "on-demand",
                            "display_name": "On Demand",
                        }, <AllowedValueInfo>{
                            "value": "reserved",
                            "display_name": "Reserved",
                        },),
                    },
                    "mine": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Mine",
                        "short_desc": "Value for Mine",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-dedicated-endpoint",
                "method": "POST",
                "path": "/endpoints",
                "display_name": "Create Dedicated Endpoint",
                "short_desc": "Create a dedicated endpoint",
                "desc": "Creates a new dedicated endpoint for serving models. **Usage charges may apply.** Review model, training, or resource settings before submitting. This uses the v1 endpoint API. The Dedicated Inference actions manage the current project-scoped v2 resources.",
                "groups": ("Endpoints",),
                "operation_id": "createEndpoint",
                "fields": {
                    "display_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Display Name",
                        "short_desc": "A human-readable name for the endpoint",
                        "example_value": "My Llama3 70b endpoint",
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "The model to deploy on this endpoint",
                        "preselected": True,
                        "required": True,
                        "example_value": "deepseek-ai/DeepSeek-R1",
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                    "hardware": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Hardware",
                        "short_desc": "The hardware configuration to use for this endpoint",
                        "preselected": True,
                        "required": True,
                        "example_value": "1x_nvidia_a100_80gb_sxm",
                        "ref_data": "hardware",
                        "supports_custom_values": True,
                    },
                    "autoscaling": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Autoscaling",
                        "short_desc": "Configuration for automatic scaling of the endpoint",
                        "preselected": True,
                        "required": True,
                    },
                    "disable_prompt_cache": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Disable Prompt Cache",
                        "short_desc": "This parameter is deprecated and no longer has any effect",
                    },
                    "disable_speculative_decoding": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Disable Speculative Decoding",
                        "short_desc": "Whether to disable speculative decoding for this endpoint",
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "display_name": "State",
                        "short_desc": "The desired state of the endpoint",
                        "example_value": "STARTED",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "STARTED",
                            "display_name": "Started",
                        }, <AllowedValueInfo>{
                            "value": "STOPPED",
                            "display_name": "Stopped",
                        },),
                    },
                    "inactive_timeout": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Inactive Timeout",
                        "short_desc": "Inactive Timeout",
                        "example_value": 60,
                    },
                    "availability_zone": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Availability Zone",
                        "short_desc": "Create the endpoint in a specified availability zone (e.g., us-central-4b)",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-endpoint",
                "method": "GET",
                "path": "/endpoints/{endpointId}",
                "display_name": "Get Endpoint",
                "short_desc": "Get endpoint by ID",
                "desc": "Retrieves details about a specific endpoint, including its current state, configuration, and scaling settings. This uses the v1 endpoint API. The Dedicated Inference actions manage the current project-scoped v2 resources.",
                "groups": ("Endpoints",),
                "operation_id": "getEndpoint",
                "fields": {
                    "endpointId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Endpoint ID",
                        "short_desc": "Value for Endpoint ID",
                        "preselected": True,
                        "required": True,
                        "example_value": "endpoint-d23901de-ef8f-44bf-b3e7-de9c1ca8f2d7",
                        "ref_data": "endpoints",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-endpoint",
                "method": "PATCH",
                "path": "/endpoints/{endpointId}",
                "display_name": "Update Endpoint",
                "short_desc": "Update endpoint, this can also be used to start or stop a dedicated endpoint",
                "desc": "Updates an existing endpoint's configuration. **Usage charges may apply.** Review model, training, or resource settings before submitting. This uses the v1 endpoint API. The Dedicated Inference actions manage the current project-scoped v2 resources.",
                "groups": ("Endpoints",),
                "operation_id": "updateEndpoint",
                "fields": {
                    "display_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Display Name",
                        "short_desc": "A human-readable name for the endpoint",
                        "example_value": "My Llama3 70b endpoint",
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "display_name": "State",
                        "short_desc": "The desired state of the endpoint",
                        "example_value": "STARTED",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "STARTED",
                            "display_name": "Started",
                        }, <AllowedValueInfo>{
                            "value": "STOPPED",
                            "display_name": "Stopped",
                        },),
                    },
                    "autoscaling": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Autoscaling",
                        "short_desc": "New autoscaling configuration for the endpoint",
                    },
                    "inactive_timeout": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Inactive Timeout",
                        "short_desc": "Inactive Timeout",
                        "example_value": 60,
                    },
                    "endpointId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Endpoint ID",
                        "short_desc": "Value for Endpoint ID",
                        "preselected": True,
                        "required": True,
                        "example_value": "endpoint-d23901de-ef8f-44bf-b3e7-de9c1ca8f2d7",
                        "ref_data": "endpoints",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-endpoint",
                "method": "DELETE",
                "path": "/endpoints/{endpointId}",
                "display_name": "Delete Endpoint",
                "short_desc": "Delete endpoint",
                "desc": "Permanently deletes an endpoint. **Deletion is permanent.** Check the selected resource before running this action. This uses the v1 endpoint API. The Dedicated Inference actions manage the current project-scoped v2 resources.",
                "groups": ("Endpoints",),
                "operation_id": "deleteEndpoint",
                "fields": {
                    "endpointId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Endpoint ID",
                        "short_desc": "Value for Endpoint ID",
                        "preselected": True,
                        "required": True,
                        "example_value": "endpoint-d23901de-ef8f-44bf-b3e7-de9c1ca8f2d7",
                        "ref_data": "endpoints",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "add-lora-adapter-to-endpoint",
                "method": "POST",
                "path": "/endpoints/{endpointId}/adapters",
                "display_name": "Add LoRA Adapter To Endpoint",
                "short_desc": "Add a LoRA adapter to an endpoint",
                "desc": "Adds a LoRA adapter model to a dedicated endpoint. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Endpoints",),
                "operation_id": "addAdapter",
                "fields": {
                    "model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model ID",
                        "short_desc": "Combined identifier in format \"endpoint_name:adapter_model_name\"",
                        "preselected": True,
                        "required": True,
                        "example_value": "username/Meta-Llama-3.1-8B-Instruct-def456:username/my-adapter-abc123",
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                    "endpointId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Endpoint ID",
                        "short_desc": "The endpoint ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "endpoints",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-adapters-on-endpoint",
                "method": "GET",
                "path": "/endpoints/{endpointId}/adapters",
                "display_name": "List Adapters On Endpoint",
                "short_desc": "List adapters on an endpoint",
                "desc": "Returns all LoRA adapters bound to the specified dedicated endpoint.",
                "groups": ("Endpoints",),
                "operation_id": "listAdapters",
                "fields": {
                    "endpointId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Endpoint ID",
                        "short_desc": "The endpoint ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "endpoints",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "remove-lora-adapter-from-endpoint",
                "method": "DELETE",
                "path": "/endpoints/{endpointId}/adapters",
                "display_name": "Remove LoRA Adapter From Endpoint",
                "short_desc": "Remove a LoRA adapter from an endpoint",
                "desc": "Removes the routing rule that binds an adapter to an endpoint. **Deletion is permanent.** Check the selected resource before running this action.",
                "groups": ("Endpoints",),
                "operation_id": "removeAdapter",
                "fields": {
                    "model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model ID",
                        "short_desc": "Combined identifier in format \"endpoint_name:adapter_model_name\"",
                        "preselected": True,
                        "required": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                    "endpointId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Endpoint ID",
                        "short_desc": "The endpoint ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "endpoints",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-hardware",
                "method": "GET",
                "path": "/hardware",
                "display_name": "List Hardware",
                "short_desc": "List available hardware configurations",
                "desc": "Returns a list of available hardware configurations for deploying models.",
                "groups": ("Endpoints",),
                "operation_id": "listHardware",
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Value for Model",
                        "example_value": "deepseek-ai/DeepSeek-R1",
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "execute-code",
                "method": "POST",
                "path": "/tci/execute",
                "display_name": "Execute Code",
                "short_desc": "Execute code",
                "desc": "Executes the given code snippet and returns the output. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Code Execution",),
                "operation_id": "tci/execute",
                "fields": {
                    "code": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Code",
                        "short_desc": "Code snippet to execute",
                        "preselected": True,
                        "required": True,
                        "example_value": "print('Hello, world!')",
                    },
                    "files": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Files",
                        "short_desc": "Files to upload to the session",
                        "fields": {
                            "encoding": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (<AllowedValueInfo>{
                                    "value": "string",
                                    "display_name": "String",
                                }, <AllowedValueInfo>{
                                    "value": "base64",
                                    "display_name": "Base64",
                                },),
                            },
                        },
                    },
                    "language": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Language",
                        "short_desc": "Programming language for the code to execute",
                        "preselected": True,
                        "required": True,
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "python",
                            "display_name": "Python",
                        },),
                    },
                    "session_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Session ID",
                        "short_desc": "Identifier of the current session",
                        "example_value": "ses_abcDEF123",
                        "ref_data": "code-sessions",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-active-sessions",
                "method": "GET",
                "path": "/tci/sessions",
                "display_name": "List Active Sessions",
                "short_desc": "List active sessions",
                "desc": "Lists all your currently active sessions.",
                "groups": ("Code Execution",),
                "operation_id": "sessions/list",
            },
            <RestSchemaActionInfo>{
                "action": "list-batches",
                "method": "GET",
                "path": "/batches",
                "display_name": "List Batches",
                "short_desc": "List batch jobs",
                "desc": "List all batch jobs for the authenticated user.",
                "groups": ("Batches",),
            },
            <RestSchemaActionInfo>{
                "action": "create-batch",
                "method": "POST",
                "path": "/batches",
                "display_name": "Create Batch",
                "short_desc": "Create a batch job",
                "desc": "Create a new batch job with the given input file and endpoint. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Batches",),
                "fields": {
                    "endpoint": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Endpoint",
                        "short_desc": "The endpoint to use for batch processing",
                        "preselected": True,
                        "required": True,
                        "example_value": "/v1/chat/completions",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "/v1/chat/completions",
                            "display_name": "V1 Chat Completions",
                        }, <AllowedValueInfo>{
                            "value": "/v1/audio/transcriptions",
                            "display_name": "V1 Audio Transcriptions",
                        }, <AllowedValueInfo>{
                            "value": "/v1/audio/translations",
                            "display_name": "V1 Audio Translations",
                        },),
                    },
                    "input_file_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Input File ID",
                        "short_desc": "ID of the uploaded input file containing batch requests",
                        "preselected": True,
                        "required": True,
                        "example_value": "file-abc123def456ghi789",
                        "ref_data": "files",
                        "supports_custom_values": True,
                    },
                    "completion_window": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Completion Window",
                        "short_desc": "Time window for batch completion (optional)",
                        "example_value": "24h",
                    },
                    "priority": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Priority",
                        "short_desc": "Priority for batch processing (optional)",
                        "example_value": 1,
                    },
                    "model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model ID",
                        "short_desc": "Model to use for processing batch requests",
                        "example_value": "Qwen/Qwen3.5-9B",
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-batch",
                "method": "GET",
                "path": "/batches/{id}",
                "display_name": "Get Batch",
                "short_desc": "Get a batch job",
                "desc": "Get details of a batch job by ID.",
                "groups": ("Batches",),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "example_value": "batch_job_abc123def456",
                        "ref_data": "batches",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "cancel-batch",
                "method": "POST",
                "path": "/batches/{id}/cancel",
                "display_name": "Cancel Batch",
                "short_desc": "Cancel a batch job",
                "desc": "Cancel a batch job by ID. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Batches",),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "example_value": "batch_job_abc123def456",
                        "ref_data": "batches",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-evaluation",
                "method": "POST",
                "path": "/evaluation",
                "display_name": "Create Evaluation",
                "short_desc": "Create an evaluation job",
                "desc": "Create an evaluation job. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Evaluation",),
                "operation_id": "createEvaluationJob",
                "fields": {
                    "type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Type",
                        "short_desc": "The type of evaluation to perform",
                        "preselected": True,
                        "required": True,
                        "example_value": "classify",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "classify",
                            "display_name": "Classify",
                        }, <AllowedValueInfo>{
                            "value": "score",
                            "display_name": "Score",
                        }, <AllowedValueInfo>{
                            "value": "compare",
                            "display_name": "Compare",
                        },),
                    },
                    "parameters": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Parameters",
                        "short_desc": "Type-specific parameters for the evaluation",
                        "preselected": True,
                        "required": True,
                        "fields": {
                            "judge": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "model": <RestSchemaFieldOverlayInfo>{
                                        "ref_data": "models",
                                        "supports_custom_values": True,
                                    },
                                    "model_source": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (<AllowedValueInfo>{
                                            "value": "serverless",
                                            "display_name": "Serverless",
                                        }, <AllowedValueInfo>{
                                            "value": "dedicated",
                                            "display_name": "Dedicated",
                                        }, <AllowedValueInfo>{
                                            "value": "external",
                                            "display_name": "External",
                                        },),
                                    },
                                    "external_api_token": <RestSchemaFieldOverlayInfo>{
                                        "sensitive": True,
                                    },
                                },
                            },
                            "model_to_evaluate": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "model": <RestSchemaFieldOverlayInfo>{
                                        "ref_data": "models",
                                        "supports_custom_values": True,
                                    },
                                    "model_source": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (<AllowedValueInfo>{
                                            "value": "serverless",
                                            "display_name": "Serverless",
                                        }, <AllowedValueInfo>{
                                            "value": "dedicated",
                                            "display_name": "Dedicated",
                                        }, <AllowedValueInfo>{
                                            "value": "external",
                                            "display_name": "External",
                                        },),
                                    },
                                    "external_api_token": <RestSchemaFieldOverlayInfo>{
                                        "sensitive": True,
                                    },
                                },
                            },
                            "input_data_file_path": <RestSchemaFieldOverlayInfo>{
                                "ref_data": "files",
                                "supports_custom_values": True,
                            },
                            "model_a": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "model": <RestSchemaFieldOverlayInfo>{
                                        "ref_data": "models",
                                        "supports_custom_values": True,
                                    },
                                    "model_source": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (<AllowedValueInfo>{
                                            "value": "serverless",
                                            "display_name": "Serverless",
                                        }, <AllowedValueInfo>{
                                            "value": "dedicated",
                                            "display_name": "Dedicated",
                                        }, <AllowedValueInfo>{
                                            "value": "external",
                                            "display_name": "External",
                                        },),
                                    },
                                    "external_api_token": <RestSchemaFieldOverlayInfo>{
                                        "sensitive": True,
                                    },
                                },
                            },
                            "model_b": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "model": <RestSchemaFieldOverlayInfo>{
                                        "ref_data": "models",
                                        "supports_custom_values": True,
                                    },
                                    "model_source": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (<AllowedValueInfo>{
                                            "value": "serverless",
                                            "display_name": "Serverless",
                                        }, <AllowedValueInfo>{
                                            "value": "dedicated",
                                            "display_name": "Dedicated",
                                        }, <AllowedValueInfo>{
                                            "value": "external",
                                            "display_name": "External",
                                        },),
                                    },
                                    "external_api_token": <RestSchemaFieldOverlayInfo>{
                                        "sensitive": True,
                                    },
                                },
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-evaluations",
                "method": "GET",
                "path": "/evaluation",
                "display_name": "List Evaluations",
                "short_desc": "Get all evaluation jobs",
                "desc": "Get all evaluation jobs.",
                "groups": ("Evaluation",),
                "operation_id": "getAllEvaluationJobs",
                "fields": {
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Value for Status",
                    },
                    "limit": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Limit",
                        "short_desc": "Value for Limit",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-evaluation-models",
                "method": "GET",
                "path": "/evaluation/model-list",
                "display_name": "List Evaluation Models",
                "short_desc": "Get model list",
                "desc": "Get model list.",
                "groups": ("Evaluation",),
                "operation_id": "getModelList",
                "fields": {
                    "model_source": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model Source",
                        "short_desc": "Value for Model Source",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-evaluation",
                "method": "GET",
                "path": "/evaluation/{id}",
                "display_name": "Get Evaluation",
                "short_desc": "Get evaluation job details",
                "desc": "Get evaluation job details.",
                "groups": ("Evaluation",),
                "operation_id": "getEvaluationJobDetails",
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "evaluations",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-evaluation-results",
                "method": "GET",
                "path": "/evaluation/{id}/status",
                "display_name": "Get Evaluation Results",
                "short_desc": "Get evaluation job status and results",
                "desc": "Get evaluation job status and results.",
                "groups": ("Evaluation",),
                "operation_id": "getEvaluationJobStatusAndResults",
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "evaluations",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "cancel-queued-job",
                "method": "POST",
                "path": "/queue/cancel",
                "display_name": "Cancel Queued Job",
                "short_desc": "Cancel a queued job",
                "desc": "Cancel a pending job. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Queue",),
                "operation_id": "cancelQueueJob",
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Model identifier the job was submitted to",
                        "preselected": True,
                        "required": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                    "request_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Request ID",
                        "short_desc": "The request ID returned from the submit endpoint",
                        "preselected": True,
                        "required": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "clear-model-s-pending-jobs",
                "method": "POST",
                "path": "/queue/clear",
                "display_name": "Clear Model's Pending Jobs",
                "short_desc": "Clear a model's pending jobs",
                "desc": "Cancel all pending jobs for the given model. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Queue",),
                "operation_id": "clearQueue",
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Model identifier whose pending jobs should be canceled",
                        "preselected": True,
                        "required": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-queue-metrics",
                "method": "GET",
                "path": "/queue/metrics",
                "display_name": "Get Queue Metrics",
                "short_desc": "Get queue metrics",
                "desc": "Get the current queue statistics for a model, including pending and running job counts.",
                "groups": ("Queue",),
                "operation_id": "getQueueMetrics",
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Value for Model",
                        "preselected": True,
                        "required": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-job-status",
                "method": "GET",
                "path": "/queue/status",
                "display_name": "Get Job Status",
                "short_desc": "Get job status",
                "desc": "Poll the current status of a previously submitted job.",
                "groups": ("Queue",),
                "operation_id": "getQueueJobStatus",
                "fields": {
                    "request_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Request ID",
                        "short_desc": "Value for Request ID",
                        "preselected": True,
                        "required": True,
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Value for Model",
                        "preselected": True,
                        "required": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "submit-queued-job",
                "method": "POST",
                "path": "/queue/submit",
                "display_name": "Submit Queued Job",
                "short_desc": "Submit a queued job",
                "desc": "Submit a new job to the queue for asynchronous processing. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Queue",),
                "operation_id": "submitQueueJob",
                "fields": {
                    "info": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Info",
                        "short_desc": "Arbitrary JSON metadata stored with the job",
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Required model identifier",
                        "preselected": True,
                        "required": True,
                        "example_value": "my-queue-model",
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                    "payload": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Payload",
                        "short_desc": "Freeform model input",
                        "preselected": True,
                        "required": True,
                    },
                    "priority": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Priority",
                        "short_desc": "Job priority",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-model-resources",
                "method": "GET",
                "path": "/rl/model-resources",
                "display_name": "List Model Resources",
                "short_desc": "List model resources",
                "desc": "Lists the caller's model resources.",
                "groups": ("Training",),
                "operation_id": "listModelResources",
                "fields": {
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Value for Status",
                    },
                    "limit": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Limit",
                        "short_desc": "Value for Limit",
                        "preselected": True,
                    },
                    "after": <RestSchemaFieldOverlayInfo>{
                        "display_name": "After",
                        "short_desc": "Value for After",
                    },
                    "created_by": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Created By",
                        "short_desc": "Value for Created By",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-model-resources",
                "method": "POST",
                "path": "/rl/model-resources",
                "display_name": "Create Model Resources",
                "short_desc": "Create model resources",
                "desc": "Provisions a standalone model resource that training sessions can attach to. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Training",),
                "operation_id": "createModelResources",
                "fields": {
                    "base_model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Base Model",
                        "short_desc": "Base model to provision the resource for",
                        "preselected": True,
                        "required": True,
                        "example_value": "Qwen/Qwen3-0.6B",
                    },
                    "compute_config": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Compute Config",
                        "short_desc": "Compute layout to provision",
                        "fields": {
                            "gpu_type": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (<AllowedValueInfo>{
                                    "value": "H100-80GB",
                                    "display_name": "H100 80 Gb",
                                }, <AllowedValueInfo>{
                                    "value": "B200-SXM",
                                    "display_name": "B200 Sxm",
                                },),
                            },
                        },
                    },
                    "lora_enabled": <RestSchemaFieldOverlayInfo>{
                        "display_name": "LoRA Enabled",
                        "short_desc": "Whether the resource hosts LoRA sessions or a single full-weight session",
                        "example_value": True,
                    },
                    "optimizer_config": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Optimizer Config",
                        "short_desc": "Optimizer configuration for this resource",
                        "fields": {
                            "muon": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "scaling_strategy": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (<AllowedValueInfo>{
                                            "value": "MUON_SCALING_STRATEGY_UNSPECIFIED",
                                            "display_name": "Muon Scaling Strategy Unspecified",
                                        }, <AllowedValueInfo>{
                                            "value": "MUON_SCALING_STRATEGY_MATCH_ADAMW",
                                            "display_name": "Muon Scaling Strategy Match Adamw",
                                        }, <AllowedValueInfo>{
                                            "value": "MUON_SCALING_STRATEGY_ORIGINAL",
                                            "display_name": "Muon Scaling Strategy Original",
                                        },),
                                    },
                                },
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "estimate-on-demand-model-resource-cost",
                "method": "POST",
                "mutates_state": False,
                "path": "/rl/model-resources/estimate-cost",
                "display_name": "Estimate On Demand Model Resource Cost",
                "short_desc": "Estimate on-demand model resource cost",
                "desc": "Estimates a model resource's on-demand hourly price without creating it. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Training",),
                "operation_id": "estimateModelResourcesCost",
                "fields": {
                    "base_model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Base Model",
                        "short_desc": "Base model to provision the resource for",
                        "preselected": True,
                        "required": True,
                        "example_value": "Qwen/Qwen3-0.6B",
                    },
                    "compute_config": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Compute Config",
                        "short_desc": "Compute layout to provision",
                        "fields": {
                            "gpu_type": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (<AllowedValueInfo>{
                                    "value": "H100-80GB",
                                    "display_name": "H100 80 Gb",
                                }, <AllowedValueInfo>{
                                    "value": "B200-SXM",
                                    "display_name": "B200 Sxm",
                                },),
                            },
                        },
                    },
                    "lora_enabled": <RestSchemaFieldOverlayInfo>{
                        "display_name": "LoRA Enabled",
                        "short_desc": "Whether the resource hosts LoRA sessions or a single full-weight session",
                        "example_value": True,
                    },
                    "optimizer_config": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Optimizer Config",
                        "short_desc": "Optimizer configuration for this resource",
                        "fields": {
                            "muon": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "scaling_strategy": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (<AllowedValueInfo>{
                                            "value": "MUON_SCALING_STRATEGY_UNSPECIFIED",
                                            "display_name": "Muon Scaling Strategy Unspecified",
                                        }, <AllowedValueInfo>{
                                            "value": "MUON_SCALING_STRATEGY_MATCH_ADAMW",
                                            "display_name": "Muon Scaling Strategy Match Adamw",
                                        }, <AllowedValueInfo>{
                                            "value": "MUON_SCALING_STRATEGY_ORIGINAL",
                                            "display_name": "Muon Scaling Strategy Original",
                                        },),
                                    },
                                },
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-model-resources",
                "method": "GET",
                "path": "/rl/model-resources/{model_resources_id}",
                "display_name": "Get Model Resources",
                "short_desc": "Get model resources",
                "desc": "Gets a model resource by its ID and returns its details.",
                "groups": ("Training",),
                "operation_id": "getModelResources",
                "fields": {
                    "model_resources_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model Resources ID",
                        "short_desc": "Value for Model Resources ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "model-resources",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "stop-model-resources",
                "method": "POST",
                "path": "/rl/model-resources/{model_resources_id}/stop",
                "display_name": "Stop Model Resources",
                "short_desc": "Stop model resources",
                "desc": "Stops every session attached to the resource and tears down its GPU pods. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Training",),
                "operation_id": "stopModelResources",
                "fields": {
                    "model_resources_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model Resources ID",
                        "short_desc": "Value for Model Resources ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "model-resources",
                        "supports_custom_values": True,
                    },
                    "force": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Force",
                        "short_desc": "Value for Force",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-training-sessions",
                "method": "GET",
                "path": "/rl/training-sessions",
                "display_name": "List Training Sessions",
                "short_desc": "List training sessions",
                "desc": "Lists all training sessions.",
                "groups": ("Training",),
                "operation_id": "listTrainingSessions",
                "fields": {
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Value for Status",
                    },
                    "limit": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Limit",
                        "short_desc": "Value for Limit",
                        "preselected": True,
                    },
                    "after": <RestSchemaFieldOverlayInfo>{
                        "display_name": "After",
                        "short_desc": "Value for After",
                    },
                    "model_resources_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model Resources ID",
                        "short_desc": "Value for Model Resources ID",
                        "ref_data": "model-resources",
                        "supports_custom_values": True,
                    },
                    "created_by": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Created By",
                        "short_desc": "Value for Created By",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-training-session",
                "method": "POST",
                "path": "/rl/training-sessions",
                "display_name": "Create Training Session",
                "short_desc": "Create training session",
                "desc": "Creates a training session and returns its details. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Training",),
                "operation_id": "startTrainingSession",
                "fields": {
                    "resume_from_checkpoint_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Resume From Checkpoint ID",
                        "short_desc": "Checkpoint ID to resume from",
                        "example_value": "123e4567-e89b-12d3-a456-426614174000",
                    },
                    "resume_from_hf_checkpoint": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Resume From Hugging Face Checkpoint",
                        "short_desc": "HuggingFace repo (or hf://) to resume model weights from",
                        "example_value": "your-org/llama-3-8b-finetuned",
                    },
                    "lora_config": <RestSchemaFieldOverlayInfo>{
                        "display_name": "LoRA Config",
                        "short_desc": "LoRA adapter configuration for the session",
                    },
                    "model_resources_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model Resources ID",
                        "short_desc": "Model resource to attach the session to",
                        "preselected": True,
                        "required": True,
                        "example_value": "123e4567-e89b-12d3-a456-426614174000",
                        "ref_data": "model-resources",
                        "supports_custom_values": True,
                    },
                    "display_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Display Name",
                        "short_desc": "Optional display name used to identify the training session",
                        "example_value": "gsm8k-experiment-2",
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "Optional auxiliary metadata to associate with the training session",
                    },
                    "load_optimizer": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Load Optimizer",
                        "short_desc": "Whether to restore optimizer state and step from a training checkpoint",
                        "example_value": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-supported-models",
                "method": "GET",
                "path": "/rl/supported-models",
                "display_name": "List Supported Models",
                "short_desc": "List supported models",
                "desc": "Returns the models supported by the RL service and their limits for training/sampling operations.",
                "groups": ("Training",),
                "operation_id": "listSupportedModels",
            },
            <RestSchemaActionInfo>{
                "action": "get-training-session",
                "method": "GET",
                "path": "/rl/training-sessions/{session_id}",
                "display_name": "Get Training Session",
                "short_desc": "Get training session",
                "desc": "Gets a training session by its ID and returns its details.",
                "groups": ("Training",),
                "operation_id": "getTrainingSession",
                "fields": {
                    "session_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Session ID",
                        "short_desc": "Value for Session ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "training-sessions",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-training-session",
                "method": "PATCH",
                "path": "/rl/training-sessions/{session_id}",
                "display_name": "Update Training Session",
                "short_desc": "Update a training session",
                "desc": "Updates the display name or metadata associated with a training session. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Training",),
                "operation_id": "updateTrainingSession",
                "fields": {
                    "display_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Display Name",
                        "short_desc": "Display name to update",
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "Metadata fields to update",
                    },
                    "session_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Session ID",
                        "short_desc": "Value for Session ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "training-sessions",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "stop-training-session",
                "method": "POST",
                "path": "/rl/training-sessions/{session_id}/stop",
                "display_name": "Stop Training Session",
                "short_desc": "Stop training session",
                "desc": "Stops a training session. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Training",),
                "operation_id": "stopTrainingSession",
                "fields": {
                    "session_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Session ID",
                        "short_desc": "Value for Session ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "training-sessions",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-forward-backward-operation",
                "method": "GET",
                "path": "/rl/training-sessions/{session_id}/operations/forward-backward/{operation_id}",
                "display_name": "Get Forward Backward Operation",
                "short_desc": "Get forward-backward operation",
                "desc": "Retrieves the current status and result of a forward-backward operation.",
                "groups": ("Training",),
                "operation_id": "getForwardBackwardOperation",
                "fields": {
                    "session_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Session ID",
                        "short_desc": "Value for Session ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "training-sessions",
                        "supports_custom_values": True,
                    },
                    "operation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Operation ID",
                        "short_desc": "Value for Operation ID",
                        "preselected": True,
                        "required": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-optim-step-operation",
                "method": "GET",
                "path": "/rl/training-sessions/{session_id}/operations/optim-step/{operation_id}",
                "display_name": "Get Optim Step Operation",
                "short_desc": "Get optim-step operation",
                "desc": "Retrieves the current status and result of an optim-step operation.",
                "groups": ("Training",),
                "operation_id": "getOptimStepOperation",
                "fields": {
                    "session_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Session ID",
                        "short_desc": "Value for Session ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "training-sessions",
                        "supports_custom_values": True,
                    },
                    "operation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Operation ID",
                        "short_desc": "Value for Operation ID",
                        "preselected": True,
                        "required": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-weights-sync-operation",
                "method": "GET",
                "path": "/rl/training-sessions/{session_id}/operations/weights-sync/{operation_id}",
                "display_name": "Get Weights Sync Operation",
                "short_desc": "Get weights-sync operation",
                "desc": "Retrieves the current status and result of a weights-sync operation.",
                "groups": ("Training",),
                "operation_id": "getWeightsSyncOperation",
                "fields": {
                    "session_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Session ID",
                        "short_desc": "Value for Session ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "training-sessions",
                        "supports_custom_values": True,
                    },
                    "operation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Operation ID",
                        "short_desc": "Value for Operation ID",
                        "preselected": True,
                        "required": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-sample-operation",
                "method": "GET",
                "path": "/rl/training-sessions/{session_id}/operations/sample/{operation_id}",
                "display_name": "Get Sample Operation",
                "short_desc": "Get sample operation",
                "desc": "Retrieves the current status and result of a sample operation.",
                "groups": ("Training",),
                "operation_id": "GetSample",
                "fields": {
                    "session_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Session ID",
                        "short_desc": "Value for Session ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "training-sessions",
                        "supports_custom_values": True,
                    },
                    "operation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Operation ID",
                        "short_desc": "Value for Operation ID",
                        "preselected": True,
                        "required": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "forward-backward-pass",
                "method": "POST",
                "path": "/rl/training-sessions/{session_id}/operations/forward-backward",
                "display_name": "Forward Backward Pass",
                "short_desc": "Forward-backward pass",
                "desc": "Submits a forward-backward pass operation that will asynchronously compute gradients via backpropagation. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Training",),
                "operation_id": "forwardBackward",
                "fields": {
                    "samples": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Samples",
                        "short_desc": "Batch of training samples to process",
                        "preselected": True,
                        "required": True,
                    },
                    "loss": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Loss",
                        "short_desc": "Loss function configuration",
                        "preselected": True,
                        "required": True,
                        "fields": {
                            "type": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (<AllowedValueInfo>{
                                    "value": "LOSS_TYPE_UNSPECIFIED",
                                    "display_name": "Loss Type Unspecified",
                                }, <AllowedValueInfo>{
                                    "value": "LOSS_TYPE_CROSS_ENTROPY",
                                    "display_name": "Loss Type Cross Entropy",
                                }, <AllowedValueInfo>{
                                    "value": "LOSS_TYPE_GRPO",
                                    "display_name": "Loss Type Grpo",
                                }, <AllowedValueInfo>{
                                    "value": "LOSS_TYPE_IMPORTANCE_SAMPLING",
                                    "display_name": "Loss Type Importance Sampling",
                                }, <AllowedValueInfo>{
                                    "value": "LOSS_TYPE_PPO",
                                    "display_name": "Loss Type Ppo",
                                }, <AllowedValueInfo>{
                                    "value": "LOSS_TYPE_CISPO",
                                    "display_name": "Loss Type Cispo",
                                }, <AllowedValueInfo>{
                                    "value": "LOSS_TYPE_DRO",
                                    "display_name": "Loss Type Dro",
                                },),
                            },
                            "grpo_params": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "agg_type": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (<AllowedValueInfo>{
                                            "value": "GRPO_LOSS_AGGREGATION_TYPE_UNSPECIFIED",
                                            "display_name": "Grpo Loss Aggregation Type Unspecified",
                                        }, <AllowedValueInfo>{
                                            "value": "GRPO_LOSS_AGGREGATION_TYPE_FIXED_HORIZON",
                                            "display_name": "Grpo Loss Aggregation Type Fixed Horizon",
                                        }, <AllowedValueInfo>{
                                            "value": "GRPO_LOSS_AGGREGATION_TYPE_TOKEN_MEAN",
                                            "display_name": "Grpo Loss Aggregation Type Token Mean",
                                        }, <AllowedValueInfo>{
                                            "value": "GRPO_LOSS_AGGREGATION_TYPE_SEQUENCE_MEAN",
                                            "display_name": "Grpo Loss Aggregation Type Sequence Mean",
                                        },),
                                    },
                                    "ratio_type": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (<AllowedValueInfo>{
                                            "value": "GRPO_LOSS_RATIO_TYPE_TOKEN",
                                            "display_name": "Grpo Loss Ratio Type Token",
                                        }, <AllowedValueInfo>{
                                            "value": "GRPO_LOSS_RATIO_TYPE_SEQUENCE",
                                            "display_name": "Grpo Loss Ratio Type Sequence",
                                        },),
                                    },
                                },
                            },
                        },
                    },
                    "session_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Session ID",
                        "short_desc": "Value for Session ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "training-sessions",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "custom-forward-backward-pass",
                "method": "POST",
                "path": "/rl/training-sessions/{session_id}/operations/custom-forward-backward",
                "display_name": "Custom Forward Backward Pass",
                "short_desc": "Custom forward-backward pass",
                "desc": "Submits a forward-backward pass driven by externally computed gradients of the loss with respect to per-token log-probabilities. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Training",),
                "operation_id": "customForwardBackward",
                "fields": {
                    "samples": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Samples",
                        "short_desc": "Batch of training samples",
                        "preselected": True,
                        "required": True,
                    },
                    "gradients": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Gradients",
                        "short_desc": "Per-sample per-token gradients of the loss with respect to log-probabilities",
                        "preselected": True,
                        "required": True,
                        "fields": {
                            "dtype": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (<AllowedValueInfo>{
                                    "value": "D_TYPE_UNSPECIFIED",
                                    "display_name": "D Type Unspecified",
                                }, <AllowedValueInfo>{
                                    "value": "D_TYPE_INT64",
                                    "display_name": "D Type Int64",
                                }, <AllowedValueInfo>{
                                    "value": "D_TYPE_FLOAT32",
                                    "display_name": "D Type Float32",
                                }, <AllowedValueInfo>{
                                    "value": "D_TYPE_BFLOAT16",
                                    "display_name": "D Type Bfloat16",
                                },),
                            },
                        },
                    },
                    "session_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Session ID",
                        "short_desc": "Value for Session ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "training-sessions",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-custom-forward-backward-operation",
                "method": "GET",
                "path": "/rl/training-sessions/{session_id}/operations/custom-forward-backward/{operation_id}",
                "display_name": "Get Custom Forward Backward Operation",
                "short_desc": "Get custom forward-backward operation",
                "desc": "Retrieves the current status and result of a custom forward-backward operation.",
                "groups": ("Training",),
                "operation_id": "getCustomForwardBackwardOperation",
                "fields": {
                    "session_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Session ID",
                        "short_desc": "Value for Session ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "training-sessions",
                        "supports_custom_values": True,
                    },
                    "operation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Operation ID",
                        "short_desc": "Value for Operation ID",
                        "preselected": True,
                        "required": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "forward-pass",
                "method": "POST",
                "path": "/rl/training-sessions/{session_id}/operations/forward",
                "display_name": "Forward Pass",
                "short_desc": "Forward pass",
                "desc": "Submits a forward operation that will asynchronously run a no-grad forward pass and return per-token log-probabilities for each sample. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Training",),
                "operation_id": "forward",
                "fields": {
                    "samples": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Samples",
                        "short_desc": "Batch of training samples for which to compute per-token log-probabilities",
                        "preselected": True,
                        "required": True,
                    },
                    "session_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Session ID",
                        "short_desc": "Value for Session ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "training-sessions",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-forward-operation",
                "method": "GET",
                "path": "/rl/training-sessions/{session_id}/operations/forward/{operation_id}",
                "display_name": "Get Forward Operation",
                "short_desc": "Get forward operation",
                "desc": "Retrieves the current status and result of a forward operation.",
                "groups": ("Training",),
                "operation_id": "getForwardOperation",
                "fields": {
                    "session_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Session ID",
                        "short_desc": "Value for Session ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "training-sessions",
                        "supports_custom_values": True,
                    },
                    "operation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Operation ID",
                        "short_desc": "Value for Operation ID",
                        "preselected": True,
                        "required": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "optimizer-step",
                "method": "POST",
                "path": "/rl/training-sessions/{session_id}/operations/optim-step",
                "display_name": "Optimizer Step",
                "short_desc": "Optimizer step",
                "desc": "Submits an optimizer step operation that will asynchronously apply accumulated gradients to update model parameters. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Training",),
                "operation_id": "OptimStep",
                "fields": {
                    "adam_params": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Adam Params",
                        "short_desc": "Adam optimizer overrides for this step",
                    },
                    "muon_params": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Muon Params",
                        "short_desc": "Muon optimizer overrides for this step",
                    },
                    "session_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Session ID",
                        "short_desc": "Value for Session ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "training-sessions",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "weights-sync",
                "method": "POST",
                "path": "/rl/training-sessions/{session_id}/operations/weights-sync",
                "display_name": "Weights Sync",
                "short_desc": "Weights sync",
                "desc": "Submits a weights-sync operation that makes the session's current trained parameters available for sampling. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Training",),
                "operation_id": "WeightsSync",
                "fields": {
                    "weight_sync_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Weight Sync Type",
                        "short_desc": "How updated parameters are made available for sampling",
                        "preselected": True,
                        "required": True,
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "WEIGHT_SYNC_TYPE_SYNCHRONOUS",
                            "display_name": "Weight Sync Type Synchronous",
                        }, <AllowedValueInfo>{
                            "value": "WEIGHT_SYNC_TYPE_BACKGROUND_PUBLISH",
                            "display_name": "Weight Sync Type Background Publish",
                        }, <AllowedValueInfo>{
                            "value": "WEIGHT_SYNC_TYPE_PIPELINE",
                            "display_name": "Weight Sync Type Pipeline",
                        },),
                    },
                    "session_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Session ID",
                        "short_desc": "Value for Session ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "training-sessions",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "sample",
                "method": "POST",
                "path": "/rl/training-sessions/{session_id}/operations/sample",
                "display_name": "Sample",
                "short_desc": "Sample",
                "desc": "Submits a sample operation that will asynchronously generate text completions with logprobs. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Training",),
                "operation_id": "Sample",
                "fields": {
                    "model_inputs": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model Inputs",
                        "short_desc": "Model inputs to sample from",
                        "preselected": True,
                        "required": True,
                    },
                    "sampling_params": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sampling Params",
                        "short_desc": "Optional sampling parameters",
                    },
                    "num_samples": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Num Samples",
                        "short_desc": "Number of completions to generate per prompt",
                        "example_value": 1,
                    },
                    "prompt_logprobs": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Prompt Logprobs",
                        "short_desc": "Prompt Logprobs",
                        "example_value": False,
                    },
                    "topk_prompt_logprobs": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Topk Prompt Logprobs",
                        "short_desc": "Topk Prompt Logprobs",
                        "example_value": 0,
                    },
                    "return_routed_experts": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Return Routed Experts",
                        "short_desc": "Return Routed Experts",
                        "example_value": False,
                    },
                    "return_routed_experts_object_uri": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Return Routed Experts Object Uri",
                        "short_desc": "Return Routed Experts Object Uri",
                        "example_value": False,
                    },
                    "session_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Session ID",
                        "short_desc": "Value for Session ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "training-sessions",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-inference-checkpoint",
                "method": "POST",
                "path": "/rl/training-sessions/{session_id}/operations/inference-checkpoint",
                "display_name": "Create Inference Checkpoint",
                "short_desc": "Create inference checkpoint",
                "desc": "Submits an operation that will asynchronously save the current LoRA adapter as an inference checkpoint and upload it to object storage. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Training",),
                "operation_id": "createInferenceCheckpoint",
                "fields": {
                    "session_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Session ID",
                        "short_desc": "Value for Session ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "training-sessions",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-inference-checkpoint-operation",
                "method": "GET",
                "path": "/rl/training-sessions/{session_id}/operations/inference-checkpoint/{operation_id}",
                "display_name": "Get Inference Checkpoint Operation",
                "short_desc": "Get inference checkpoint operation",
                "desc": "Retrieves the current status and result of an inference checkpoint operation.",
                "groups": ("Training",),
                "operation_id": "getInferenceCheckpointOperation",
                "fields": {
                    "session_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Session ID",
                        "short_desc": "Value for Session ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "training-sessions",
                        "supports_custom_values": True,
                    },
                    "operation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Operation ID",
                        "short_desc": "Value for Operation ID",
                        "preselected": True,
                        "required": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "save-training-checkpoint",
                "method": "POST",
                "path": "/rl/training-sessions/{session_id}/operations/training-checkpoint",
                "display_name": "Save Training Checkpoint",
                "short_desc": "Save training checkpoint",
                "desc": "Submits an operation that will asynchronously save the full training state (adapter + optimizer + step). **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("Training",),
                "operation_id": "createTrainingCheckpoint",
                "fields": {
                    "session_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Session ID",
                        "short_desc": "Value for Session ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "training-sessions",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-save-training-checkpoint-operation",
                "method": "GET",
                "path": "/rl/training-sessions/{session_id}/operations/training-checkpoint/{operation_id}",
                "display_name": "Get Save Training Checkpoint Operation",
                "short_desc": "Get save training checkpoint operation",
                "desc": "Retrieves the current status and result of a save training checkpoint operation.",
                "groups": ("Training",),
                "operation_id": "getTrainingCheckpointOperation",
                "fields": {
                    "session_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Session ID",
                        "short_desc": "Value for Session ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "training-sessions",
                        "supports_custom_values": True,
                    },
                    "operation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Operation ID",
                        "short_desc": "Value for Operation ID",
                        "preselected": True,
                        "required": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-checkpoint",
                "method": "GET",
                "path": "/rl/checkpoints/{id}",
                "display_name": "Get Checkpoint",
                "short_desc": "Get checkpoint",
                "desc": "Returns metadata for a checkpoint: type, base model, LoRA rank, step, and owning session.",
                "groups": ("Training",),
                "operation_id": "getCheckpoint",
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "download-checkpoint",
                "method": "GET",
                "path": "/rl/checkpoints/{id}/download",
                "display_name": "Download Checkpoint",
                "short_desc": "Download checkpoint",
                "desc": "Returns presigned URLs for downloading a checkpoint's model files.",
                "groups": ("Training",),
                "operation_id": "downloadCheckpoint",
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID",
                        "short_desc": "Value for ID",
                        "preselected": True,
                        "required": True,
                    },
                    "variant": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Variant",
                        "short_desc": "Value for Variant",
                        "preselected": True,
                        "required": True,
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "CHECKPOINT_VARIANT_UNSPECIFIED",
                            "display_name": "Checkpoint Variant Unspecified",
                        }, <AllowedValueInfo>{
                            "value": "CHECKPOINT_VARIANT_MERGED",
                            "display_name": "Checkpoint Variant Merged",
                        }, <AllowedValueInfo>{
                            "value": "CHECKPOINT_VARIANT_ADAPTER",
                            "display_name": "Checkpoint Variant Adapter",
                        },),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-addons",
                "method": "GET",
                "path": "/compute/clusters/{cluster_id}/addons",
                "display_name": "List Addons",
                "short_desc": "List Addons",
                "desc": "List Addons.",
                "groups": ("GPU Clusters",),
                "operation_id": "InstanceClusterAddOnService_List",
                "fields": {
                    "cluster_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cluster ID",
                        "short_desc": "Value for Cluster ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "gpu-clusters",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-addon",
                "method": "POST",
                "path": "/compute/clusters/{cluster_id}/addons",
                "display_name": "Create Addon",
                "short_desc": "Create Addon",
                "desc": "Create Addon. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("GPU Clusters",),
                "operation_id": "InstanceClusterAddOnService_Create",
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Human-readable name for this add-on instance",
                        "preselected": True,
                        "required": True,
                    },
                    "add_on_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Add On Type",
                        "short_desc": "Type of add-on",
                        "preselected": True,
                        "required": True,
                    },
                    "config": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Config",
                        "short_desc": "Configuration for a cluster add-on",
                    },
                    "cluster_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cluster ID",
                        "short_desc": "Value for Cluster ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "gpu-clusters",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-addon",
                "method": "GET",
                "path": "/compute/clusters/{cluster_id}/addons/{addon_id}",
                "display_name": "Get Addon",
                "short_desc": "Get Addon",
                "desc": "Get Addon.",
                "groups": ("GPU Clusters",),
                "operation_id": "InstanceClusterAddOnService_Get",
                "fields": {
                    "cluster_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cluster ID",
                        "short_desc": "Value for Cluster ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "gpu-clusters",
                        "supports_custom_values": True,
                        "has_dependents": True,
                    },
                    "addon_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Addon ID",
                        "short_desc": "Value for Addon ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "addons",
                        "supports_custom_values": True,
                        "depends_on": ("cluster_id",),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-addon",
                "method": "PUT",
                "path": "/compute/clusters/{cluster_id}/addons/{addon_id}",
                "display_name": "Update Addon",
                "short_desc": "Update Addon",
                "desc": "Update Addon. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("GPU Clusters",),
                "operation_id": "InstanceClusterAddOnService_Update",
                "fields": {
                    "config": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Config",
                        "short_desc": "Configuration for a cluster add-on",
                    },
                    "cluster_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cluster ID",
                        "short_desc": "Value for Cluster ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "gpu-clusters",
                        "supports_custom_values": True,
                        "has_dependents": True,
                    },
                    "addon_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Addon ID",
                        "short_desc": "Value for Addon ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "addons",
                        "supports_custom_values": True,
                        "depends_on": ("cluster_id",),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-addon",
                "method": "DELETE",
                "path": "/compute/clusters/{cluster_id}/addons/{addon_id}",
                "display_name": "Delete Addon",
                "short_desc": "Delete Addon",
                "desc": "Delete Addon. **Deletion is permanent.** Check the selected resource before running this action.",
                "groups": ("GPU Clusters",),
                "operation_id": "InstanceClusterAddOnService_Delete",
                "fields": {
                    "cluster_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cluster ID",
                        "short_desc": "Value for Cluster ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "gpu-clusters",
                        "supports_custom_values": True,
                        "has_dependents": True,
                    },
                    "addon_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Addon ID",
                        "short_desc": "Value for Addon ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "addons",
                        "supports_custom_values": True,
                        "depends_on": ("cluster_id",),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-remediations",
                "method": "GET",
                "path": "/compute/clusters/{cluster_id}/instances/{instance_id}/remediations",
                "display_name": "List Remediations",
                "short_desc": "List remediations",
                "desc": "Lists remediations for an instance or cluster.",
                "groups": ("GPU Clusters",),
                "operation_id": "RemediationService_ListRemediations",
                "fields": {
                    "cluster_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cluster ID",
                        "short_desc": "Value for Cluster ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "gpu-clusters",
                        "supports_custom_values": True,
                    },
                    "instance_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Instance ID",
                        "short_desc": "Value for Instance ID",
                        "preselected": True,
                        "required": True,
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Value for Page Size",
                    },
                    "page_token": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "short_desc": "Value for Page Token",
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "display_name": "State",
                        "short_desc": "Value for State",
                    },
                    "order_by": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "short_desc": "Value for Order By",
                    },
                    "trigger": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Trigger",
                        "short_desc": "Value for Trigger",
                    },
                    "mode": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Mode",
                        "short_desc": "Value for Mode",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-remediation",
                "method": "POST",
                "path": "/compute/clusters/{cluster_id}/instances/{instance_id}/remediations",
                "display_name": "Create Remediation",
                "short_desc": "Create Remediation",
                "desc": "Creates a new remediation for an instance. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("GPU Clusters",),
                "operation_id": "RemediationService_CreateRemediation",
                "fields": {
                    "mode": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Mode",
                        "short_desc": "Remediation mode specifies how the remediation should be performed",
                        "preselected": True,
                        "required": True,
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "REMEDIATION_MODE_VM_ONLY",
                            "display_name": "Remediation Mode Vm Only",
                        }, <AllowedValueInfo>{
                            "value": "REMEDIATION_MODE_HOST_AWARE",
                            "display_name": "Remediation Mode Host Aware",
                        }, <AllowedValueInfo>{
                            "value": "REMEDIATION_MODE_EVICT_WITHOUT_REPLACEMENT",
                            "display_name": "Remediation Mode Evict Without Replacement",
                        }, <AllowedValueInfo>{
                            "value": "REMEDIATION_MODE_REBOOT_VM",
                            "display_name": "Remediation Mode Reboot Vm",
                        }, <AllowedValueInfo>{
                            "value": "REMEDIATION_MODE_HOST_POWER_CYCLE",
                            "display_name": "Remediation Mode Host Power Cycle",
                        },),
                    },
                    "reason": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reason",
                        "short_desc": "User-provided reason for the remediation",
                    },
                    "cluster_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cluster ID",
                        "short_desc": "Value for Cluster ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "gpu-clusters",
                        "supports_custom_values": True,
                    },
                    "instance_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Instance ID",
                        "short_desc": "Value for Instance ID",
                        "preselected": True,
                        "required": True,
                    },
                    "remediation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Remediation ID",
                        "short_desc": "Value for Remediation ID",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-remediation",
                "method": "GET",
                "path": "/compute/clusters/{cluster_id}/instances/{instance_id}/remediations/{remediation_id}",
                "display_name": "Get Remediation",
                "short_desc": "Get Remediation",
                "desc": "Retrieve the status of a specific remdiation on a specific instance in a specific cluster.",
                "groups": ("GPU Clusters",),
                "operation_id": "RemediationService_GetRemediation",
                "fields": {
                    "cluster_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cluster ID",
                        "short_desc": "Value for Cluster ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "gpu-clusters",
                        "supports_custom_values": True,
                        "has_dependents": True,
                    },
                    "instance_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Instance ID",
                        "short_desc": "Value for Instance ID",
                        "preselected": True,
                        "required": True,
                        "has_dependents": True,
                    },
                    "remediation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Remediation ID",
                        "short_desc": "Value for Remediation ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "remediations",
                        "supports_custom_values": True,
                        "depends_on": ("cluster_id", "instance_id",),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "approve-remediation",
                "method": "POST",
                "path": "/compute/clusters/{cluster_id}/instances/{instance_id}/remediations/{remediation_id}/approve",
                "display_name": "Approve Remediation",
                "short_desc": "Approve Remediation",
                "desc": "Approves a pending remediation. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("GPU Clusters",),
                "operation_id": "RemediationService_ApproveRemediation",
                "fields": {
                    "comment": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Comment",
                        "short_desc": "Approval comment explaining the decision",
                    },
                    "mode": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Mode",
                        "short_desc": "Remediation mode to use after approval",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "REMEDIATION_MODE_VM_ONLY",
                            "display_name": "Remediation Mode Vm Only",
                        }, <AllowedValueInfo>{
                            "value": "REMEDIATION_MODE_HOST_AWARE",
                            "display_name": "Remediation Mode Host Aware",
                        }, <AllowedValueInfo>{
                            "value": "REMEDIATION_MODE_EVICT_WITHOUT_REPLACEMENT",
                            "display_name": "Remediation Mode Evict Without Replacement",
                        }, <AllowedValueInfo>{
                            "value": "REMEDIATION_MODE_REBOOT_VM",
                            "display_name": "Remediation Mode Reboot Vm",
                        }, <AllowedValueInfo>{
                            "value": "REMEDIATION_MODE_HOST_POWER_CYCLE",
                            "display_name": "Remediation Mode Host Power Cycle",
                        },),
                    },
                    "cluster_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cluster ID",
                        "short_desc": "Value for Cluster ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "gpu-clusters",
                        "supports_custom_values": True,
                        "has_dependents": True,
                    },
                    "instance_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Instance ID",
                        "short_desc": "Value for Instance ID",
                        "preselected": True,
                        "required": True,
                        "has_dependents": True,
                    },
                    "remediation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Remediation ID",
                        "short_desc": "Value for Remediation ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "remediations",
                        "supports_custom_values": True,
                        "depends_on": ("cluster_id", "instance_id",),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "cancel-remediation",
                "method": "POST",
                "path": "/compute/clusters/{cluster_id}/instances/{instance_id}/remediations/{remediation_id}/cancel",
                "display_name": "Cancel Remediation",
                "short_desc": "Cancel Remediation",
                "desc": "Cancels a pending remediation. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("GPU Clusters",),
                "operation_id": "RemediationService_CancelRemediation",
                "fields": {
                    "cluster_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cluster ID",
                        "short_desc": "Value for Cluster ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "gpu-clusters",
                        "supports_custom_values": True,
                        "has_dependents": True,
                    },
                    "instance_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Instance ID",
                        "short_desc": "Value for Instance ID",
                        "preselected": True,
                        "required": True,
                        "has_dependents": True,
                    },
                    "remediation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Remediation ID",
                        "short_desc": "Value for Remediation ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "remediations",
                        "supports_custom_values": True,
                        "depends_on": ("cluster_id", "instance_id",),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "reject-remediation",
                "method": "POST",
                "path": "/compute/clusters/{cluster_id}/instances/{instance_id}/remediations/{remediation_id}/reject",
                "display_name": "Reject Remediation",
                "short_desc": "Reject Remediation",
                "desc": "Rejects a pending remediation. **Usage charges may apply.** Review model, training, or resource settings before submitting.",
                "groups": ("GPU Clusters",),
                "operation_id": "RemediationService_RejectRemediation",
                "fields": {
                    "comment": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Comment",
                        "short_desc": "Comment explaining the action",
                    },
                    "cluster_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cluster ID",
                        "short_desc": "Value for Cluster ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "gpu-clusters",
                        "supports_custom_values": True,
                        "has_dependents": True,
                    },
                    "instance_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Instance ID",
                        "short_desc": "Value for Instance ID",
                        "preselected": True,
                        "required": True,
                        "has_dependents": True,
                    },
                    "remediation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Remediation ID",
                        "short_desc": "Value for Remediation ID",
                        "preselected": True,
                        "required": True,
                        "ref_data": "remediations",
                        "supports_custom_values": True,
                        "depends_on": ("cluster_id", "instance_id",),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-passive-health-check-alerts",
                "method": "GET",
                "path": "/compute/passive_health_check_alerts",
                "display_name": "List Passive Health Check Alerts",
                "short_desc": "List passive health check alerts",
                "desc": "Lists passive health check alerts.",
                "groups": ("GPU Clusters",),
                "operation_id": "HealthCheckService_ListPassiveHealthCheckAlerts",
                "fields": {
                    "intent_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Intent ID",
                        "short_desc": "Value for Intent ID",
                    },
                    "instance_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Instance ID",
                        "short_desc": "Value for Instance ID",
                    },
                    "cluster_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cluster ID",
                        "short_desc": "Value for Cluster ID",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Value for Page Size",
                    },
                    "page_token": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "short_desc": "Value for Page Token",
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Value for Status",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "PHC_ALERT_STATUS_FIRING",
                            "display_name": "Phc Alert Status Firing",
                        }, <AllowedValueInfo>{
                            "value": "PHC_ALERT_STATUS_RESOLVED",
                            "display_name": "Phc Alert Status Resolved",
                        }, <AllowedValueInfo>{
                            "value": "PHC_ALERT_STATUS_ALL",
                            "display_name": "Phc Alert Status All",
                        },),
                    },
                    "severity_filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Severity Filter",
                        "short_desc": "Value for Severity Filter",
                    },
                    "order_by": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "short_desc": "Value for Order By",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "PHC_ALERT_ORDER_STARTED_AT_ASC",
                            "display_name": "Phc Alert Order Started At Asc",
                        }, <AllowedValueInfo>{
                            "value": "PHC_ALERT_ORDER_STARTED_AT_DESC",
                            "display_name": "Phc Alert Order Started At Desc",
                        },),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-api-key-identity",
                "method": "GET",
                "path": "/whoami",
                "display_name": "Get API Key Identity",
                "short_desc": "Get API key identity",
                "desc": "Returns identity information about the authenticated API key.",
                "groups": ("Account",),
                "operation_id": "whoami",
            },
            <RestSchemaActionInfo>{
                "action": "get-billing-usage",
                "method": "GET",
                "path": "/billing/usage",
                "display_name": "Get Billing Usage",
                "short_desc": "Get billing usage",
                "desc": "Returns an organization's billing usage for a month as cost-annotated line items grouped into time windows.",
                "groups": ("Account",),
                "operation_id": "getBillingUsage",
                "fields": {
                    "month": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Month",
                        "short_desc": "Value for Month",
                    },
                    "organization_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Organization ID",
                        "short_desc": "Value for Organization ID",
                        "ref_data": "organizations",
                        "supports_custom_values": True,
                    },
                    "granularity": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Granularity",
                        "short_desc": "Value for Granularity",
                        "allowed_values": (<AllowedValueInfo>{
                            "value": "day",
                            "display_name": "Day",
                        }, <AllowedValueInfo>{
                            "value": "hour",
                            "display_name": "Hour",
                        },),
                    },
                    "limit": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Limit",
                        "short_desc": "Value for Limit",
                        "preselected": True,
                    },
                    "after": <RestSchemaFieldOverlayInfo>{
                        "display_name": "After",
                        "short_desc": "Value for After",
                    },
                },
            },
        );
    }
}
}
