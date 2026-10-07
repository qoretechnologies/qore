# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LlamaParseDataProvider module: the exported LlamaIndex Cloud operations

/** LlamaParseManifest.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace LlamaParseDataProvider {
#! The operations the application exports from the LlamaIndex Cloud API
/** Action IDs are a product commitment: an upstream rename is reported as drift and never renames an
    action here.

    @par Which API version

    LlamaIndex serves \c /api/v1 and \c /api/v2 side by side and publishes a v1-to-v2 migration guide.
    This application exports <b>v2 wherever the vendor has one</b> - parsing, extraction, classification
    and batches - and v1 only where there is no v2 counterpart: files, splitting, saved configurations
    and the account lookups.  The v2 parse surface is also what makes the application usable in one
    step: \c GET \c /api/v2/parse/{job_id} returns the parsed content itself through \c expand, where
    the v1 API required a separate result call per output format.

    @par What the overlay supplies

    The schema types every field correctly, but three things it does not describe have to be supplied
    here, and all three were found by generating the actions rather than by reading the document:

    - Several list parameters name their permitted values in prose and type the elements as bare
      strings: \c expand on four operations, \c order_by on the file listing.  Those values are
      restored as element allowed values, because a list field enumerates what its <b>elements</b> may
      be.
    - \c project_id and \c organization_id appear on almost every operation and are opaque IDs; they
      are backed by the account listings this application also exports.
    - The classify request still carries \c file_id and \c parse_job_id, which the vendor's own
      descriptions mark deprecated in favour of \c file_input.  Offering three ways to say the same
      thing is worse than offering one, so the two deprecated fields are ignored.

    @par What is not exported

    The vendor offers two multipart uploads - \c POST \c /api/v2/parse/upload and
    \c POST \c /api/v1/beta/files.  Neither can be generated: the first declares no request body at
    all, and @ref restschemaactionsintro "RestSchemaActions" has no multipart support, so a generated
    action would send JSON where the server expects a multipart body.  Both are hand-written instead;
    see @ref LlamaParseDataProvider::LlamaParseUploadFileDataProvider "LlamaParseUploadFileDataProvider".

    The v1 extraction agents under \c /api/v1/extraction are not exported either.  They are the older
    stateful form of what \c /api/v2/extract does with a saved configuration, and exporting both would
    give two ways to do one thing, with different IDs, different statuses and different result shapes -
    the same reason the deprecated \c file_id and \c parse_job_id are ignored on the classify request.

    @since %LlamaParseDataProvider 1.1
*/
public class LlamaParseManifest {
    public {
        #! Action groups
        const GroupParse = "Parsing";
        const GroupExtract = "Extraction";
        const GroupClassify = "Classification";
        const GroupSplit = "Splitting";
        const GroupBatches = "Batches";
        const GroupFiles = "Files";
        const GroupConfigurations = "Configurations";
        const GroupDirectories = "Directories";
        const GroupAccount = "Account";

        #! Reference data names
        const RefProject = "llamaparse-projects";
        const RefOrganization = "llamaparse-organizations";
        const RefFile = "llamaparse-files";
        const RefConfiguration = "llamaparse-configurations";
        const RefParseJob = "llamaparse-parse-jobs";
        const RefExtractJob = "llamaparse-extract-jobs";
        const RefClassifyJob = "llamaparse-classify-jobs";
        const RefSplitJob = "llamaparse-split-jobs";
        const RefBatch = "llamaparse-batches";
        const RefDirectory = "llamaparse-directories";
        const RefDirectoryFile = "llamaparse-directory-files";

        #! The project an operation is scoped to
        const ProjectOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Project",
            "short_desc": "The project to work in",
            "desc": "The project to work in.  Leave unset to use the project the API key belongs to.",
            "ref_data": RefProject,
        };

