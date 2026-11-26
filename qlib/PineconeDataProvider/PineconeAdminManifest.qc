# -*- mode: qore; indent-tabs-mode: nil -*-
/** PineconeAdminManifest.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace PineconeDataProvider {
#! The 32 explicitly selected Pinecone Admin operations and their presentation
/** Admin actions use the separate service-account connection. Vendor x-enum declarations are restored
    as named choices, and fields containing issued credentials are marked sensitive in responses.
*/
public class PineconeAdminManifest {
    public {
        #! Live identity choices; API key IDs are listed across the organization's projects
        const ReferenceData = {
            "pinecone-admin-projects": <RestSchemaReferenceDataInfo>{
                "name": "pinecone-admin-projects",
                "action": "list-projects",
                "records_path": "data",
                "value_field": "id",
                "display_field": "name",
                "desc": "Find the projects available to the organization before provisioning workloads or reviewing access.",
            },
            "pinecone-admin-organizations": <RestSchemaReferenceDataInfo>{
                "name": "pinecone-admin-organizations",
                "action": "list-organizations",
                "records_path": "data",
                "value_field": "id",
                "display_field": "name",
                "desc": "Find the organizations associated with the account before reviewing their settings or access.",
            },
            "pinecone-admin-api-keys": <RestSchemaReferenceDataInfo>{
                "name": "pinecone-admin-api-keys",
                "action": "list-project-api-keys",
                "records_path": "data",
                "value_field": "id",
                "display_field": "name",
                "desc": "Review the API keys issued for a project before granting access or retiring unused credentials.",
                "options_reference": "pinecone-admin-projects",
                "reference_option": "project_id",
            },
            "pinecone-admin-service-accounts": <RestSchemaReferenceDataInfo>{
                "name": "pinecone-admin-service-accounts",
                "action": "list-service-accounts",
                "records_path": "data",
                "value_field": "id",
                "display_field": "name",
                "desc": "Review the service accounts used by automated integrations before provisioning or retiring access.",
                "options": {
                    "limit": 100,
                },
            },
            "pinecone-admin-users": <RestSchemaReferenceDataInfo>{
                "name": "pinecone-admin-users",
                "action": "list-users",
                "records_path": "data",
                "value_field": "id",
                "display_field": "email",
                "desc": "Review organization membership, optionally filtering by email to find a specific team member.",
                "options": {
                    "limit": 100,
                },
            },
            "pinecone-admin-invites": <RestSchemaReferenceDataInfo>{
                "name": "pinecone-admin-invites",
                "action": "list-invites",
                "records_path": "data",
                "value_field": "id",
                "display_field": "email",
                "desc": "Review pending and expired invitations to follow up on team onboarding.",
                "options": {
                    "limit": 100,
                },
            },
            "pinecone-admin-role-bindings": <RestSchemaReferenceDataInfo>{
                "name": "pinecone-admin-role-bindings",
                "action": "list-role-bindings",
                "records_path": "data",
                "value_field": "id",
                "display_field": "id",
                "desc": "Review granted permissions, optionally filtering by principal, resource, or role to audit a team or workload.",
                "options": {
                    "limit": 100,
                },
            },
        };

        #! The complete pinned Admin API surface
        const Manifest = (
            <RestSchemaActionInfo>{
                "action": "list-projects",
                "method": "GET",
                "path": "/admin/projects",
                "operation_id": "list_projects",
                "display_name": "List projects",
                "short_desc": "List projects",
                "desc": "List all projects in an organization.",
                "groups": ("Projects",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-project",
                "method": "POST",
                "path": "/admin/projects",
                "operation_id": "create_project",
                "display_name": "Create project",
                "short_desc": "Create a new project",
                "desc": "Create a project to separate a team or workload's indexes, API keys, and deployment settings.",
                "groups": ("Projects",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name",
                        "desc": "The name of the new project.",
                        "example_value": "chatbot-prod",
                    },
                    "max_pods": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Max Pods",
                        "short_desc": "Max Pods",
                        "desc": "The maximum number of Pods that can be created in the project. Default is `0` (serverless only).",
                    },
                    "force_encryption_with_cmek": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Force Encryption With CMEK",
                        "short_desc": "Force Encryption With CMEK",
                        "desc": "Whether to force encryption with a customer-managed encryption key (CMEK). Default is `false`.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-organizations",
                "method": "GET",
                "path": "/admin/organizations",
                "operation_id": "list_organizations",
                "display_name": "List organizations",
                "short_desc": "List organizations",
                "desc": "List all organizations associated with an account.",
                "groups": ("Organizations",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                },
                "response_fields": {
                    "data": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "plan": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (<AllowedValueInfo>{
                                        "value": "Free",
                                        "display_name": "Free",
                                    }, <AllowedValueInfo>{
                                        "value": "Standard",
                                        "display_name": "Standard",
                                    }, <AllowedValueInfo>{
                                        "value": "Enterprise",
                                        "display_name": "Enterprise",
                                    }, <AllowedValueInfo>{
                                        "value": "Dedicated",
                                        "display_name": "Dedicated",
                                    }),
                            },
                            "payment_status": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (<AllowedValueInfo>{
                                        "value": "Active",
                                        "display_name": "Active",
                                    }, <AllowedValueInfo>{
                                        "value": "PaymentPending",
                                        "display_name": "Payment Pending",
                                    }, <AllowedValueInfo>{
                                        "value": "PastDue",
                                        "display_name": "Past Due",
                                    }, <AllowedValueInfo>{
                                        "value": "Restricted",
                                        "display_name": "Restricted",
                                    }, <AllowedValueInfo>{
                                        "value": "Suspended",
                                        "display_name": "Suspended",
                                    }, <AllowedValueInfo>{
                                        "value": "Deactivated",
                                        "display_name": "Deactivated",
                                    }),
                            },
                            "support_tier": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (<AllowedValueInfo>{
                                        "value": "Free",
                                        "display_name": "Free",
                                    }, <AllowedValueInfo>{
                                        "value": "Developer",
                                        "display_name": "Developer",
                                    }, <AllowedValueInfo>{
                                        "value": "Pro",
                                        "display_name": "Pro",
                                    }, <AllowedValueInfo>{
                                        "value": "Enterprise",
                                        "display_name": "Enterprise",
                                    }),
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-project",
                "method": "GET",
                "path": "/admin/projects/{project_id}",
                "operation_id": "fetch_project",
                "display_name": "Get project",
                "short_desc": "Get project details",
                "desc": "Read a project's configuration before changing deployment limits or encryption settings.",
                "groups": ("Projects",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "project_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Project ID",
                        "short_desc": "Project ID",
                        "desc": "Project ID",
                        "ref_data": "pinecone-admin-projects",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-project",
                "method": "DELETE",
                "path": "/admin/projects/{project_id}",
                "operation_id": "delete_project",
                "display_name": "Delete project",
                "short_desc": "Delete a project",
                "desc": "Delete a project and all its configuration; delete its indexes, assistants, backups, and collections first.",
                "groups": ("Projects",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "project_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Project ID",
                        "short_desc": "Project ID",
                        "desc": "Project ID",
                        "ref_data": "pinecone-admin-projects",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-project",
                "method": "PATCH",
                "path": "/admin/projects/{project_id}",
                "operation_id": "update_project",
                "display_name": "Update project",
                "short_desc": "Update a project",
                "desc": "Adjust a project's name, pod limit, or customer-managed encryption settings as its workload changes.",
                "groups": ("Projects",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "project_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Project ID",
                        "short_desc": "Project ID",
                        "desc": "Project ID",
                        "ref_data": "pinecone-admin-projects",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name",
                        "desc": "The name of the new project.",
                        "example_value": "chatbot-prod",
                    },
                    "max_pods": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Max Pods",
                        "short_desc": "Max Pods",
                        "desc": "The maximum number of Pods that can be created in the project.",
                    },
                    "force_encryption_with_cmek": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Force Encryption With CMEK",
                        "short_desc": "Force Encryption With CMEK",
                        "desc": "Whether to force encryption with a customer-managed encryption key (CMEK). Once enabled, CMEK encryption cannot be disabled.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-organization",
                "method": "GET",
                "path": "/admin/organizations/{organization_id}",
                "operation_id": "fetch_organization",
                "display_name": "Get organization",
                "short_desc": "Get organization details",
                "desc": "Read an organization's configuration to review its plan and administrative settings.",
                "groups": ("Organizations",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "organization_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Organization ID",
                        "short_desc": "Organization ID",
                        "desc": "Organization ID",
                        "ref_data": "pinecone-admin-organizations",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
                "response_fields": {
                    "plan": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "Free",
                                "display_name": "Free",
                            }, <AllowedValueInfo>{
                                "value": "Standard",
                                "display_name": "Standard",
                            }, <AllowedValueInfo>{
                                "value": "Enterprise",
                                "display_name": "Enterprise",
                            }, <AllowedValueInfo>{
                                "value": "Dedicated",
                                "display_name": "Dedicated",
                            }),
                    },
                    "payment_status": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "Active",
                                "display_name": "Active",
                            }, <AllowedValueInfo>{
                                "value": "PaymentPending",
                                "display_name": "Payment Pending",
                            }, <AllowedValueInfo>{
                                "value": "PastDue",
                                "display_name": "Past Due",
                            }, <AllowedValueInfo>{
                                "value": "Restricted",
                                "display_name": "Restricted",
                            }, <AllowedValueInfo>{
                                "value": "Suspended",
                                "display_name": "Suspended",
                            }, <AllowedValueInfo>{
                                "value": "Deactivated",
                                "display_name": "Deactivated",
                            }),
                    },
                    "support_tier": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "Free",
                                "display_name": "Free",
                            }, <AllowedValueInfo>{
                                "value": "Developer",
                                "display_name": "Developer",
                            }, <AllowedValueInfo>{
                                "value": "Pro",
                                "display_name": "Pro",
                            }, <AllowedValueInfo>{
                                "value": "Enterprise",
                                "display_name": "Enterprise",
                            }),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-organization",
                "method": "DELETE",
                "path": "/admin/organizations/{organization_id}",
                "operation_id": "delete_organization",
                "display_name": "Delete organization",
                "short_desc": "Delete an organization",
                "desc": "Delete an organization and all its configuration; delete all its projects first.",
                "groups": ("Organizations",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "organization_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Organization ID",
                        "short_desc": "Organization ID",
                        "desc": "Organization ID",
                        "ref_data": "pinecone-admin-organizations",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-organization",
                "method": "PATCH",
                "path": "/admin/organizations/{organization_id}",
                "operation_id": "update_organization",
                "display_name": "Update organization",
                "short_desc": "Update an organization",
                "desc": "Rename an organization so its display name matches the business or team it serves.",
                "groups": ("Organizations",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "organization_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Organization ID",
                        "short_desc": "Organization ID",
                        "desc": "Organization ID",
                        "ref_data": "pinecone-admin-organizations",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name",
                        "desc": "The new name for the organization.",
                        "example_value": "organization-name",
                    },
                },
                "response_fields": {
                    "plan": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "Free",
                                "display_name": "Free",
                            }, <AllowedValueInfo>{
                                "value": "Standard",
                                "display_name": "Standard",
                            }, <AllowedValueInfo>{
                                "value": "Enterprise",
                                "display_name": "Enterprise",
                            }, <AllowedValueInfo>{
                                "value": "Dedicated",
                                "display_name": "Dedicated",
                            }),
                    },
                    "payment_status": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "Active",
                                "display_name": "Active",
                            }, <AllowedValueInfo>{
                                "value": "PaymentPending",
                                "display_name": "Payment Pending",
                            }, <AllowedValueInfo>{
                                "value": "PastDue",
                                "display_name": "Past Due",
                            }, <AllowedValueInfo>{
                                "value": "Restricted",
                                "display_name": "Restricted",
                            }, <AllowedValueInfo>{
                                "value": "Suspended",
                                "display_name": "Suspended",
                            }, <AllowedValueInfo>{
                                "value": "Deactivated",
                                "display_name": "Deactivated",
                            }),
                    },
                    "support_tier": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "Free",
                                "display_name": "Free",
                            }, <AllowedValueInfo>{
                                "value": "Developer",
                                "display_name": "Developer",
                            }, <AllowedValueInfo>{
                                "value": "Pro",
                                "display_name": "Pro",
                            }, <AllowedValueInfo>{
                                "value": "Enterprise",
                                "display_name": "Enterprise",
                            }),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-project-api-keys",
                "method": "GET",
                "path": "/admin/projects/{project_id}/api-keys",
                "operation_id": "list_project_api_keys",
                "display_name": "List project API keys",
                "short_desc": "List API keys",
                "desc": "List all API keys in a project.",
                "groups": ("API Keys",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "project_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Project ID",
                        "short_desc": "Project ID",
                        "desc": "Project ID",
                        "ref_data": "pinecone-admin-projects",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
                "response_fields": {
                    "data": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "roles": <RestSchemaFieldOverlayInfo>{
                                "element_allowed_values": (<AllowedValueInfo>{
                                        "value": "ProjectEditor",
                                        "display_name": "Project Editor",
                                    }, <AllowedValueInfo>{
                                        "value": "ProjectViewer",
                                        "display_name": "Project Viewer",
                                    }, <AllowedValueInfo>{
                                        "value": "ControlPlaneEditor",
                                        "display_name": "Control Plane Editor",
                                    }, <AllowedValueInfo>{
                                        "value": "ControlPlaneViewer",
                                        "display_name": "Control Plane Viewer",
                                    }, <AllowedValueInfo>{
                                        "value": "DataPlaneEditor",
                                        "display_name": "Data Plane Editor",
                                    }, <AllowedValueInfo>{
                                        "value": "DataPlaneViewer",
                                        "display_name": "Data Plane Viewer",
                                    }),
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-api-key",
                "method": "POST",
                "path": "/admin/projects/{project_id}/api-keys",
                "operation_id": "create_api_key",
                "display_name": "Create API key",
                "short_desc": "Create an API key",
                "desc": "Create an API key for a project to authenticate Data Plane and Control Plane requests.\n\n**Save the key:** the API returns the new key only when it is created. Use it with a separate Pinecone database connection.",
                "groups": ("API Keys",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "project_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Project ID",
                        "short_desc": "Project ID",
                        "desc": "Project ID",
                        "ref_data": "pinecone-admin-projects",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name",
                        "desc": "The name of the API key. The name must be 1-80 characters long.",
                        "example_value": "devkey",
                    },
                    "roles": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Roles",
                        "short_desc": "Roles",
                        "desc": "The roles to create the API key with. Default is `[\"ProjectEditor\"]`.",
                        "element_allowed_values": (<AllowedValueInfo>{
                                "value": "ProjectEditor",
                                "display_name": "Project Editor",
                            }, <AllowedValueInfo>{
                                "value": "ProjectViewer",
                                "display_name": "Project Viewer",
                            }, <AllowedValueInfo>{
                                "value": "ControlPlaneEditor",
                                "display_name": "Control Plane Editor",
                            }, <AllowedValueInfo>{
                                "value": "ControlPlaneViewer",
                                "display_name": "Control Plane Viewer",
                            }, <AllowedValueInfo>{
                                "value": "DataPlaneEditor",
                                "display_name": "Data Plane Editor",
                            }, <AllowedValueInfo>{
                                "value": "DataPlaneViewer",
                                "display_name": "Data Plane Viewer",
                            }),
                    },
                },
                "response_fields": {
                    "key": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "roles": <RestSchemaFieldOverlayInfo>{
                                "element_allowed_values": (<AllowedValueInfo>{
                                        "value": "ProjectEditor",
                                        "display_name": "Project Editor",
                                    }, <AllowedValueInfo>{
                                        "value": "ProjectViewer",
                                        "display_name": "Project Viewer",
                                    }, <AllowedValueInfo>{
                                        "value": "ControlPlaneEditor",
                                        "display_name": "Control Plane Editor",
                                    }, <AllowedValueInfo>{
                                        "value": "ControlPlaneViewer",
                                        "display_name": "Control Plane Viewer",
                                    }, <AllowedValueInfo>{
                                        "value": "DataPlaneEditor",
                                        "display_name": "Data Plane Editor",
                                    }, <AllowedValueInfo>{
                                        "value": "DataPlaneViewer",
                                        "display_name": "Data Plane Viewer",
                                    }),
                            },
                        },
                    },
                    "value": <RestSchemaFieldOverlayInfo>{
                        "sensitive": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-api-key",
                "method": "GET",
                "path": "/admin/api-keys/{api_key_id}",
                "operation_id": "fetch_api_key",
                "display_name": "Get API key",
                "short_desc": "Get API key details",
                "desc": "Read an API key's configuration and roles to review its access. The response excludes the secret.",
                "groups": ("API Keys",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "api_key_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "API Key ID",
                        "short_desc": "API Key ID",
                        "desc": "API key ID",
                        "ref_data": "pinecone-admin-api-keys",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
                "response_fields": {
                    "roles": <RestSchemaFieldOverlayInfo>{
                        "element_allowed_values": (<AllowedValueInfo>{
                                "value": "ProjectEditor",
                                "display_name": "Project Editor",
                            }, <AllowedValueInfo>{
                                "value": "ProjectViewer",
                                "display_name": "Project Viewer",
                            }, <AllowedValueInfo>{
                                "value": "ControlPlaneEditor",
                                "display_name": "Control Plane Editor",
                            }, <AllowedValueInfo>{
                                "value": "ControlPlaneViewer",
                                "display_name": "Control Plane Viewer",
                            }, <AllowedValueInfo>{
                                "value": "DataPlaneEditor",
                                "display_name": "Data Plane Editor",
                            }, <AllowedValueInfo>{
                                "value": "DataPlaneViewer",
                                "display_name": "Data Plane Viewer",
                            }),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-api-key",
                "method": "DELETE",
                "path": "/admin/api-keys/{api_key_id}",
                "operation_id": "delete_api_key",
                "display_name": "Delete API key",
                "short_desc": "Delete an API key",
                "desc": "Revoke a project API key when an integration is retired or its credentials must be replaced.",
                "groups": ("API Keys",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "api_key_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "API Key ID",
                        "short_desc": "API Key ID",
                        "desc": "API key ID",
                        "ref_data": "pinecone-admin-api-keys",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-api-key",
                "method": "PATCH",
                "path": "/admin/api-keys/{api_key_id}",
                "operation_id": "update_api_key",
                "display_name": "Update API key",
                "short_desc": "Update an API key",
                "desc": "Change an API key's name or roles to keep its access aligned with the integration it serves.",
                "groups": ("API Keys",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "api_key_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "API Key ID",
                        "short_desc": "API Key ID",
                        "desc": "API key ID",
                        "ref_data": "pinecone-admin-api-keys",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name",
                        "desc": "A new name for the API key. The name must be 1-80 characters long. If omitted, the name will not be updated.",
                        "example_value": "devkey",
                    },
                    "roles": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Roles",
                        "short_desc": "Roles",
                        "desc": "A new set of roles for the API key. Existing roles will be removed if not included.\nIf this field is omitted, the roles will not be updated.",
                        "element_allowed_values": (<AllowedValueInfo>{
                                "value": "ProjectEditor",
                                "display_name": "Project Editor",
                            }, <AllowedValueInfo>{
                                "value": "ProjectViewer",
                                "display_name": "Project Viewer",
                            }, <AllowedValueInfo>{
                                "value": "ControlPlaneEditor",
                                "display_name": "Control Plane Editor",
                            }, <AllowedValueInfo>{
                                "value": "ControlPlaneViewer",
                                "display_name": "Control Plane Viewer",
                            }, <AllowedValueInfo>{
                                "value": "DataPlaneEditor",
                                "display_name": "Data Plane Editor",
                            }, <AllowedValueInfo>{
                                "value": "DataPlaneViewer",
                                "display_name": "Data Plane Viewer",
                            }),
                    },
                },
                "response_fields": {
                    "roles": <RestSchemaFieldOverlayInfo>{
                        "element_allowed_values": (<AllowedValueInfo>{
                                "value": "ProjectEditor",
                                "display_name": "Project Editor",
                            }, <AllowedValueInfo>{
                                "value": "ProjectViewer",
                                "display_name": "Project Viewer",
                            }, <AllowedValueInfo>{
                                "value": "ControlPlaneEditor",
                                "display_name": "Control Plane Editor",
                            }, <AllowedValueInfo>{
                                "value": "ControlPlaneViewer",
                                "display_name": "Control Plane Viewer",
                            }, <AllowedValueInfo>{
                                "value": "DataPlaneEditor",
                                "display_name": "Data Plane Editor",
                            }, <AllowedValueInfo>{
                                "value": "DataPlaneViewer",
                                "display_name": "Data Plane Viewer",
                            }),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-users",
                "method": "GET",
                "path": "/admin/users",
                "operation_id": "list_users",
                "display_name": "List users",
                "short_desc": "List users in the organization",
                "desc": "List users in the caller's organization, optionally filtered by email address.",
                "groups": ("Users",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "email": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Email",
                        "short_desc": "Email",
                        "desc": "Case-insensitive filter on the user's email address. Malformed email returns `400 INVALID_ARGUMENT`.",
                        "example_value": "operator@example.com",
                    },
                    "limit": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "display_name": "Limit",
                        "short_desc": "Limit",
                        "desc": "The number of results to return per page. When omitted, the server defaults to 100. Out-of-range values return `400 OUT_OF_RANGE`.",
                    },
                    "paginationToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Pagination Token",
                        "short_desc": "Pagination Token",
                        "desc": "Cursor from `pagination.next` of a prior response. Must be reused with the same query context (path parameters, filters, and `limit`).",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-user",
                "method": "GET",
                "path": "/admin/users/{user_id}",
                "operation_id": "fetch_user",
                "display_name": "Get user",
                "short_desc": "Get user details",
                "desc": "Read a team member's organization details before reviewing their access or removing membership.",
                "groups": ("Users",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "user_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "User ID",
                        "short_desc": "User ID",
                        "desc": "User ID",
                        "ref_data": "pinecone-admin-users",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-user",
                "method": "DELETE",
                "path": "/admin/users/{user_id}",
                "operation_id": "delete_user",
                "display_name": "Delete user",
                "short_desc": "Remove a user from the organization",
                "desc": "Remove a user from the organization and revoke their role bindings; their Pinecone account is not deleted.",
                "groups": ("Users",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "user_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "User ID",
                        "short_desc": "User ID",
                        "desc": "User ID",
                        "ref_data": "pinecone-admin-users",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-invites",
                "method": "GET",
                "path": "/admin/invites",
                "operation_id": "list_invites",
                "display_name": "List invites",
                "short_desc": "List invites",
                "desc": "List pending and expired invites in the caller's organization.",
                "groups": ("Invites",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "limit": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "display_name": "Limit",
                        "short_desc": "Limit",
                        "desc": "The number of results to return per page. When omitted, the server defaults to 100. Out-of-range values return `400 OUT_OF_RANGE`.",
                    },
                    "paginationToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Pagination Token",
                        "short_desc": "Pagination Token",
                        "desc": "Cursor from `pagination.next` of a prior response. Must be reused with the same query context (path parameters, filters, and `limit`).",
                    },
                },
                "response_fields": {
                    "data": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "status": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (<AllowedValueInfo>{
                                        "value": "pending",
                                        "display_name": "Pending",
                                    }, <AllowedValueInfo>{
                                        "value": "expired",
                                        "display_name": "Expired",
                                    }, <AllowedValueInfo>{
                                        "value": "processed",
                                        "display_name": "Processed",
                                    }),
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-invite",
                "method": "POST",
                "path": "/admin/invites",
                "operation_id": "create_invite",
                "display_name": "Create invite",
                "short_desc": "Invite a user to the organization",
                "desc": "Invite a user to the organization by email and grant their initial role bindings.\n\n**Sends email:** delivers an invitation to the specified address.",
                "groups": ("Invites",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "email": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Email",
                        "short_desc": "Email",
                        "desc": "The email address to invite.",
                        "example_value": "newhire@acme.com",
                    },
                    "role_bindings": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Role Bindings",
                        "short_desc": "Role Bindings",
                        "desc": "Role bindings for the invitee. Must include at least one `organization`-scoped binding that grants organization membership (`OrgOwner`, `OrgManager`, `OrgBillingAdmin`, or `OrgMember`); `project`-scoped bindings are optional. Not returned in the response.",
                        "fields": {
                            "resource_type": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Resource Type",
                                "short_desc": "Resource Type",
                                "desc": "The kind of resource scope a role binding applies to.\nPossible values: `organization`, `project`.",
                                "allowed_values": (<AllowedValueInfo>{
                                        "value": "organization",
                                        "display_name": "Organization",
                                    }, <AllowedValueInfo>{
                                        "value": "project",
                                        "display_name": "Project",
                                    }),
                                "example_value": "project",
                            },
                            "resource_id": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Resource ID",
                                "short_desc": "Resource ID",
                                "desc": "Project UUID. Required when `resource_type` is `project`; omit for `organization` scope.",
                                "ref_data": "pinecone-admin-projects",
                                "supports_custom_values": True,
                                "depends_on": ("resource_type",),
                            },
                            "role": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Role",
                                "short_desc": "Role",
                                "desc": "A role assigned to a principal at a resource scope.",
                                "allowed_values": (<AllowedValueInfo>{
                                        "value": "OrgOwner",
                                        "display_name": "Organization Owner",
                                    }, <AllowedValueInfo>{
                                        "value": "OrgManager",
                                        "display_name": "Organization Manager",
                                    }, <AllowedValueInfo>{
                                        "value": "OrgMember",
                                        "display_name": "Organization Member",
                                    }, <AllowedValueInfo>{
                                        "value": "OrgBillingAdmin",
                                        "display_name": "Organization Billing Admin",
                                    }, <AllowedValueInfo>{
                                        "value": "ProjectOwner",
                                        "display_name": "Project Owner",
                                    }, <AllowedValueInfo>{
                                        "value": "ProjectManager",
                                        "display_name": "Project Manager",
                                    }, <AllowedValueInfo>{
                                        "value": "ProjectMember",
                                        "display_name": "Project Member",
                                    }, <AllowedValueInfo>{
                                        "value": "ProjectEditor",
                                        "display_name": "Project Editor",
                                    }, <AllowedValueInfo>{
                                        "value": "ProjectViewer",
                                        "display_name": "Project Viewer",
                                    }, <AllowedValueInfo>{
                                        "value": "ControlPlaneEditor",
                                        "display_name": "Control Plane Editor",
                                    }, <AllowedValueInfo>{
                                        "value": "ControlPlaneViewer",
                                        "display_name": "Control Plane Viewer",
                                    }, <AllowedValueInfo>{
                                        "value": "DataPlaneEditor",
                                        "display_name": "Data Plane Editor",
                                    }, <AllowedValueInfo>{
                                        "value": "DataPlaneViewer",
                                        "display_name": "Data Plane Viewer",
                                    }),
                                "example_value": "ProjectOwner",
                            },
                        },
                    },
                },
                "response_fields": {
                    "status": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "pending",
                                "display_name": "Pending",
                            }, <AllowedValueInfo>{
                                "value": "expired",
                                "display_name": "Expired",
                            }, <AllowedValueInfo>{
                                "value": "processed",
                                "display_name": "Processed",
                            }),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-invite",
                "method": "GET",
                "path": "/admin/invites/{invite_id}",
                "operation_id": "fetch_invite",
                "display_name": "Get invite",
                "short_desc": "Get invite details",
                "desc": "Read an invitation's details before following up, resending it, or withdrawing access.",
                "groups": ("Invites",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "invite_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Invite ID",
                        "short_desc": "Invite ID",
                        "desc": "Invite ID",
                        "ref_data": "pinecone-admin-invites",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
                "response_fields": {
                    "status": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "pending",
                                "display_name": "Pending",
                            }, <AllowedValueInfo>{
                                "value": "expired",
                                "display_name": "Expired",
                            }, <AllowedValueInfo>{
                                "value": "processed",
                                "display_name": "Processed",
                            }),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-invite",
                "method": "DELETE",
                "path": "/admin/invites/{invite_id}",
                "operation_id": "delete_invite",
                "display_name": "Delete invite",
                "short_desc": "Delete an invite",
                "desc": "Delete a pending or expired invite and its role bindings; to remove an accepted user, delete the user instead.",
                "groups": ("Invites",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "invite_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Invite ID",
                        "short_desc": "Invite ID",
                        "desc": "Invite ID",
                        "ref_data": "pinecone-admin-invites",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "resend-invite",
                "method": "POST",
                "path": "/admin/invites/{invite_id}/resend",
                "operation_id": "resend_invite",
                "display_name": "Resend invite",
                "short_desc": "Resend an invite email",
                "desc": "Resend the invite email and extend its expiration to 7 days from now; limited to 100 emails per hour per organization.\n\n**Sends email:** delivers another invitation and renews its expiry.",
                "groups": ("Invites",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "invite_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Invite ID",
                        "short_desc": "Invite ID",
                        "desc": "Invite ID",
                        "ref_data": "pinecone-admin-invites",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
                "response_fields": {
                    "status": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "pending",
                                "display_name": "Pending",
                            }, <AllowedValueInfo>{
                                "value": "expired",
                                "display_name": "Expired",
                            }, <AllowedValueInfo>{
                                "value": "processed",
                                "display_name": "Processed",
                            }),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-service-accounts",
                "method": "GET",
                "path": "/admin/service-accounts",
                "operation_id": "list_service_accounts",
                "display_name": "List service accounts",
                "short_desc": "List service accounts",
                "desc": "List service accounts in the caller's organization.",
                "groups": ("Service Accounts",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "limit": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "display_name": "Limit",
                        "short_desc": "Limit",
                        "desc": "The number of results to return per page. When omitted, the server defaults to 100. Out-of-range values return `400 OUT_OF_RANGE`.",
                    },
                    "paginationToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Pagination Token",
                        "short_desc": "Pagination Token",
                        "desc": "Cursor from `pagination.next` of a prior response. Must be reused with the same query context (path parameters, filters, and `limit`).",
                    },
                },
                "response_fields": {
                    "data": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "client_id": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-service-account",
                "method": "POST",
                "path": "/admin/service-accounts",
                "operation_id": "create_service_account",
                "display_name": "Create service account",
                "short_desc": "Create a service account",
                "desc": "Create a service account for automated workflows, optionally assigning its initial roles.\n\n**Save the secret:** `client_secret` is returned only once and cannot be read again.",
                "groups": ("Service Accounts",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name",
                        "desc": "The human-readable name of the service account.",
                        "example_value": "ci-prod",
                    },
                    "role_bindings": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Role Bindings",
                        "short_desc": "Role Bindings",
                        "desc": "Optional initial role bindings. Omitting the field or passing an empty array creates the service account with no role bindings; roles can be added later via the role binding endpoints. A service account may be granted any organization- or project-scoped role. Not returned in the response.",
                        "fields": {
                            "resource_type": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Resource Type",
                                "short_desc": "Resource Type",
                                "desc": "The kind of resource scope a role binding applies to.\nPossible values: `organization`, `project`.",
                                "allowed_values": (<AllowedValueInfo>{
                                        "value": "organization",
                                        "display_name": "Organization",
                                    }, <AllowedValueInfo>{
                                        "value": "project",
                                        "display_name": "Project",
                                    }),
                                "example_value": "project",
                            },
                            "resource_id": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Resource ID",
                                "short_desc": "Resource ID",
                                "desc": "Project UUID. Required when `resource_type` is `project`; omit for `organization` scope.",
                                "ref_data": "pinecone-admin-projects",
                                "supports_custom_values": True,
                                "depends_on": ("resource_type",),
                            },
                            "role": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Role",
                                "short_desc": "Role",
                                "desc": "A role assigned to a principal at a resource scope.",
                                "allowed_values": (<AllowedValueInfo>{
                                        "value": "OrgOwner",
                                        "display_name": "Organization Owner",
                                    }, <AllowedValueInfo>{
                                        "value": "OrgManager",
                                        "display_name": "Organization Manager",
                                    }, <AllowedValueInfo>{
                                        "value": "OrgMember",
                                        "display_name": "Organization Member",
                                    }, <AllowedValueInfo>{
                                        "value": "OrgBillingAdmin",
                                        "display_name": "Organization Billing Admin",
                                    }, <AllowedValueInfo>{
                                        "value": "ProjectOwner",
                                        "display_name": "Project Owner",
                                    }, <AllowedValueInfo>{
                                        "value": "ProjectManager",
                                        "display_name": "Project Manager",
                                    }, <AllowedValueInfo>{
                                        "value": "ProjectMember",
                                        "display_name": "Project Member",
                                    }, <AllowedValueInfo>{
                                        "value": "ProjectEditor",
                                        "display_name": "Project Editor",
                                    }, <AllowedValueInfo>{
                                        "value": "ProjectViewer",
                                        "display_name": "Project Viewer",
                                    }, <AllowedValueInfo>{
                                        "value": "ControlPlaneEditor",
                                        "display_name": "Control Plane Editor",
                                    }, <AllowedValueInfo>{
                                        "value": "ControlPlaneViewer",
                                        "display_name": "Control Plane Viewer",
                                    }, <AllowedValueInfo>{
                                        "value": "DataPlaneEditor",
                                        "display_name": "Data Plane Editor",
                                    }, <AllowedValueInfo>{
                                        "value": "DataPlaneViewer",
                                        "display_name": "Data Plane Viewer",
                                    }),
                                "example_value": "ProjectOwner",
                            },
                        },
                    },
                },
                "response_fields": {
                    "service_account": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "client_id": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                    "client_secret": <RestSchemaFieldOverlayInfo>{
                        "sensitive": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-service-account",
                "method": "GET",
                "path": "/admin/service-accounts/{service_account_id}",
                "operation_id": "fetch_service_account",
                "display_name": "Get service account",
                "short_desc": "Get service account details",
                "desc": "Read a service account's details to review an integration's identity. Its secret is available only when created or rotated.",
                "groups": ("Service Accounts",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "service_account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Service Account ID",
                        "short_desc": "Service Account ID",
                        "desc": "The unique identifier of the service account.",
                        "ref_data": "pinecone-admin-service-accounts",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
                "response_fields": {
                    "client_id": <RestSchemaFieldOverlayInfo>{
                        "sensitive": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-service-account",
                "method": "DELETE",
                "path": "/admin/service-accounts/{service_account_id}",
                "operation_id": "delete_service_account",
                "display_name": "Delete service account",
                "short_desc": "Delete a service account",
                "desc": "Delete a service account and its role bindings; tokens it minted are revoked within a few seconds.",
                "groups": ("Service Accounts",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "service_account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Service Account ID",
                        "short_desc": "Service Account ID",
                        "desc": "The unique identifier of the service account.",
                        "ref_data": "pinecone-admin-service-accounts",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-service-account",
                "method": "PATCH",
                "path": "/admin/service-accounts/{service_account_id}",
                "operation_id": "update_service_account",
                "display_name": "Update service account",
                "short_desc": "Update a service account",
                "desc": "Rename a service account to identify the integration it serves. Manage its permissions with the role-binding actions.",
                "groups": ("Service Accounts",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "service_account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Service Account ID",
                        "short_desc": "Service Account ID",
                        "desc": "The unique identifier of the service account.",
                        "ref_data": "pinecone-admin-service-accounts",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name",
                        "desc": "A new name for the service account. If omitted, the name is unchanged.",
                        "example_value": "ci-prod-renamed",
                    },
                },
                "response_fields": {
                    "client_id": <RestSchemaFieldOverlayInfo>{
                        "sensitive": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "rotate-service-account-secret",
                "method": "POST",
                "path": "/admin/service-accounts/{service_account_id}/rotate-secret",
                "operation_id": "rotate_service_account_secret",
                "display_name": "Rotate service account secret",
                "short_desc": "Rotate a service account's OAuth client secret",
                "desc": "Rotate a service account's OAuth client secret; the previous secret and its tokens are revoked within seconds and the new secret is returned only once.\n\n**Save the secret:** store `client_secret` securely when returned; it cannot be read again. Update the connections that use this service account after rotation.",
                "groups": ("Service Accounts",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "service_account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Service Account ID",
                        "short_desc": "Service Account ID",
                        "desc": "The unique identifier of the service account.",
                        "ref_data": "pinecone-admin-service-accounts",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
                "response_fields": {
                    "service_account": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "client_id": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                    "client_secret": <RestSchemaFieldOverlayInfo>{
                        "sensitive": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-role-bindings",
                "method": "GET",
                "path": "/admin/role-bindings",
                "operation_id": "list_role_bindings",
                "display_name": "List role bindings",
                "short_desc": "List role bindings",
                "desc": "List role bindings in the caller's organization, optionally filtered by principal, resource, and role.",
                "groups": ("Role Bindings",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "principal_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Principal Type",
                        "short_desc": "Principal Type",
                        "desc": "Filter by principal type. Required when `principal_id` is set.",
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "user",
                                "display_name": "User",
                            }, <AllowedValueInfo>{
                                "value": "service_account",
                                "display_name": "Service Account",
                            }, <AllowedValueInfo>{
                                "value": "api_key",
                                "display_name": "API Key",
                            }, <AllowedValueInfo>{
                                "value": "invite",
                                "display_name": "Invite",
                            }),
                        "example_value": "service_account",
                    },
                    "principal_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Principal ID",
                        "short_desc": "Principal ID",
                        "desc": "The resource ID of the selected `principal_type`, from **List Users**, **List Service Accounts**, **List Project API Keys**, or **List Invites**. For a service account use its `id`, not its OAuth2 `client_id`.",
                        "depends_on": ("principal_type",),
                        "preselected": True,
                    },
                    "resource_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Resource Type",
                        "short_desc": "Resource Type",
                        "desc": "Filter by resource type. Required when `resource_id` is set.",
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "organization",
                                "display_name": "Organization",
                            }, <AllowedValueInfo>{
                                "value": "project",
                                "display_name": "Project",
                            }),
                        "example_value": "project",
                    },
                    "resource_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Resource ID",
                        "short_desc": "Resource ID",
                        "desc": "Filter by resource ID. Requires `resource_type`.",
                        "ref_data": "pinecone-admin-projects",
                        "supports_custom_values": True,
                        "depends_on": ("resource_type",),
                    },
                    "role": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Role",
                        "short_desc": "Role",
                        "desc": "Filter by role.",
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "OrgOwner",
                                "display_name": "Organization Owner",
                            }, <AllowedValueInfo>{
                                "value": "OrgManager",
                                "display_name": "Organization Manager",
                            }, <AllowedValueInfo>{
                                "value": "OrgMember",
                                "display_name": "Organization Member",
                            }, <AllowedValueInfo>{
                                "value": "OrgBillingAdmin",
                                "display_name": "Organization Billing Admin",
                            }, <AllowedValueInfo>{
                                "value": "ProjectOwner",
                                "display_name": "Project Owner",
                            }, <AllowedValueInfo>{
                                "value": "ProjectManager",
                                "display_name": "Project Manager",
                            }, <AllowedValueInfo>{
                                "value": "ProjectMember",
                                "display_name": "Project Member",
                            }, <AllowedValueInfo>{
                                "value": "ProjectEditor",
                                "display_name": "Project Editor",
                            }, <AllowedValueInfo>{
                                "value": "ProjectViewer",
                                "display_name": "Project Viewer",
                            }, <AllowedValueInfo>{
                                "value": "ControlPlaneEditor",
                                "display_name": "Control Plane Editor",
                            }, <AllowedValueInfo>{
                                "value": "ControlPlaneViewer",
                                "display_name": "Control Plane Viewer",
                            }, <AllowedValueInfo>{
                                "value": "DataPlaneEditor",
                                "display_name": "Data Plane Editor",
                            }, <AllowedValueInfo>{
                                "value": "DataPlaneViewer",
                                "display_name": "Data Plane Viewer",
                            }),
                        "example_value": "ProjectOwner",
                    },
                    "limit": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "display_name": "Limit",
                        "short_desc": "Limit",
                        "desc": "The number of results to return per page. When omitted, the server defaults to 100. Out-of-range values return `400 OUT_OF_RANGE`.",
                    },
                    "paginationToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Pagination Token",
                        "short_desc": "Pagination Token",
                        "desc": "Cursor from `pagination.next` of a prior response. Must be reused with the same query context (path parameters, filters, and `limit`).",
                    },
                },
                "response_fields": {
                    "data": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "principal_type": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (<AllowedValueInfo>{
                                        "value": "user",
                                        "display_name": "User",
                                    }, <AllowedValueInfo>{
                                        "value": "service_account",
                                        "display_name": "Service Account",
                                    }, <AllowedValueInfo>{
                                        "value": "api_key",
                                        "display_name": "API Key",
                                    }, <AllowedValueInfo>{
                                        "value": "invite",
                                        "display_name": "Invite",
                                    }),
                            },
                            "resource_type": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (<AllowedValueInfo>{
                                        "value": "organization",
                                        "display_name": "Organization",
                                    }, <AllowedValueInfo>{
                                        "value": "project",
                                        "display_name": "Project",
                                    }),
                            },
                            "role": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (<AllowedValueInfo>{
                                        "value": "OrgOwner",
                                        "display_name": "Organization Owner",
                                    }, <AllowedValueInfo>{
                                        "value": "OrgManager",
                                        "display_name": "Organization Manager",
                                    }, <AllowedValueInfo>{
                                        "value": "OrgMember",
                                        "display_name": "Organization Member",
                                    }, <AllowedValueInfo>{
                                        "value": "OrgBillingAdmin",
                                        "display_name": "Organization Billing Admin",
                                    }, <AllowedValueInfo>{
                                        "value": "ProjectOwner",
                                        "display_name": "Project Owner",
                                    }, <AllowedValueInfo>{
                                        "value": "ProjectManager",
                                        "display_name": "Project Manager",
                                    }, <AllowedValueInfo>{
                                        "value": "ProjectMember",
                                        "display_name": "Project Member",
                                    }, <AllowedValueInfo>{
                                        "value": "ProjectEditor",
                                        "display_name": "Project Editor",
                                    }, <AllowedValueInfo>{
                                        "value": "ProjectViewer",
                                        "display_name": "Project Viewer",
                                    }, <AllowedValueInfo>{
                                        "value": "ControlPlaneEditor",
                                        "display_name": "Control Plane Editor",
                                    }, <AllowedValueInfo>{
                                        "value": "ControlPlaneViewer",
                                        "display_name": "Control Plane Viewer",
                                    }, <AllowedValueInfo>{
                                        "value": "DataPlaneEditor",
                                        "display_name": "Data Plane Editor",
                                    }, <AllowedValueInfo>{
                                        "value": "DataPlaneViewer",
                                        "display_name": "Data Plane Viewer",
                                    }),
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-role-binding",
                "method": "POST",
                "path": "/admin/role-bindings",
                "operation_id": "create_role_binding",
                "display_name": "Create role binding",
                "short_desc": "Create a role binding",
                "desc": "Grant a user or service account the role it needs at organization or project scope.",
                "groups": ("Role Bindings",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "principal_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Principal Type",
                        "short_desc": "Principal Type",
                        "desc": "The kind of principal that receives permissions from a role binding.\nPossible values: `user`, `service_account`, `api_key`, `invite`.",
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "user",
                                "display_name": "User",
                            }, <AllowedValueInfo>{
                                "value": "service_account",
                                "display_name": "Service Account",
                            }, <AllowedValueInfo>{
                                "value": "api_key",
                                "display_name": "API Key",
                            }, <AllowedValueInfo>{
                                "value": "invite",
                                "display_name": "Invite",
                            }),
                        "example_value": "service_account",
                    },
                    "principal_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Principal ID",
                        "short_desc": "Principal ID",
                        "desc": "The resource ID of the selected `principal_type`, from **List Users**, **List Service Accounts**, **List Project API Keys**, or **List Invites**. For a service account use its `id`, not its OAuth2 `client_id`.",
                        "example_value": "e2e92523-85dc-4142-b8c2-e681be8b78df",
                        "depends_on": ("principal_type",),
                        "preselected": True,
                    },
                    "resource_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Resource Type",
                        "short_desc": "Resource Type",
                        "desc": "The kind of resource scope a role binding applies to.\nPossible values: `organization`, `project`.",
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "organization",
                                "display_name": "Organization",
                            }, <AllowedValueInfo>{
                                "value": "project",
                                "display_name": "Project",
                            }),
                        "example_value": "project",
                    },
                    "resource_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Resource ID",
                        "short_desc": "Resource ID",
                        "desc": "Project UUID. Required when `resource_type` is `project`; omit for `organization` scope.",
                        "example_value": "a2f7dddb-1597-4eff-9f71-535fde243f58",
                        "ref_data": "pinecone-admin-projects",
                        "supports_custom_values": True,
                        "depends_on": ("resource_type",),
                    },
                    "role": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Role",
                        "short_desc": "Role",
                        "desc": "A role assigned to a principal at a resource scope.",
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "OrgOwner",
                                "display_name": "Organization Owner",
                            }, <AllowedValueInfo>{
                                "value": "OrgManager",
                                "display_name": "Organization Manager",
                            }, <AllowedValueInfo>{
                                "value": "OrgMember",
                                "display_name": "Organization Member",
                            }, <AllowedValueInfo>{
                                "value": "OrgBillingAdmin",
                                "display_name": "Organization Billing Admin",
                            }, <AllowedValueInfo>{
                                "value": "ProjectOwner",
                                "display_name": "Project Owner",
                            }, <AllowedValueInfo>{
                                "value": "ProjectManager",
                                "display_name": "Project Manager",
                            }, <AllowedValueInfo>{
                                "value": "ProjectMember",
                                "display_name": "Project Member",
                            }, <AllowedValueInfo>{
                                "value": "ProjectEditor",
                                "display_name": "Project Editor",
                            }, <AllowedValueInfo>{
                                "value": "ProjectViewer",
                                "display_name": "Project Viewer",
                            }, <AllowedValueInfo>{
                                "value": "ControlPlaneEditor",
                                "display_name": "Control Plane Editor",
                            }, <AllowedValueInfo>{
                                "value": "ControlPlaneViewer",
                                "display_name": "Control Plane Viewer",
                            }, <AllowedValueInfo>{
                                "value": "DataPlaneEditor",
                                "display_name": "Data Plane Editor",
                            }, <AllowedValueInfo>{
                                "value": "DataPlaneViewer",
                                "display_name": "Data Plane Viewer",
                            }),
                        "example_value": "ProjectOwner",
                    },
                },
                "response_fields": {
                    "principal_type": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "user",
                                "display_name": "User",
                            }, <AllowedValueInfo>{
                                "value": "service_account",
                                "display_name": "Service Account",
                            }, <AllowedValueInfo>{
                                "value": "api_key",
                                "display_name": "API Key",
                            }, <AllowedValueInfo>{
                                "value": "invite",
                                "display_name": "Invite",
                            }),
                    },
                    "resource_type": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "organization",
                                "display_name": "Organization",
                            }, <AllowedValueInfo>{
                                "value": "project",
                                "display_name": "Project",
                            }),
                    },
                    "role": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "OrgOwner",
                                "display_name": "Organization Owner",
                            }, <AllowedValueInfo>{
                                "value": "OrgManager",
                                "display_name": "Organization Manager",
                            }, <AllowedValueInfo>{
                                "value": "OrgMember",
                                "display_name": "Organization Member",
                            }, <AllowedValueInfo>{
                                "value": "OrgBillingAdmin",
                                "display_name": "Organization Billing Admin",
                            }, <AllowedValueInfo>{
                                "value": "ProjectOwner",
                                "display_name": "Project Owner",
                            }, <AllowedValueInfo>{
                                "value": "ProjectManager",
                                "display_name": "Project Manager",
                            }, <AllowedValueInfo>{
                                "value": "ProjectMember",
                                "display_name": "Project Member",
                            }, <AllowedValueInfo>{
                                "value": "ProjectEditor",
                                "display_name": "Project Editor",
                            }, <AllowedValueInfo>{
                                "value": "ProjectViewer",
                                "display_name": "Project Viewer",
                            }, <AllowedValueInfo>{
                                "value": "ControlPlaneEditor",
                                "display_name": "Control Plane Editor",
                            }, <AllowedValueInfo>{
                                "value": "ControlPlaneViewer",
                                "display_name": "Control Plane Viewer",
                            }, <AllowedValueInfo>{
                                "value": "DataPlaneEditor",
                                "display_name": "Data Plane Editor",
                            }, <AllowedValueInfo>{
                                "value": "DataPlaneViewer",
                                "display_name": "Data Plane Viewer",
                            }),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-role-binding",
                "method": "GET",
                "path": "/admin/role-bindings/{role_binding_id}",
                "operation_id": "fetch_role_binding",
                "display_name": "Get role binding",
                "short_desc": "Get role binding details",
                "desc": "Read a role assignment to check which principal has access to the selected resource.",
                "groups": ("Role Bindings",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "role_binding_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Role Binding ID",
                        "short_desc": "Role Binding ID",
                        "desc": "Role binding ID",
                        "ref_data": "pinecone-admin-role-bindings",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
                "response_fields": {
                    "principal_type": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "user",
                                "display_name": "User",
                            }, <AllowedValueInfo>{
                                "value": "service_account",
                                "display_name": "Service Account",
                            }, <AllowedValueInfo>{
                                "value": "api_key",
                                "display_name": "API Key",
                            }, <AllowedValueInfo>{
                                "value": "invite",
                                "display_name": "Invite",
                            }),
                    },
                    "resource_type": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "organization",
                                "display_name": "Organization",
                            }, <AllowedValueInfo>{
                                "value": "project",
                                "display_name": "Project",
                            }),
                    },
                    "role": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (<AllowedValueInfo>{
                                "value": "OrgOwner",
                                "display_name": "Organization Owner",
                            }, <AllowedValueInfo>{
                                "value": "OrgManager",
                                "display_name": "Organization Manager",
                            }, <AllowedValueInfo>{
                                "value": "OrgMember",
                                "display_name": "Organization Member",
                            }, <AllowedValueInfo>{
                                "value": "OrgBillingAdmin",
                                "display_name": "Organization Billing Admin",
                            }, <AllowedValueInfo>{
                                "value": "ProjectOwner",
                                "display_name": "Project Owner",
                            }, <AllowedValueInfo>{
                                "value": "ProjectManager",
                                "display_name": "Project Manager",
                            }, <AllowedValueInfo>{
                                "value": "ProjectMember",
                                "display_name": "Project Member",
                            }, <AllowedValueInfo>{
                                "value": "ProjectEditor",
                                "display_name": "Project Editor",
                            }, <AllowedValueInfo>{
                                "value": "ProjectViewer",
                                "display_name": "Project Viewer",
                            }, <AllowedValueInfo>{
                                "value": "ControlPlaneEditor",
                                "display_name": "Control Plane Editor",
                            }, <AllowedValueInfo>{
                                "value": "ControlPlaneViewer",
                                "display_name": "Control Plane Viewer",
                            }, <AllowedValueInfo>{
                                "value": "DataPlaneEditor",
                                "display_name": "Data Plane Editor",
                            }, <AllowedValueInfo>{
                                "value": "DataPlaneViewer",
                                "display_name": "Data Plane Viewer",
                            }),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-role-binding",
                "method": "DELETE",
                "path": "/admin/role-bindings/{role_binding_id}",
                "operation_id": "delete_role_binding",
                "display_name": "Delete role binding",
                "short_desc": "Delete a role binding",
                "desc": "Remove a role assignment when access is no longer needed. Permissions are revoked when deletion completes.",
                "groups": ("Role Bindings",),
                "fields": {
                    "header.X-Pinecone-Api-Version": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                        "connection_option": "headers",
                    },
                    "role_binding_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Role Binding ID",
                        "short_desc": "Role Binding ID",
                        "desc": "Role binding ID",
                        "ref_data": "pinecone-admin-role-bindings",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
        );
    }
}
}