        #! The organization an operation is scoped to
        const OrganizationOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Organization",
            "short_desc": "The organization to work in",
            "desc": "The organization to work in.  Leave unset to use the organization the API key "
                "belongs to.",
            "ref_data": RefOrganization,
        };

        #! The two overlays almost every operation in this API needs
        /** The document also declares a \c session <b>cookie</b> parameter on every operation - the
            LlamaCloud console's own browser session.  It needs no entry here: a cookie parameter is
            never published as an action option, so it cannot reach a user in the first place.
        */
        const CommonFields = {
            "project_id": ProjectOverlay,
            "organization_id": OrganizationOverlay,
        };

        #! Job statuses, restored from the schema's own enum
        const JobStatusValues = (
            <AllowedValueInfo>{"value": "PENDING", "display_name": "Pending"},
            <AllowedValueInfo>{"value": "RUNNING", "display_name": "Running"},
            <AllowedValueInfo>{"value": "COMPLETED", "display_name": "Completed"},
            <AllowedValueInfo>{"value": "FAILED", "display_name": "Failed"},
            <AllowedValueInfo>{"value": "CANCELLED", "display_name": "Cancelled"},
        );

        #! Job statuses for the surfaces that can also throttle
        const ThrottledJobStatusValues = (
            <AllowedValueInfo>{"value": "PENDING", "display_name": "Pending"},
            <AllowedValueInfo>{"value": "THROTTLED", "display_name": "Throttled"},
            <AllowedValueInfo>{"value": "RUNNING", "display_name": "Running"},
            <AllowedValueInfo>{"value": "COMPLETED", "display_name": "Completed"},
            <AllowedValueInfo>{"value": "FAILED", "display_name": "Failed"},
            <AllowedValueInfo>{"value": "CANCELLED", "display_name": "Cancelled"},
        );

        #! Split job statuses, which the vendor spells in lower case unlike every other surface
        const SplitStatusValues = (
            <AllowedValueInfo>{"value": "pending", "display_name": "Pending"},
            <AllowedValueInfo>{"value": "processing", "display_name": "Processing"},
            <AllowedValueInfo>{"value": "completed", "display_name": "Completed"},
            <AllowedValueInfo>{"value": "failed", "display_name": "Failed"},
            <AllowedValueInfo>{"value": "cancelled", "display_name": "Cancelled"},
        );

        #! Parsing tiers, restored from the schema's own enum
        const TierValues = (
            <AllowedValueInfo>{"value": "fast", "display_name": "Fast"},
            <AllowedValueInfo>{"value": "cost_effective", "display_name": "Cost Effective"},
            <AllowedValueInfo>{"value": "agentic", "display_name": "Agentic"},
            <AllowedValueInfo>{"value": "agentic_plus", "display_name": "Agentic Plus"},
        );

        #! Saved-configuration product types, restored from the schema's own enum
        const ProductTypeValues = (
            <AllowedValueInfo>{"value": "parse_v2", "display_name": "Parse"},
            <AllowedValueInfo>{"value": "extract_v2", "display_name": "Extract"},
            <AllowedValueInfo>{"value": "classify_v2", "display_name": "Classify"},
            <AllowedValueInfo>{"value": "split_v1", "display_name": "Split"},
            <AllowedValueInfo>{"value": "spreadsheet_v1", "display_name": "Spreadsheet"},
            <AllowedValueInfo>{"value": "unknown", "display_name": "Unknown"},
        );

        #! What a parse job can be asked to include, named in the parameter's prose and typed as string
        const ParseExpandValues = (
            <AllowedValueInfo>{"value": "text", "display_name": "Plain Text"},
            <AllowedValueInfo>{"value": "markdown", "display_name": "Markdown"},
            <AllowedValueInfo>{"value": "items", "display_name": "Structured Items"},
            <AllowedValueInfo>{"value": "metadata", "display_name": "Metadata"},
            <AllowedValueInfo>{"value": "forms", "display_name": "Forms"},
            <AllowedValueInfo>{"value": "job_metadata", "display_name": "Job Metadata"},
            <AllowedValueInfo>{"value": "usage", "display_name": "Usage"},
            <AllowedValueInfo>{"value": "text_content_metadata",
                "display_name": "Plain Text Download URL"},
            <AllowedValueInfo>{"value": "markdown_content_metadata",
                "display_name": "Markdown Download URL"},
            <AllowedValueInfo>{"value": "items_content_metadata",
                "display_name": "Structured Items Download URL"},
            <AllowedValueInfo>{"value": "metadata_content_metadata",
                "display_name": "Metadata Download URL"},
            <AllowedValueInfo>{"value": "forms_content_metadata",
                "display_name": "Forms Download URL"},
            <AllowedValueInfo>{"value": "raw_words_content_metadata",
                "display_name": "Raw Words Download URL"},
            <AllowedValueInfo>{"value": "xlsx_content_metadata",
                "display_name": "Spreadsheet Download URL"},
            <AllowedValueInfo>{"value": "output_pdf_content_metadata",
                "display_name": "Output PDF Download URL"},
            <AllowedValueInfo>{"value": "images_content_metadata",
                "display_name": "Images Download URL"},
        );

        #! How a file listing may be ordered, named in the parameter's prose and typed as string
        /** The vendor rejects anything else, including an explicit \c asc / \c desc modifier, so the
            three it accepts are the whole set and the field is not creatable.
        */
        const FileOrderValues = (
            <AllowedValueInfo>{"value": "name", "display_name": "Name (A-Z)"},
            <AllowedValueInfo>{"value": "id", "display_name": "ID (ascending)"},
            <AllowedValueInfo>{"value": "created_at", "display_name": "Newest First"},
        );

        #! Dropdowns backed by the application's own listing actions
        /** Every listing in this API answers with the same envelope - \c items, \c next_page_token and
            \c total_size - so every entry here follows \c "items" and caps the page, rather than
            walking a collection to fill a control.
        */
        const ReferenceData = {
            RefProject: <RestSchemaReferenceDataInfo>{
                "name": RefProject,
                "action": "list-projects",
                "options": {"page_size": 100},
                "records_path": "items",
                "value_field": "id",
                "display_field": "name",
                "desc": "projects in this LlamaCloud account",
            },
            RefOrganization: <RestSchemaReferenceDataInfo>{
                "name": RefOrganization,
                "action": "list-organizations",
                "options": {"page_size": 100},
                "records_path": "items",
                "value_field": "id",
                "display_field": "name",
                "desc": "organizations in this LlamaCloud account",
            },
            RefFile: <RestSchemaReferenceDataInfo>{
                "name": RefFile,
                "action": "list-files",
                "options": {"page_size": 100, "order_by": "created_at"},
                "records_path": "items",
                "value_field": "id",
                "display_field": "name",
                "desc": "files uploaded to this project, newest first",
            },
            RefConfiguration: <RestSchemaReferenceDataInfo>{
                "name": RefConfiguration,
                "action": "list-configurations",
                "options": {"page_size": 100, "latest_only": True},
                "records_path": "items",
                "value_field": "id",
                "display_field": "name",
                "desc": "saved configurations in this project",
            },
            RefParseJob: <RestSchemaReferenceDataInfo>{
                "name": RefParseJob,
                "action": "list-parse-jobs",
                "options": {"page_size": 100},
                "records_path": "items",
                "value_field": "id",
                "display_field": "name",
                "desc": "parse jobs in this project",
            },
            RefExtractJob: <RestSchemaReferenceDataInfo>{
                "name": RefExtractJob,
                "action": "list-extract-jobs",
                "options": {"page_size": 100},
                "records_path": "items",
                "value_field": "id",
                "desc": "extraction jobs in this project",
            },
            RefClassifyJob: <RestSchemaReferenceDataInfo>{
                "name": RefClassifyJob,
                "action": "list-classify-jobs",
                "options": {"page_size": 100},
                "records_path": "items",
                "value_field": "id",
                "desc": "classification jobs in this project",
            },
            RefSplitJob: <RestSchemaReferenceDataInfo>{
                "name": RefSplitJob,
                "action": "list-split-jobs",
                "options": {"page_size": 100},
                "records_path": "items",
                "value_field": "id",
                "desc": "split jobs in this project",
            },
            RefDirectory: <RestSchemaReferenceDataInfo>{
                "name": RefDirectory,
                "action": "list-directories",
                "options": {"page_size": 100},
                "records_path": "items",
                "value_field": "id",
                "display_field": "name",
                "desc": "directories in this project",
            },
            RefDirectoryFile: <RestSchemaReferenceDataInfo>{
                "name": RefDirectoryFile,
                "action": "list-directory-files",
                "options": {"page_size": 100},
                "options_from": {"directory_id": "directory_id"},
                "records_path": "items",
                "value_field": "id",
                "display_field": "display_name",
                "desc": "files in the selected directory",
            },
            RefBatch: <RestSchemaReferenceDataInfo>{
                "name": RefBatch,
                "action": "list-batches",
                "options": {"page_size": 100},
                "records_path": "items",
                "value_field": "id",
                "desc": "batches in this project",
            },
        };

        #! The operations this application exports
        const Manifest = (
            # ------------------------------------------------------------------------------ parsing
            <RestSchemaActionInfo>{
                "action": "parse-file",
                "method": "POST",
                "path": "/api/v2/parse",
                "display_name": "Parse Document",
                "short_desc": "Parse a document into markdown, text and structured items",
                "desc": "Submits a document to LlamaParse and returns the job that was started.\n\n"
                    "Give the document either as `file_id` - a file already uploaded to the project - "
                    "or as `source_url`, a URL LlamaParse can fetch.  Use **Upload File** first if the "
                    "document is on local disk.\n\n"
                    "`tier` decides how the document is read and what it costs:\n"
                    "- `fast` - rule-based, cheapest, no AI\n"
                    "- `cost_effective` - balanced speed and quality\n"
                    "- `agentic` - full AI-powered parsing\n"
                    "- `agentic_plus` - premium AI with specialized features\n\n"
                    "The job runs asynchronously; read the result with **Get Parse Job**, asking for "
                    "the output formats you want through `expand`.",
                "groups": (GroupParse,),
                "field_order": ("file_id", "source_url", "tier", "version", "configuration_id"),
                "fields": CommonFields + {
                    "file_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File",
                        "short_desc": "A file already uploaded to the project",
                        "desc": "A file already uploaded to the project.  Mutually exclusive with "
                            "`source_url`.",
                        "ref_data": RefFile,
                        "exclusive_with": ("source_url",),
                        "required_groups": ("document",),
                        "preselected": True,
                    },
                    "source_url": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Document URL",
                        "short_desc": "A public URL LlamaParse can fetch the document from",
                        "desc": "A public URL LlamaParse can fetch the document from.  Mutually "
                            "exclusive with `file_id`.",
                        "exclusive_with": ("file_id",),
                        "required_groups": ("document",),
                        "preselected": True,
                    },
                    "tier": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Tier",
                        "short_desc": "How thoroughly the document is read, and what it costs",
                        "desc": "How thoroughly the document is read, and what it costs.  `fast` is "
                            "rule-based and cheapest; `agentic_plus` is the most capable.",
                        "allowed_values": TierValues,
                        "preselected": True,
                    },
                    "version": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Tier Version",
                        "short_desc": "Which dated version of the tier to run",
                        "desc": "Which dated version of the chosen tier to run.  `latest` tracks the "
                            "newest; a dated version pins the behaviour.  **List Parse Versions** "
                            "reports the versions each tier currently accepts, and any of them may be "
                            "given here - the values offered are the ones the pinned API description "
                            "names.",
                        "allowed_values_creatable": True,
                        "preselected": True,
                    },
                    "configuration_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Saved Configuration",
                        "short_desc": "A saved parse configuration to start from",
                        "desc": "A saved parse configuration to start from.  When set, `tier` and "
                            "`version` default to the saved configuration's own.",
                        "ref_data": RefConfiguration,
                    },
                    "webhook_configuration_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Saved Webhook Configurations",
                        "short_desc": "Saved webhook configurations to notify about this job",
                    },
                    "disable_cache": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Bypass Cache",
                        "short_desc": "Re-parse even if this document was parsed before",
                    },
                    "http_proxy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "HTTP Proxy",
                        "short_desc": "Proxy used to fetch the document URL",
                        "desc": "Proxy LlamaParse uses to fetch `source_url`.  Ignored when the "
                            "document is given as `file_id`.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-parse-jobs",
                "method": "GET",
                "path": "/api/v2/parse",
                "display_name": "List Parse Jobs",
                "short_desc": "List parse jobs in this project",
                "desc": "Lists parse jobs, newest first, with optional filtering by status, by job ID "
                    "and by when they were created.\n\n"
                    "Results are paged: pass the `next_page_token` of one response as the "
                    "`page_token` of the next.",
                "groups": (GroupParse,),
                "fields": CommonFields + {
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Return only jobs in this status",
                        "allowed_values": JobStatusValues,
                        "preselected": True,
                    },
                    "job_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Jobs",
                        "short_desc": "Return only these jobs",
                        "element_ref_data": RefParseJob,
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "How many jobs to return per page",
                    },
                    "page_token": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "short_desc": "The next_page_token of the previous response",
                    },
                    "created_at_on_or_after": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Created Since",
                        "short_desc": "Return only jobs created on or after this date",
                    },
                    "created_at_on_or_before": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Created Before",
                        "short_desc": "Return only jobs created on or before this date",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-parse-job",
                "method": "GET",
                "path": "/api/v2/parse/{job_id}",
                "display_name": "Get Parse Job",
                "short_desc": "Get a parse job and the content it produced",
                "desc": "Returns a parse job.  By itself that is only the job's status; the parsed "
                    "content comes back when `expand` asks for it.\n\n"
                    "Ask for `markdown` or `text` for the document as a whole, `items` for it "
                    "page-by-page, and a `..._content_metadata` value for a download URL instead of "
                    "the content itself, which is what to use when a result is too large to hold in "
                    "the response.",
                "groups": (GroupParse,),
                "field_order": ("job_id", "expand"),
                "fields": CommonFields + {
                    "job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Parse Job",
                        "short_desc": "The job to read",
                        "ref_data": RefParseJob,
                        "preselected": True,
                    },
                    "expand": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include",
                        "short_desc": "The output formats to include in the response",
                        "desc": "The output formats to include in the response.  Without this the job "
                            "comes back with its status and no parsed content.",
                        # the vendor types these as bare strings and names the permitted values only
                        # in the parameter's description
                        "element_allowed_values": ParseExpandValues,
                        "element_allowed_values_creatable": True,
                        "preselected": True,
                    },
                    "image_filenames": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Image Filenames",
                        "short_desc": "Restrict returned image metadata to these filenames",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "cancel-parse-job",
                "method": "POST",
                "path": "/api/v2/parse/{job_id}/cancel",
                "display_name": "Cancel Parse Job",
                "short_desc": "Cancel a running parse job",
                "desc": "Cancels a parse job that is pending or running.  A job that has already "
                    "finished, failed or been cancelled cannot be cancelled again.",
                "groups": (GroupParse,),
                "fields": CommonFields + {
                    "job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Parse Job",
                        "short_desc": "The job to cancel",
                        "ref_data": RefParseJob,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-parse-versions",
                "method": "GET",
                "path": "/api/v2/parse/versions",
                "display_name": "List Parse Versions",
                "short_desc": "List the parse versions each tier accepts",
                "desc": "Lists the dated versions each parsing tier currently accepts, and what "
                    "`latest` resolves to for each.\n\n"
                    "Use it to pin **Parse Document** to a version whose behaviour you have already "
                    "tested, rather than tracking `latest`.",
                "groups": (GroupParse,),
                "fields": {},
            },

            # -------------------------------------------------------------------------------- files
            <RestSchemaActionInfo>{
                "action": "list-files",
                "method": "GET",
                "path": "/api/v1/beta/files",
                "display_name": "List Files",
                "short_desc": "List the files uploaded to this project",
                "desc": "Lists the files uploaded to this project.  A file is uploaded once and can "
                    "then be parsed, extracted from, classified and split without being sent again.",
                "groups": (GroupFiles,),
                "fields": CommonFields + {
                    "file_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File Name",
                        "short_desc": "Return only files with this name",
                        "preselected": True,
                    },
                    "file_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Files",
                        "short_desc": "Return only these files",
                        "element_ref_data": RefFile,
                    },
                    "external_file_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "External File ID",
                        "short_desc": "Return only the file carrying this caller-supplied ID",
                    },
                    "order_by": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "short_desc": "How to order the results",
                        "desc": "How to order the results.  LlamaIndex accepts only these three and "
                            "rejects anything else, including an explicit ascending or descending "
                            "modifier.",
                        # named in the parameter's prose; the schema types it as a bare string
                        "allowed_values": FileOrderValues,
                    },
                    "expand": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include",
                        "short_desc": "Extra fields to include on each file",
                        "element_allowed_values": (
                            <AllowedValueInfo>{"value": "download_url",
                                "display_name": "Download URL"},
                        ),
                        "element_allowed_values_creatable": True,
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "How many files to return per page",
                    },
                    "page_token": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "short_desc": "The next_page_token of the previous response",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-file",
                "method": "GET",
                "path": "/api/v1/beta/files/{file_id}",
                "display_name": "Get File",
                "short_desc": "Get one file's metadata",
                "desc": "Returns one file's metadata.  Ask for `download_url` through `expand` to get "
                    "a link to the stored content.",
                "groups": (GroupFiles,),
                "fields": CommonFields + {
                    "file_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File",
                        "short_desc": "The file to return",
                        "ref_data": RefFile,
                        "preselected": True,
                    },
                    "expand": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include",
                        "short_desc": "Extra fields to include",
                        "element_allowed_values": (
                            <AllowedValueInfo>{"value": "download_url",
                                "display_name": "Download URL"},
                        ),
                        "element_allowed_values_creatable": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-file",
                "method": "DELETE",
                "path": "/api/v1/beta/files/{file_id}",
                "display_name": "Delete File",
                "short_desc": "Delete a file from the project",
                "desc": "Deletes a file from the project.  Jobs that already ran against it keep "
                    "their results.",
                "groups": (GroupFiles,),
                "fields": CommonFields + {
                    "file_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File",
                        "short_desc": "The file to delete",
                        "ref_data": RefFile,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-file-content-url",
                "method": "GET",
                "path": "/api/v1/beta/files/{file_id}/content",
                "display_name": "Get File Download URL",
                "short_desc": "Get a temporary URL to download a file's content",
                "desc": "Returns a presigned URL for downloading the file's stored content.  The URL "
                    "expires, so fetch it when the download is about to happen rather than storing "
                    "it.",
                "groups": (GroupFiles,),
                "fields": CommonFields + {
                    "file_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File",
                        "short_desc": "The file to download",
                        "ref_data": RefFile,
                        "preselected": True,
                    },
                    "expires_at_seconds": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Expires In",
                        "short_desc": "How long the URL stays valid, in seconds",
                    },
                },
            },

            # ----------------------------------------------------------------- page images and forms
            <RestSchemaActionInfo>{
                "action": "list-page-figures",
                "method": "GET",
                "path": "/api/v1/files/{id}/page-figures",
                "display_name": "List Page Figures",
                "short_desc": "List the figures extracted from a document's pages",
                "desc": "Lists the figures - charts, diagrams and images - that parsing extracted from "
                    "a document, across all of its pages.",
                "groups": (GroupParse,),
                "fields": CommonFields + {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File",
                        "short_desc": "The parsed file",
                        "ref_data": RefFile,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-page-figures-for-page",
                "method": "GET",
                "path": "/api/v1/files/{id}/page-figures/{page_index}",
                "display_name": "List Figures on a Page",
                "short_desc": "List the figures extracted from one page",
                "desc": "Lists the figures parsing extracted from one page of a document.",
                "groups": (GroupParse,),
                "fields": CommonFields + {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File",
                        "short_desc": "The parsed file",
                        "ref_data": RefFile,
                        "preselected": True,
                    },
                    "page_index": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page",
                        "short_desc": "The page to list, counted from zero",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-page-screenshots",
                "method": "GET",
                "path": "/api/v1/files/{id}/page_screenshots",
                "display_name": "List Page Screenshots",
                "short_desc": "List the page screenshots parsing produced",
                "desc": "Lists the rendered page images parsing produced for a document.  Useful for "
                    "showing a reviewer the page a value was read from.",
                "groups": (GroupParse,),
                "fields": CommonFields + {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File",
                        "short_desc": "The parsed file",
                        "ref_data": RefFile,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-page-screenshot",
                "method": "GET",
                "path": "/api/v1/files/{id}/page_screenshots/{page_index}",
                "display_name": "Get Page Screenshot",
                "short_desc": "Get the rendered image of one page",
                "desc": "Returns the rendered image of one page of a parsed document.",
                "groups": (GroupParse,),
                "fields": CommonFields + {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File",
                        "short_desc": "The parsed file",
                        "ref_data": RefFile,
                        "preselected": True,
                    },
                    "page_index": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page",
                        "short_desc": "The page to return, counted from zero",
                        "preselected": True,
                    },
                },
            },

            # --------------------------------------------------------------- structured extraction
            <RestSchemaActionInfo>{
                "action": "extract-from-document",
                "method": "POST",
                "path": "/api/v2/extract",
                "display_name": "Extract Structured Data",
                "short_desc": "Extract structured data from a document against a JSON schema",
                "desc": "Pulls structured data out of a document and returns the job that was "
                    "started.\n\n"
                    "Say what to extract in one of two ways: `configuration_id` names a saved "
                    "extraction configuration, or `configuration` supplies one inline with its own "
                    "`data_schema`.  **Generate Extraction Schema** will write that schema from a "
                    "plain-language description if you do not have one.\n\n"
                    "`file_input` is a file ID or the ID of a completed parse job, so a document can "
                    "be parsed once and then extracted from repeatedly.\n\n"
                    "The job runs asynchronously; read it with **Get Extraction Job**.",
                "groups": (GroupExtract,),
                "field_order": ("file_input", "configuration_id", "configuration"),
                "fields": CommonFields + {
                    "file_input": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Document",
                        "short_desc": "The file or completed parse job to extract from",
                        "desc": "The file or completed parse job to extract from.  A file ID starts "
                            "with `dfl-`, a parse job ID with `pjb-`.",
                        "ref_data": RefFile,
                        "preselected": True,
                    },
                    "configuration_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Saved Configuration",
                        "short_desc": "A saved extraction configuration to use",
                        "desc": "A saved extraction configuration to use.  Supply this or "
                            "`configuration`, not both.",
                        "ref_data": RefConfiguration,
                        "exclusive_with": ("configuration",),
                        "required_groups": ("what-to-extract",),
                        "preselected": True,
                    },
                    "configuration": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Configuration",
                        "short_desc": "An inline extraction configuration with its data schema",
                        "exclusive_with": ("configuration_id",),
                        "required_groups": ("what-to-extract",),
                        "preselected": True,
                    },
                    "webhook_configuration_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Saved Webhook Configurations",
                        "short_desc": "Saved webhook configurations to notify about this job",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-extract-jobs",
                "method": "GET",
                "path": "/api/v2/extract",
                "display_name": "List Extraction Jobs",
                "short_desc": "List extraction jobs in this project",
                "desc": "Lists extraction jobs, newest first, with optional filtering by status, by "
                    "configuration and by when they were created.",
                "groups": (GroupExtract,),
                "fields": CommonFields + {
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Return only jobs in this status",
                        "allowed_values": ThrottledJobStatusValues,
                        "preselected": True,
                    },
                    "configuration_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Saved Configuration",
                        "short_desc": "Return only jobs that used this configuration",
                        "ref_data": RefConfiguration,
                    },
                    "file_input": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Document",
                        "short_desc": "Return only jobs for this file or parse job",
                        "ref_data": RefFile,
                    },
                    "job_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Jobs",
                        "short_desc": "Return only these jobs",
                        "element_ref_data": RefExtractJob,
                    },
                    "expand": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include",
                        "short_desc": "Extra fields to include on each job",
                        "element_allowed_values": (
                            <AllowedValueInfo>{"value": "configuration",
                                "display_name": "Configuration Used"},
                            <AllowedValueInfo>{"value": "extract_metadata",
                                "display_name": "Per-Field Metadata"},
                        ),
                        "element_allowed_values_creatable": True,
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "How many jobs to return per page",
                    },
                    "page_token": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "short_desc": "The next_page_token of the previous response",
                    },
                    "created_at_on_or_after": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Created Since",
                        "short_desc": "Return only jobs created on or after this date",
                    },
                    "created_at_on_or_before": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Created Before",
                        "short_desc": "Return only jobs created on or before this date",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-extract-job",
                "method": "GET",
                "path": "/api/v2/extract/{job_id}",
                "display_name": "Get Extraction Job",
                "short_desc": "Get an extraction job and its extracted data",
                "desc": "Returns an extraction job, including the structured data it produced once it "
                    "has completed.",
                "groups": (GroupExtract,),
                "fields": CommonFields + {
                    "job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Extraction Job",
                        "short_desc": "The job to read",
                        "ref_data": RefExtractJob,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "cancel-extract-job",
                "method": "POST",
                "path": "/api/v2/extract/{job_id}/cancel",
                "display_name": "Cancel Extraction Job",
                "short_desc": "Cancel a running extraction job",
                "desc": "Cancels an extraction job that is pending or running.  A job in a terminal "
                    "status cannot be cancelled.",
                "groups": (GroupExtract,),
                "fields": CommonFields + {
                    "job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Extraction Job",
                        "short_desc": "The job to cancel",
                        "ref_data": RefExtractJob,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-extract-job",
                "method": "DELETE",
                "path": "/api/v2/extract/{job_id}",
                "display_name": "Delete Extraction Job",
                "short_desc": "Delete an extraction job and its result",
                "desc": "Deletes an extraction job and the data it extracted.  The document it ran "
                    "against is not deleted.",
                "groups": (GroupExtract,),
                "fields": CommonFields + {
                    "job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Extraction Job",
                        "short_desc": "The job to delete",
                        "ref_data": RefExtractJob,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "generate-extract-schema",
                "method": "POST",
                "path": "/api/v2/extract/schema/generate",
                "display_name": "Generate Extraction Schema",
                "short_desc": "Write an extraction schema from a plain-language description",
                "desc": "Turns a plain-language description of the fields you want into the JSON "
                    "Schema **Extract Structured Data** takes as its `data_schema`.\n\n"
                    "Use it to start an extraction without writing a schema by hand, then refine the "
                    "result and save it with **Create Configuration**.\n\n"
                    "Give a `prompt` describing the fields you want, a `file_id` to infer them from a "
                    "real document, or both.",
                "groups": (GroupExtract,),
                "field_order": ("prompt", "file_id", "name"),
                "fields": CommonFields + {
                    "prompt": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "A plain-language description of the data to extract",
                        "preselected": True,
                    },
                    "file_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Example Document",
                        "short_desc": "A document to infer the schema from",
                        "desc": "A document to infer the schema from, instead of or as well as "
                            "describing it in `prompt`.",
                        "ref_data": RefFile,
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "A name for the generated configuration",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "validate-extract-schema",
                "method": "POST",
                "path": "/api/v2/extract/schema/validation",
                "display_name": "Validate Extraction Schema",
                "short_desc": "Check an extraction schema before running a job with it",
                "desc": "Checks that a JSON Schema is one LlamaExtract can use, and reports what is "
                    "wrong with it if not.  Cheaper than discovering the problem when a job fails.",
                "groups": (GroupExtract,),
                "fields": {},
            },

            # ------------------------------------------------------------------------ classification
            <RestSchemaActionInfo>{
                "action": "classify-document",
                "method": "POST",
                "path": "/api/v2/classify",
                "display_name": "Classify Document",
                "short_desc": "Decide which of a set of document types a document is",
                "desc": "Classifies a document against a set of rules and returns the job that was "
                    "started.\n\n"
                    "Each rule is a `type` - the label to assign - and a `description` in plain "
                    "language saying when it applies.  The classifier returns the best match with a "
                    "confidence score and its reasoning, which is what makes this usable for routing "
                    "an inbound document to the right process.\n\n"
                    "Supply the rules inline as `configuration`, or name a saved one with "
                    "`configuration_id`.",
                "groups": (GroupClassify,),
                "field_order": ("file_input", "configuration_id", "configuration"),
                "fields": CommonFields + {
                    # the vendor's own descriptions mark both of these deprecated in favour of
                    # "file_input"; offering three ways to name one document is worse than one
                    "file_id": <RestSchemaFieldOverlayInfo>{"ignore": True},
                    "parse_job_id": <RestSchemaFieldOverlayInfo>{"ignore": True},
                    "file_input": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Document",
                        "short_desc": "The file or completed parse job to classify",
                        "desc": "The file or completed parse job to classify.  A file ID starts with "
                            "`dfl-`, a parse job ID with `pjb-`.",
                        "ref_data": RefFile,
                        "preselected": True,
                    },
                    "configuration_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Saved Configuration",
                        "short_desc": "A saved set of classification rules to use",
                        "ref_data": RefConfiguration,
                        "exclusive_with": ("configuration",),
                        "required_groups": ("rules",),
                        "preselected": True,
                    },
                    "configuration": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Rules",
                        "short_desc": "An inline set of classification rules",
                        "exclusive_with": ("configuration_id",),
                        "required_groups": ("rules",),
                        "preselected": True,
                    },
                    "transaction_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Idempotency Key",
                        "short_desc": "Reusing a key returns the original job instead of a new one",
                    },
                    "webhook_configuration_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Saved Webhook Configurations",
                        "short_desc": "Saved webhook configurations to notify about this job",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-classify-jobs",
                "method": "GET",
                "path": "/api/v2/classify",
                "display_name": "List Classification Jobs",
                "short_desc": "List classification jobs in this project",
                "desc": "Lists classification jobs, with optional filtering by status, by "
                    "configuration and by when they were created.",
                "groups": (GroupClassify,),
                "fields": CommonFields + {
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Return only jobs in this status",
                        "allowed_values": JobStatusValues,
                        "preselected": True,
                    },
                    "configuration_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Saved Configuration",
                        "short_desc": "Return only jobs that used this configuration",
                        "ref_data": RefConfiguration,
                    },
                    "job_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Jobs",
                        "short_desc": "Return only these jobs",
                        "element_ref_data": RefClassifyJob,
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "How many jobs to return per page",
                    },
                    "page_token": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "short_desc": "The next_page_token of the previous response",
                    },
                    "created_at_on_or_after": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Created Since",
                        "short_desc": "Return only jobs created on or after this date",
                    },
                    "created_at_on_or_before": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Created Before",
                        "short_desc": "Return only jobs created on or before this date",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-classify-job",
                "method": "GET",
                "path": "/api/v2/classify/{job_id}",
                "display_name": "Get Classification Job",
                "short_desc": "Get a classification job and its result",
                "desc": "Returns a classification job, including the matched document type, the "
                    "confidence score and the reasoning once it has completed.",
                "groups": (GroupClassify,),
                "fields": CommonFields + {
                    "job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Classification Job",
                        "short_desc": "The job to read",
                        "ref_data": RefClassifyJob,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "cancel-classify-job",
                "method": "POST",
                "path": "/api/v2/classify/{job_id}/cancel",
                "display_name": "Cancel Classification Job",
                "short_desc": "Cancel a running classification job",
                "desc": "Cancels a classification job that is pending or running.  A job in a terminal "
                    "status cannot be cancelled.",
                "groups": (GroupClassify,),
                "fields": CommonFields + {
                    "job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Classification Job",
                        "short_desc": "The job to cancel",
                        "ref_data": RefClassifyJob,
                        "preselected": True,
                    },
                },
            },

            # ----------------------------------------------------------------------------- splitting
            <RestSchemaActionInfo>{
                "action": "split-document",
                "method": "POST",
                "path": "/api/v1/split/jobs",
                "display_name": "Split Document",
                "short_desc": "Split one document into its constituent sub-documents",
                "desc": "Splits a document that holds several documents into its parts and returns the "
                    "job that was started.\n\n"
                    "This is the action for a scanned batch - a stack of invoices in one PDF, a loan "
                    "file, a mail run - where each part has to be processed on its own afterwards.",
                "groups": (GroupSplit,),
                "field_order": ("file_input", "configuration_id", "configuration"),
                "fields": CommonFields + {
                    "file_input": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Document",
                        "short_desc": "The file or completed parse job to split",
                        "ref_data": RefFile,
                        "preselected": True,
                    },
                    "configuration_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Saved Configuration",
                        "short_desc": "A saved split configuration to use",
                        "ref_data": RefConfiguration,
                        "exclusive_with": ("configuration",),
                        "preselected": True,
                    },
                    "configuration": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Configuration",
                        "short_desc": "An inline split configuration",
                        "exclusive_with": ("configuration_id",),
                    },
                    "transaction_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Idempotency Key",
                        "short_desc": "Reusing a key returns the original job instead of a new one",
                    },
                    "webhook_configuration_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Saved Webhook Configurations",
                        "short_desc": "Saved webhook configurations to notify about this job",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-split-jobs",
                "method": "GET",
                "path": "/api/v1/split/jobs",
                "display_name": "List Split Jobs",
                "short_desc": "List split jobs in this project",
                "desc": "Lists split jobs, with optional filtering by status and by when they were "
                    "created.",
                "groups": (GroupSplit,),
                "fields": CommonFields + {
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Return only jobs in this status",
                        "desc": "Return only jobs in this status.  LlamaIndex spells split statuses "
                            "in lower case, unlike the parse and extract surfaces.",
                        "allowed_values": SplitStatusValues,
                        "preselected": True,
                    },
                    "job_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Jobs",
                        "short_desc": "Return only these jobs",
                        "element_ref_data": RefSplitJob,
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "How many jobs to return per page",
                    },
                    "page_token": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "short_desc": "The next_page_token of the previous response",
                    },
                    "created_at_on_or_after": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Created Since",
                        "short_desc": "Return only jobs created on or after this date",
                    },
                    "created_at_on_or_before": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Created Before",
                        "short_desc": "Return only jobs created on or before this date",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-split-job",
                "method": "GET",
                "path": "/api/v1/split/jobs/{split_job_id}",
                "display_name": "Get Split Job",
                "short_desc": "Get a split job and the parts it found",
                "desc": "Returns a split job, including the sub-documents it identified once it has "
                    "completed.",
                "groups": (GroupSplit,),
                "fields": CommonFields + {
                    "split_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Split Job",
                        "short_desc": "The job to read",
                        "ref_data": RefSplitJob,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "cancel-split-job",
                "method": "POST",
                "path": "/api/v1/split/jobs/{split_job_id}/cancel",
                "display_name": "Cancel Split Job",
                "short_desc": "Cancel a running split job",
                "desc": "Cancels a split job that is pending or processing.",
                "groups": (GroupSplit,),
                "fields": CommonFields + {
                    "split_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Split Job",
                        "short_desc": "The job to cancel",
                        "ref_data": RefSplitJob,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-split-job",
                "method": "DELETE",
                "path": "/api/v1/split/jobs/{split_job_id}",
                "display_name": "Delete Split Job",
                "short_desc": "Delete a split job and its result",
                "desc": "Deletes a split job and the parts it identified.  The document it ran "
                    "against is not deleted.",
                "groups": (GroupSplit,),
                "fields": CommonFields + {
                    "split_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Split Job",
                        "short_desc": "The job to delete",
                        "ref_data": RefSplitJob,
                        "preselected": True,
                    },
                },
            },

            # ------------------------------------------------------------------------------ batches
            <RestSchemaActionInfo>{
                "action": "create-batch",
                "method": "POST",
                "path": "/api/v2/batches",
                "display_name": "Create Batch",
                "short_desc": "Process every file in a directory in one job",
                "desc": "Processes every file in a source directory under one configuration and "
                    "returns the batch that was started.\n\n"
                    "This is how a whole folder is put through the same pipeline without a request "
                    "per document.  A batch reaching `COMPLETED` means every file was mapped to a "
                    "job, not that every file succeeded - read `results` for the per-file outcome.",
                "groups": (GroupBatches,),
                "field_order": ("source_directory_id", "config"),
                "fields": CommonFields + {
                    "source_directory_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Source Directory",
                        "short_desc": "The directory whose files should be processed",
                        "ref_data": RefDirectory,
                        "preselected": True,
                    },
                    "config": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Configuration",
                        "short_desc": "The configuration applied to every file in the directory",
                        "preselected": True,
                    },
                    "webhook_configuration_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Saved Webhook Configurations",
                        "short_desc": "Saved webhook configurations to notify as the batch progresses",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-batches",
                "method": "GET",
                "path": "/api/v2/batches",
                "display_name": "List Batches",
                "short_desc": "List batches in this project",
                "desc": "Lists batches, with optional filtering by status, by source directory and by "
                    "when they were created.",
                "groups": (GroupBatches,),
                "fields": CommonFields + {
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Return only batches in this status",
                        "allowed_values": ThrottledJobStatusValues,
                        "preselected": True,
                    },
                    "source_directory_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Source Directory",
                        "short_desc": "Return only batches over this directory",
                        "ref_data": RefDirectory,
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "How many batches to return per page",
                    },
                    "page_token": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "short_desc": "The next_page_token of the previous response",
                    },
                    "created_at_on_or_after": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Created Since",
                        "short_desc": "Return only batches created on or after this date",
                    },
                    "created_at_on_or_before": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Created Before",
                        "short_desc": "Return only batches created on or before this date",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-batch",
                "method": "GET",
                "path": "/api/v2/batches/{batch_id}",
                "display_name": "Get Batch",
                "short_desc": "Get a batch and its per-file outcomes",
                "desc": "Returns a batch.  Ask for `results` through `expand` to see what happened to "
                    "each file, which is where a batch that completed with failures reports them.",
                "groups": (GroupBatches,),
                "fields": CommonFields + {
                    "batch_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Batch",
                        "short_desc": "The batch to read",
                        "ref_data": RefBatch,
                        "preselected": True,
                    },
                    "expand": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include",
                        "short_desc": "Extra fields to include",
                        "element_allowed_values": (
                            <AllowedValueInfo>{"value": "results",
                                "display_name": "Per-File Results"},
                        ),
                        "element_allowed_values_creatable": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "cancel-batch",
                "method": "POST",
                "path": "/api/v2/batches/{batch_id}/cancel",
                "display_name": "Cancel Batch",
                "short_desc": "Cancel a running batch",
                "desc": "Cancels a running batch.  Files that already finished keep their results; a "
                    "batch in a terminal status cannot be cancelled.",
                "groups": (GroupBatches,),
                "fields": CommonFields + {
                    "batch_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Batch",
                        "short_desc": "The batch to cancel",
                        "ref_data": RefBatch,
                        "preselected": True,
                    },
                },
            },

            # ----------------------------------------------------------------- saved configurations
            <RestSchemaActionInfo>{
                "action": "create-configuration",
                "method": "POST",
                "path": "/api/v1/beta/configurations",
                "display_name": "Create Configuration",
                "short_desc": "Save a parse, extract, classify or split configuration for reuse",
                "desc": "Saves a configuration so that later jobs can name it instead of repeating "
                    "it.\n\n"
                    "The configuration is versioned: saving again under the same name and product "
                    "type creates a new version rather than a second configuration, so jobs pinned to "
                    "an older version keep running as they did.",
                "groups": (GroupConfigurations,),
                "fields": CommonFields + {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "A name for this configuration",
                        "preselected": True,
                    },
                    "parameters": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Parameters",
                        "short_desc": "The configuration itself, shaped by the product it is for",
                        "desc": "The configuration itself.  Its shape depends on the product the "
                            "configuration is for - parse, extract, classify, split or spreadsheet.",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-configurations",
                "method": "GET",
                "path": "/api/v1/beta/configurations",
                "display_name": "List Configurations",
                "short_desc": "List the saved configurations in this project",
                "desc": "Lists the saved configurations in this project, optionally narrowed to one "
                    "or more product types.",
                "groups": (GroupConfigurations,),
                "fields": CommonFields + {
                    "product_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Product",
                        "short_desc": "Return only configurations for these products",
                        "element_allowed_values": ProductTypeValues,
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Return only configurations with this name",
                    },
                    "latest_only": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Latest Version Only",
                        "short_desc": "Return only the newest version of each configuration",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "How many configurations to return per page",
                    },
                    "page_token": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "short_desc": "The next_page_token of the previous response",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-configuration",
                "method": "GET",
                "path": "/api/v1/beta/configurations/{config_id}",
                "display_name": "Get Configuration",
                "short_desc": "Get one saved configuration",
                "desc": "Returns one saved configuration and the parameters it holds.",
                "groups": (GroupConfigurations,),
                "fields": CommonFields + {
                    "config_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Configuration",
                        "short_desc": "The configuration to return",
                        "ref_data": RefConfiguration,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-configuration",
                "method": "PUT",
                "path": "/api/v1/beta/configurations/{config_id}",
                "display_name": "Update Configuration",
                "short_desc": "Update a saved configuration",
                "desc": "Updates a saved configuration.  Omit a field to leave it unchanged.",
                "groups": (GroupConfigurations,),
                "fields": CommonFields + {
                    "config_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Configuration",
                        "short_desc": "The configuration to update",
                        "ref_data": RefConfiguration,
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "A new name for the configuration",
                    },
                    "parameters": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Parameters",
                        "short_desc": "The replacement configuration parameters",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-configuration",
                "method": "DELETE",
                "path": "/api/v1/beta/configurations/{config_id}",
                "display_name": "Delete Configuration",
                "short_desc": "Delete a saved configuration",
                "desc": "Deletes a saved configuration.  Jobs that already ran with it keep the "
                    "configuration they were given.",
                "groups": (GroupConfigurations,),
                "fields": CommonFields + {
                    "config_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Configuration",
                        "short_desc": "The configuration to delete",
                        "ref_data": RefConfiguration,
                        "preselected": True,
                    },
                },
            },

            # -------------------------------------------------------------------------- directories
            <RestSchemaActionInfo>{
                "action": "create-directory",
                "method": "POST",
                "path": "/api/v1/beta/directories",
                "display_name": "Create Directory",
                "short_desc": "Create a directory to group files for batch processing",
                "desc": "Creates a directory.  A directory groups files so that a batch can put all "
                    "of them through the same configuration in one job.",
                "groups": (GroupDirectories,),
                "fields": CommonFields + {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "A name for the directory",
                        "preselected": True,
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "What this directory holds",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-directories",
                "method": "GET",
                "path": "/api/v1/beta/directories",
                "display_name": "List Directories",
                "short_desc": "List the directories in this project",
                "desc": "Lists the directories in this project.  A batch runs over a directory, so "
                    "this is where its ID comes from.",
                "groups": (GroupDirectories,),
                "fields": CommonFields + {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Return only directories with this name",
                        "preselected": True,
                    },
                    "include_deleted": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include Deleted",
                        "short_desc": "Include directories that have been deleted",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "How many directories to return per page",
                    },
                    "page_token": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "short_desc": "The next_page_token of the previous response",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-directory",
                "method": "GET",
                "path": "/api/v1/beta/directories/{directory_id}",
                "display_name": "Get Directory",
                "short_desc": "Get one directory",
                "desc": "Returns one directory.",
                "groups": (GroupDirectories,),
                "fields": CommonFields + {
                    "directory_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Directory",
                        "short_desc": "The directory to return",
                        "ref_data": RefDirectory,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-directory",
                "method": "PATCH",
                "path": "/api/v1/beta/directories/{directory_id}",
                "display_name": "Update Directory",
                "short_desc": "Rename a directory or change its description",
                "desc": "Updates a directory's name or description.  Omit a field to leave it "
                    "unchanged.",
                "groups": (GroupDirectories,),
                "fields": CommonFields + {
                    "directory_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Directory",
                        "short_desc": "The directory to update",
                        "ref_data": RefDirectory,
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "A new name for the directory",
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "A new description for the directory",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-directory",
                "method": "DELETE",
                "path": "/api/v1/beta/directories/{directory_id}",
                "display_name": "Delete Directory",
                "short_desc": "Delete a directory",
                "desc": "Deletes a directory.  The files it grouped are not deleted; they simply stop "
                    "being members of it.",
                "groups": (GroupDirectories,),
                "fields": CommonFields + {
                    "directory_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Directory",
                        "short_desc": "The directory to delete",
                        "ref_data": RefDirectory,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "add-directory-file",
                "method": "POST",
                "path": "/api/v1/beta/directories/{directory_id}/files",
                "display_name": "Add File to Directory",
                "short_desc": "Put an uploaded file into a directory",
                "desc": "Adds a file that has already been uploaded to a directory, so that the next "
                    "batch over that directory includes it.",
                "groups": (GroupDirectories,),
                "field_order": ("directory_id", "file_id", "display_name"),
                "fields": CommonFields + {
                    "directory_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Directory",
                        "short_desc": "The directory to add the file to",
                        "ref_data": RefDirectory,
                        "preselected": True,
                    },
                    "file_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File",
                        "short_desc": "The uploaded file to add",
                        "ref_data": RefFile,
                        "preselected": True,
                    },
                    "display_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Display Name",
                        "short_desc": "The name the file is shown under in this directory",
                    },
                    "unique_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Unique ID",
                        "short_desc": "A caller-supplied ID unique within the directory",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-directory-files",
                "method": "GET",
                "path": "/api/v1/beta/directories/{directory_id}/files",
                "display_name": "List Files in a Directory",
                "short_desc": "List the files a directory holds",
                "desc": "Lists the files a directory holds, with optional filtering by name and by "
                    "when they were last changed.",
                "groups": (GroupDirectories,),
                "fields": CommonFields + {
                    "directory_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Directory",
                        "short_desc": "The directory to list",
                        "ref_data": RefDirectory,
                        "has_dependents": True,
                        "preselected": True,
                    },
                    "display_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Display Name",
                        "short_desc": "Return only files shown under this name",
                    },
                    "display_name_contains": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name Contains",
                        "short_desc": "Return only files whose name contains this text",
                        "preselected": True,
                    },
                    "file_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File",
                        "short_desc": "Return only the entry for this uploaded file",
                        "ref_data": RefFile,
                    },
                    "include_deleted": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include Deleted",
                        "short_desc": "Include files that have been removed from the directory",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "How many files to return per page",
                    },
                    "page_token": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "short_desc": "The next_page_token of the previous response",
                    },
                    "updated_at_on_or_after": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Updated Since",
                        "short_desc": "Return only files changed on or after this date",
                    },
                    "updated_at_on_or_before": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Updated Before",
                        "short_desc": "Return only files changed on or before this date",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-directory-file",
                "method": "GET",
                "path": "/api/v1/beta/directories/{directory_id}/files/{directory_file_id}",
                "display_name": "Get a File in a Directory",
                "short_desc": "Get one file's entry in a directory",
                "desc": "Returns one file's entry in a directory, which carries the name and metadata "
                    "the directory holds for it rather than the file itself.",
                "groups": (GroupDirectories,),
                "fields": CommonFields + {
                    "directory_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Directory",
                        "short_desc": "The directory the file is in",
                        "ref_data": RefDirectory,
                        "has_dependents": True,
                        "preselected": True,
                    },
                    "directory_file_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File",
                        "short_desc": "The directory entry to return",
                        "ref_data": RefDirectoryFile,
                        "depends_on": "directory_id",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-directory-file",
                "method": "PATCH",
                "path": "/api/v1/beta/directories/{directory_id}/files/{directory_file_id}",
                "display_name": "Update a File in a Directory",
                "short_desc": "Rename a file in a directory or move it to another",
                "desc": "Updates a file's entry in a directory.  `target_directory_id` moves the file "
                    "to another directory; omit a field to leave it unchanged.",
                "groups": (GroupDirectories,),
                "fields": CommonFields + {
                    "directory_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Directory",
                        "short_desc": "The directory the file is in",
                        "ref_data": RefDirectory,
                        "has_dependents": True,
                        "preselected": True,
                    },
                    "directory_file_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File",
                        "short_desc": "The directory entry to update",
                        "ref_data": RefDirectoryFile,
                        "depends_on": "directory_id",
                        "preselected": True,
                    },
                    "display_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Display Name",
                        "short_desc": "The name the file is shown under",
                    },
                    "target_directory_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Move to Directory",
                        "short_desc": "The directory to move the file into",
                        "ref_data": RefDirectory,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-directory-file",
                "method": "DELETE",
                "path": "/api/v1/beta/directories/{directory_id}/files/{directory_file_id}",
                "display_name": "Remove a File from a Directory",
                "short_desc": "Remove one file from a directory",
                "desc": "Removes one file from a directory.  The uploaded file itself is not deleted.",
                "groups": (GroupDirectories,),
                "fields": CommonFields + {
                    "directory_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Directory",
                        "short_desc": "The directory to remove the file from",
                        "ref_data": RefDirectory,
                        "has_dependents": True,
                        "preselected": True,
                    },
                    "directory_file_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File",
                        "short_desc": "The directory entry to remove",
                        "ref_data": RefDirectoryFile,
                        "depends_on": "directory_id",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-directory-files",
                "method": "POST",
                "path": "/api/v1/beta/directories/{directory_id}/files/bulk-delete",
                "display_name": "Remove Files from a Directory",
                "short_desc": "Remove several files from a directory at once",
                "desc": "Removes several files from a directory in one request.  The uploaded files "
                    "themselves are not deleted.",
                "groups": (GroupDirectories,),
                "fields": CommonFields + {
                    "directory_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Directory",
                        "short_desc": "The directory to remove the files from",
                        "ref_data": RefDirectory,
                        "has_dependents": True,
                        "preselected": True,
                    },
                    "directory_file_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Files",
                        "short_desc": "The directory entries to remove",
                        "element_ref_data": RefDirectoryFile,
                        "depends_on": "directory_id",
                        "preselected": True,
                    },
                },
            },

            # ------------------------------------------------------------------------------ account
            <RestSchemaActionInfo>{
                "action": "list-projects",
                "method": "GET",
                "path": "/api/v2/projects",
                "display_name": "List Projects",
                "short_desc": "List the projects this API key can reach",
                "desc": "Lists the projects this API key can reach.  A project scopes files, saved "
                    "configurations and jobs, and every other action can be pointed at one.",
                "groups": (GroupAccount,),
                "fields": (CommonFields - "project_id") + {
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "How many projects to return per page",
                    },
                    "page_token": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "short_desc": "The next_page_token of the previous response",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-project",
                "method": "GET",
                "path": "/api/v2/projects/{project_id}",
                "display_name": "Get Project",
                "short_desc": "Get one project",
                "desc": "Returns one project.",
                "groups": (GroupAccount,),
                "fields": (CommonFields - "project_id") + {
                    "project_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Project",
                        "short_desc": "The project to return",
                        "ref_data": RefProject,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-organizations",
                "method": "GET",
                "path": "/api/v2/organizations",
                "display_name": "List Organizations",
                "short_desc": "List the organizations this API key can reach",
                "desc": "Lists the organizations this API key can reach.  An organization holds "
                    "projects.",
                "groups": (GroupAccount,),
                "fields": {},
            },
            <RestSchemaActionInfo>{
                "action": "list-supported-file-types",
                "method": "GET",
                "path": "/api/v1/parsing/supported_file_extensions",
                "display_name": "List Supported File Types",
                "short_desc": "List the file extensions LlamaParse accepts",
                "desc": "Lists the file extensions LlamaParse can parse.  Check a document against "
                    "this before uploading it, rather than discovering the answer from a failed "
                    "job.",
                "groups": (GroupAccount,),
                "fields": {},
            },
        );
    }
}
}
