# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnstructuredDataProvider class definitions

/** UnstructuredDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Partition options shared by every Unstructured partitioning action
/** These map one-to-one onto the form fields of Unstructured's
    <tt>POST /general/v0/general</tt> endpoint and are sent unchanged; the request types add only
    the fields that describe *what* is being partitioned.
*/
public const UnstructuredPartitionOptionFields = {
    "strategy": {
        "display_name": "Strategy",
        "short_desc": "Partitioning strategy",
        "desc": "How the document is analyzed.\n\n"
            "- `auto` — let Unstructured choose based on the document\n"
            "- `fast` — text extraction only; fastest and cheapest, but no OCR\n"
            "- `hi_res` — layout-model inference; needed for accurate tables and "
            "element types (the API default)\n"
            "- `ocr_only` — OCR the whole document; for scans with no text layer\n"
            "- `od_only` — object detection only\n"
            "- `vlm` — use a vision language model; see `vlm_model_provider` and `vlm_model`",
        "type": AbstractDataProviderTypeMap."*softstring",
        "allowed_values": (
            <AllowedValueInfo>{"value": "auto", "display_name": "Auto"},
            <AllowedValueInfo>{"value": "fast", "display_name": "Fast"},
            <AllowedValueInfo>{"value": "hi_res", "display_name": "Hi-Res"},
            <AllowedValueInfo>{"value": "ocr_only", "display_name": "OCR Only"},
            <AllowedValueInfo>{"value": "od_only", "display_name": "Object Detection Only"},
            <AllowedValueInfo>{"value": "vlm", "display_name": "Vision Language Model"},
        ),
        "preselected": True,
    },
    "content_type": {
        "display_name": "Content Type",
        "short_desc": "MIME type hint for the document",
        "desc": "MIME type of the document content, in `type/subtype` form.\n\n"
            "Unstructured uses this to decide how to parse the input, so setting it matters "
            "for any text format that carries structure.  Content submitted as `text/plain` "
            "is not parsed for markup: HTML returns as a single element holding the raw "
            "markup, and Markdown keeps its `#` and `-` markers in the element text.",
        "type": AbstractDataProviderTypeMap."*softstring",
        "example_value": "text/markdown",
        "preselected": True,
    },
    "chunking_strategy": {
        "display_name": "Chunking Strategy",
        "short_desc": "Server-side chunking strategy",
        "desc": "Groups the partitioned elements into chunks server-side, returning "
            "`CompositeElement` chunks instead of the raw element tree.\n\n"
            "- `basic` — fill each chunk up to the size limits\n"
            "- `by_title` — start a new chunk at each section heading\n"
            "- `by_page` — start a new chunk at each page boundary\n"
            "- `by_similarity` — group topically similar elements; see `similarity_threshold`\n\n"
            "When unset, no chunking is performed and the other chunking options are ignored.",
        "type": AbstractDataProviderTypeMap."*softstring",
        "allowed_values": (
            <AllowedValueInfo>{"value": "basic", "display_name": "Basic"},
            <AllowedValueInfo>{"value": "by_title", "display_name": "By Title"},
            <AllowedValueInfo>{"value": "by_page", "display_name": "By Page"},
            <AllowedValueInfo>{"value": "by_similarity", "display_name": "By Similarity"},
        ),
        "preselected": True,
    },
    "languages": {
        "display_name": "Languages",
        "short_desc": "Document language hints (ISO codes)",
        "desc": "List of ISO 639 language codes present in the document, used to guide "
            "partitioning and OCR.",
        "type": AbstractDataProviderTypeMap."*softlist",
        "example_value": ("eng",),
    },
    "coordinates": {
        "display_name": "Coordinates",
        "short_desc": "Include layout coordinates in each element",
        "desc": "When `True`, each element includes its bounding box in "
            "`metadata.coordinates`.",
        "type": AbstractDataProviderTypeMap."*softbool",
    },
    "extract_image_block_types": {
        "display_name": "Extract Image Block Types",
        "short_desc": "Element types to return as base64 images",
        "desc": "Element types to extract as images, returned base64-encoded in "
            "`metadata.image_base64`.\n\n"
            "Use it to keep figures and tables that carry meaning no text extraction can "
            "recover — charts, diagrams, scanned tables — alongside the text.",
        "type": AbstractDataProviderTypeMap."*softlist",
        "example_value": ("Image", "Table"),
    },
    "include_page_breaks": {
        "display_name": "Include Page Breaks",
        "short_desc": "Emit an element at each page boundary",
        "desc": "When `True`, a `PageBreak` element is emitted at each page boundary.",
        "type": AbstractDataProviderTypeMap."*softbool",
    },
    "starting_page_number": {
        "display_name": "Starting Page Number",
        "short_desc": "Page number to assign to the first page",
        "desc": "Page number to assign to the first page of the submitted document.  Set it "
            "when submitting a slice of a larger document so that `metadata.page_number` "
            "stays correct.",
        "type": AbstractDataProviderTypeMap."*softint",
    },
    "unique_element_ids": {
        "display_name": "Unique Element IDs",
        "short_desc": "Assign a unique ID to every element",
        "desc": "When `True`, every element gets a UUID.  By default the ID is a hash of the "
            "element's text, so identical text produces identical IDs.",
        "type": AbstractDataProviderTypeMap."*softbool",
    },
    "pdf_infer_table_structure": {
        "display_name": "Infer PDF Table Structure",
        "short_desc": "Reconstruct table structure in PDFs",
        "desc": "When `True` (the API default), `Table` elements from PDFs carry an HTML "
            "rendering of the table in `metadata.text_as_html`.  Requires the `hi_res` "
            "strategy.",
        "type": AbstractDataProviderTypeMap."*softbool",
    },
    "skip_infer_table_types": {
        "display_name": "Skip Table Inference For",
        "short_desc": "Document types to skip table extraction for",
        "desc": "Document types to skip table extraction for, for example `pdf`, `jpg` or "
            "`heic`.",
        "type": AbstractDataProviderTypeMap."*softlist",
    },
    "hi_res_model_name": {
        "display_name": "Hi-Res Model Name",
        "short_desc": "Layout model to use with the hi_res strategy",
        "desc": "Name of the layout inference model used when `strategy` is `hi_res`.",
        "type": AbstractDataProviderTypeMap."*softstring",
        "example_value": "yolox",
    },
    "vlm_model_provider": {
        "display_name": "VLM Provider",
        "short_desc": "Vision language model provider",
        "desc": "Provider of the vision language model used when `strategy` is `vlm`.",
        "type": AbstractDataProviderTypeMap."*softstring",
        "allowed_values": (
            <AllowedValueInfo>{"value": "openai", "display_name": "OpenAI"},
            <AllowedValueInfo>{"value": "anthropic", "display_name": "Anthropic"},
            <AllowedValueInfo>{"value": "bedrock", "display_name": "AWS Bedrock"},
            <AllowedValueInfo>{"value": "anthropic_bedrock", "display_name": "Anthropic on Bedrock"},
            <AllowedValueInfo>{"value": "vertexai", "display_name": "Google Vertex AI"},
            <AllowedValueInfo>{"value": "google", "display_name": "Google"},
            <AllowedValueInfo>{"value": "azure_openai", "display_name": "Azure OpenAI"},
        ),
    },
    "vlm_model": {
        "display_name": "VLM Model",
        "short_desc": "Vision language model to use",
        "desc": "Vision language model to use when `strategy` is `vlm`; must be available from "
            "`vlm_model_provider`.",
        "type": AbstractDataProviderTypeMap."*softstring",
        "example_value": "gpt-4o",
    },
    "xml_keep_tags": {
        "display_name": "Keep XML Tags",
        "short_desc": "Retain XML tags in extracted text",
        "desc": "When `True`, XML tags are retained in the extracted text instead of being "
            "stripped.",
        "type": AbstractDataProviderTypeMap."*softbool",
    },
    "include_slide_notes": {
        "display_name": "Include Slide Notes",
        "short_desc": "Include presenter notes from presentations",
        "desc": "When `True` (the API default), presenter notes in PPTX documents are "
            "partitioned along with the slides.",
        "type": AbstractDataProviderTypeMap."*softbool",
    },
    "encoding": {
        "display_name": "Text Encoding",
        "short_desc": "Character encoding used to decode the input",
        "desc": "Character encoding used to decode the document text; defaults to `utf-8`.",
        "type": AbstractDataProviderTypeMap."*softstring",
        "example_value": "utf-8",
    },
    "max_characters": {
        "display_name": "Maximum Chunk Size",
        "short_desc": "Hard character limit per chunk",
        "desc": "Hard maximum number of characters in a chunk; a larger element is split.  "
            "Applies only when `chunking_strategy` is set.",
        "type": AbstractDataProviderTypeMap."*softint",
        "example_value": 500,
    },
    "new_after_n_chars": {
        "display_name": "Soft Chunk Size",
        "short_desc": "Soft character limit per chunk",
        "desc": "Soft maximum: a chunk is closed once it reaches this length, without "
            "splitting the element that crosses it.  Applies only when `chunking_strategy` "
            "is set.",
        "type": AbstractDataProviderTypeMap."*softint",
        "example_value": 1500,
    },
    "combine_under_n_chars": {
        "display_name": "Combine Under N Characters",
        "short_desc": "Merge chunks smaller than this",
        "desc": "Consecutive chunks are combined until they reach this length, so that a "
            "short section does not become a chunk of its own.  Applies only when "
            "`chunking_strategy` is set.",
        "type": AbstractDataProviderTypeMap."*softint",
        "example_value": 500,
    },
    "overlap": {
        "display_name": "Chunk Overlap",
        "short_desc": "Characters carried over between chunks",
        "desc": "Number of characters drawn from the end of one chunk and prefixed to the "
            "next, so that context is not lost at a chunk boundary.  By default this applies "
            "only to chunks produced by splitting an oversized element; see `overlap_all`.",
        "type": AbstractDataProviderTypeMap."*softint",
        "example_value": 0,
    },
    "overlap_all": {
        "display_name": "Overlap All Chunks",
        "short_desc": "Apply the overlap between every pair of chunks",
        "desc": "When `True`, `overlap` is applied between every pair of adjacent chunks, not "
            "only those produced by splitting an oversized element.",
        "type": AbstractDataProviderTypeMap."*softbool",
    },
    "multipage_sections": {
        "display_name": "Multipage Sections",
        "short_desc": "Allow a chunk to span pages",
        "desc": "When `True` (the API default), a chunk may span more than one page.",
        "type": AbstractDataProviderTypeMap."*softbool",
    },
    "include_orig_elements": {
        "display_name": "Include Original Elements",
        "short_desc": "Return the elements each chunk was built from",
        "desc": "When `True` (the API default), each chunk carries the elements it was "
            "assembled from in `metadata.orig_elements`.",
        "type": AbstractDataProviderTypeMap."*softbool",
    },
    "similarity_threshold": {
        "display_name": "Similarity Threshold",
        "short_desc": "Minimum similarity for by_similarity chunking",
        "desc": "Value between `0.0` and `1.0` giving the minimum similarity two elements "
            "must have to land in the same chunk.  Applies only when `chunking_strategy` is "
            "`by_similarity`.",
        "type": AbstractDataProviderTypeMap."*softfloat",
        "example_value": 0.5,
    },
    "skip_table_chunking": {
        "display_name": "Skip Table Chunking",
        "short_desc": "Leave table elements unchunked",
        "desc": "When `True`, `Table` elements are returned whole instead of being chunked.",
        "type": AbstractDataProviderTypeMap."*softbool",
    },
};

public class UnstructuredDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "Unstructured.io API key",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Unstructured.io API key (sent as `unstructured-api-key` header).",
                "required": True,
                "sensitive": True,
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "Server URL",
                "short_desc": "Unstructured.io REST API URL",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "URL of the Unstructured.io REST API.",
                "default_value": UnstructuredRestClient::UnstructuredRestClientBase::DefaultUrl,
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("UnstructuredRestClient::UnstructuredRestClientIo"),
                    NOTHING, {DTT_ClientOnly: True}),
                "desc": "The Unstructured REST client to use",
            },
            "platform_url": <DataProviderOptionInfo>{
                "display_name": "Platform API URL",
                "short_desc": "Overrides the derived Unstructured Platform API URL",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "URL of the Unstructured Platform API, which manages connectors, workflows "
                    "and jobs.  Derived from `url` unless set here.",
            },
            "platformclient": <DataProviderOptionInfo>{
                "display_name": "Platform REST Client",
                "short_desc": "The Platform API REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("UnstructuredRestClient::UnstructuredPlatformRestClientIo"),
                    NOTHING, {DTT_ClientOnly: True}),
                "desc": "The Unstructured Platform API REST client to use",
            },
        };
    }

    private {
        *UnstructuredRestClient::UnstructuredRestClientIo rest;

        #! The Platform API client, built on demand from the same credentials
        *UnstructuredRestClient::UnstructuredPlatformRestClientIo platform_rest;

        #! Options to build the Platform API client from, when one was not supplied
        *hash<auto> platform_opts;
    }

    constructor(UnstructuredRestClient::UnstructuredRestClientIo rest) {
        self.rest = rest;
    }

    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        } else {
            if (!copts.apikey.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "missing required 'apikey' argument for Unstructured API "
                    "communication";
            }
            rest = new UnstructuredRestClient::UnstructuredRestClientIo({
                "url": copts.url,
                "apikey": copts.apikey,
            });
        }
        if (copts.platformclient) {
            platform_rest = copts.platformclient;
        } else if (copts.apikey.val()) {
            platform_opts = {
                "apikey": copts.apikey,
                "url": copts.url,
                "platform_url": copts.platform_url,
            };
        }
    }

    #! Returns the Platform API client, building it on first use
    /** @throw PLATFORM-CLIENT-ERROR the provider was built from a partition client alone, so there are
        no credentials to build a Platform API client from; supply \c platformclient or \c apikey
    */
    UnstructuredRestClient::UnstructuredPlatformRestClientIo getPlatformClient() {
        if (!platform_rest) {
            if (!platform_opts) {
                throw "PLATFORM-CLIENT-ERROR", "this data provider was created from a partition API "
                    "client alone, which carries no credentials for the Unstructured Platform API; "
                    "create it from a connection, or pass 'apikey' or 'platformclient'";
            }
            platform_rest = new UnstructuredRestClient::UnstructuredPlatformRestClientIo(platform_opts);
        }
        return platform_rest;
    }

    #! Sets the Platform API client
    setPlatformClient(UnstructuredRestClient::UnstructuredPlatformRestClientIo platform_rest) {
        self.platform_rest = platform_rest;
    }
}

public class UnstructuredDataProvider inherits UnstructuredDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "unstructured",
            "type": "UnstructuredDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
            "children_can_support_observers": True,
        };
    }

    private:internal {
        #! the process-wide registry of child data providers; private:internal, so that only this class
        #! changes it (qore#6744)
        static hash<string, Reflection::Class> childMap = {
            "partition-text":
                Class::forName("UnstructuredDataProvider::UnstructuredPartitionTextDataProvider"),
            "partition-file":
                Class::forName("UnstructuredDataProvider::UnstructuredPartitionFileDataProvider"),
        };
    }

    private {
    }

    constructor(UnstructuredRestClient::UnstructuredRestClientIo rest)
            : UnstructuredDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : UnstructuredDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }

    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return (map $1.getConstant("ProviderSummaryInfo").getValue(), childMap.iterator())
            + cast<hash<DataProviderSummaryInfo>>(UnstructuredPlatformDataProvider::ProviderInfo{
                AbstractDataProvider::DataProviderSummaryInfoKeys
            });
    }
    private *list<string> getChildProviderNamesImpl() {
        return (keys childMap) + UnstructuredPlatformDataProvider::ProviderName;
    }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        if (name == UnstructuredPlatformDataProvider::ProviderName) {
            return new UnstructuredPlatformDataProvider(getPlatformClient());
        }
        *Class cls = childMap{name};
        return cls ? cls.newObject(rest) : NOTHING;
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class UnstructuredDataProviderFactory inherits AbstractDataProviderFactory {
    private {
        static Class cls = new Class("UnstructuredDataProvider");
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "unstructured",
            "desc": "Unstructured.io data provider factory",
            "children_can_support_apis": True,
        };
    }
    private hash<DataProviderFactoryInfo> getInfoImpl() { return FactoryInfo; }
    private hash<DataProviderInfo> getProviderInfoImpl() { return UnstructuredDataProvider::ProviderInfo; }
    private Class getClassImpl() { return cls; }
}

public class UnstructuredPartitionTextRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "text": {
                "display_name": "Text",
                "short_desc": "Document text content to partition",
                "desc": "Raw text content of the document to partition.\n\n"
                    "Set `content_type` when the text carries structure — Markdown, HTML or "
                    "CSV — otherwise it is submitted as plain text and its markup is not "
                    "parsed.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "Section One\n\nHello world, this is a test document.",
            },
        } + UnstructuredPartitionOptionFields;
    }

    constructor() : HashDataType("UnstructuredPartitionTextRequest") {
        addQoreFields(Fields);
    }
}

public class UnstructuredPartitionFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file_data": {
                "display_name": "File Data",
                "short_desc": "Document content to partition",
                "desc": "Raw content of the document to partition — a PDF, DOCX, PPTX, XLSX, "
                    "image, email or any other format Unstructured supports.\n\n"
                    "Mutually exclusive with `file_path`; exactly one of the two is required.",
                "type": AbstractDataProviderTypeMap."*binary",
            },
            "file_path": {
                "display_name": "File Path",
                "short_desc": "Local path of the document to partition",
                "desc": "Local path of the document to read and partition.\n\n"
                    "Mutually exclusive with `file_data`; exactly one of the two is required.",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "/tmp/report.pdf",
            },
            "filename": {
                "display_name": "File Name",
                "short_desc": "Name to report for the document",
                "desc": "Name reported to Unstructured for the document and returned in "
                    "`metadata.filename`.  Defaults to the base name of `file_path`, or to "
                    "`document` when `file_data` is used.",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "report.pdf",
            },
        } + UnstructuredPartitionOptionFields;
    }

    constructor() : HashDataType("UnstructuredPartitionFileRequest") {
        addQoreFields(Fields);
    }
}

public class UnstructuredElementMetadataDataType inherits HashDataType {
    private {
        const Fields = {
            "filename": {
                "display_name": "File Name",
                "short_desc": "Name of the source document",
                "desc": "Name of the source document the element came from.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "filetype": {
                "display_name": "File Type",
                "short_desc": "MIME type of the source document",
                "desc": "MIME type Unstructured detected for the source document, "
                    "for example `application/pdf`.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "languages": {
                "display_name": "Languages",
                "short_desc": "Languages detected in the element",
                "desc": "ISO 639 language codes detected in the element's text.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "page_number": {
                "display_name": "Page Number",
                "short_desc": "1-based page the element appears on",
                "desc": "1-based page the element appears on; present for paginated formats "
                    "such as PDF, DOCX and PPTX.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "parent_id": {
                "display_name": "Parent Element ID",
                "short_desc": "ID of the element this one is nested under",
                "desc": "`element_id` of the element this one is nested under, normally the "
                    "`Title` that heads the section.  Use it to rebuild the document "
                    "hierarchy from the flat element list.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "category_depth": {
                "display_name": "Category Depth",
                "short_desc": "Nesting depth of the element's category",
                "desc": "Nesting depth of the element within its category; `0` is the "
                    "outermost heading or list level.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "text_as_html": {
                "display_name": "Text as HTML",
                "short_desc": "HTML rendering of a table element",
                "desc": "HTML rendering of a `Table` element, preserving its row and column "
                    "structure.  Returned when the table structure was inferred; see "
                    "`pdf_infer_table_structure`.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "image_base64": {
                "display_name": "Image Data",
                "short_desc": "Base64-encoded image extracted from the element",
                "desc": "Base64-encoded image data for the element, returned only for the "
                    "element types named in `extract_image_block_types`.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "image_mime_type": {
                "display_name": "Image MIME Type",
                "short_desc": "MIME type of the extracted image",
                "desc": "MIME type of the data in `image_base64`.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "coordinates": {
                "display_name": "Coordinates",
                "short_desc": "Layout coordinates of the element on the page",
                "desc": "Layout coordinates of the element, returned when the `coordinates` "
                    "option is enabled.  Carries the bounding-box `points` and the "
                    "`system`, `layout_width` and `layout_height` they are expressed in.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "links": {
                "display_name": "Links",
                "short_desc": "Hyperlinks found in the element",
                "desc": "Hyperlinks found in the element's text.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "emphasized_text_contents": {
                "display_name": "Emphasized Text",
                "short_desc": "Emphasized runs of text in the element",
                "desc": "Runs of text that carry emphasis (bold, italic) in the source "
                    "document.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "emphasized_text_tags": {
                "display_name": "Emphasis Tags",
                "short_desc": "Emphasis markers for the emphasized text runs",
                "desc": "Emphasis markers corresponding to `emphasized_text_contents`, for "
                    "example `b` and `i`.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "orig_elements": {
                "display_name": "Original Elements",
                "short_desc": "Compressed elements a chunk was built from",
                "desc": "The elements consolidated to form a `CompositeElement` chunk, as a "
                    "compressed base64 string.  Returned only when a chunking strategy is "
                    "used with `include_orig_elements` enabled.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "url": {
                "display_name": "Source URL",
                "short_desc": "URL the source document was retrieved from",
                "desc": "URL the source document was retrieved from, when applicable.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "last_modified": {
                "display_name": "Last Modified",
                "short_desc": "Last modification timestamp of the source document",
                "desc": "Last modification timestamp reported for the source document.",
                "type": AbstractDataProviderTypeMap."*date",
            },
        };
    }

    constructor() : HashDataType("UnstructuredElementMetadata") {
        addQoreFields(Fields);
        # Unstructured's Element schema declares additionalProperties, and the metadata keys
        # returned depend on the source format and the options used, so unknown keys are kept
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

public class UnstructuredElementDataType inherits HashDataType {
    private {
        const Fields = {
            "type": {
                "display_name": "Type",
                "short_desc": "The element's structural type",
                "desc": "Element type assigned by Unstructured, such as `Title`, "
                    "`NarrativeText`, `ListItem`, `Table`, `Image` or `CompositeElement` "
                    "(returned when a chunking strategy is used).",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "text": {
                "display_name": "Text",
                "short_desc": "The element's text content",
                "desc": "Element text content.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "element_id": {
                "display_name": "Element ID",
                "short_desc": "Unstructured-assigned element ID",
                "desc": "Unstructured-assigned element ID; stable for the same content unless "
                    "`unique_element_ids` is set.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "metadata": {
                "display_name": "Metadata",
                "short_desc": "Per-element metadata",
                "desc": "Per-element metadata; see "
                    "@ref UnstructuredDataProvider::UnstructuredElementMetadataDataType "
                    "\"UnstructuredElementMetadataDataType\".",
                "type": new UnstructuredElementMetadataDataType(),
            },
        };
    }

    constructor() : HashDataType("UnstructuredElement") {
        addQoreFields(Fields);
    }
}

public class UnstructuredPartitionResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "elt_list": {
                "display_name": "Elements",
                "short_desc": "The typed elements the document was partitioned into",
                "desc": "List of typed elements returned by Unstructured.io, in document order.\n\n"
                    "When a chunking strategy is used, the elements are `CompositeElement` "
                    "chunks rather than the raw element tree.",
                "type": new ListDataType("UnstructuredElementList",
                    new UnstructuredElementDataType(), True),
            },
        };
    }

    constructor() : HashDataType("UnstructuredPartitionResponse") {
        addQoreFields(Fields);
    }
}

public class UnstructuredPartitionTextDataProvider inherits UnstructuredDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "partition-text",
            "type": "UnstructuredPartitionTextDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        static UnstructuredPartitionTextRequestDataType RequestType =
            new UnstructuredPartitionTextRequestDataType();
        static UnstructuredPartitionResponseDataType ResponseType =
            new UnstructuredPartitionResponseDataType();
    }

    constructor(UnstructuredRestClient::UnstructuredRestClientIo rest)
            : UnstructuredDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : UnstructuredDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string text = req.text;
        hash<auto> opts = req - "text";
        list<hash<auto>> elt_list = rest.partitionText(text, opts);
        return {"elt_list": elt_list};
    }
}

#! Partitions a document file into a typed element tree
/** This is the action to use for PDFs, Office documents, images, emails and every other binary
    format Unstructured supports; @ref UnstructuredPartitionTextDataProvider is for content
    already held as text.
*/
public class UnstructuredPartitionFileDataProvider inherits UnstructuredDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "partition-file",
            "type": "UnstructuredPartitionFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        static UnstructuredPartitionFileRequestDataType RequestType =
            new UnstructuredPartitionFileRequestDataType();
        static UnstructuredPartitionResponseDataType ResponseType =
            new UnstructuredPartitionResponseDataType();

        #! File name reported when the content is supplied inline with no name
        const DefaultFilename = "document";
    }

    constructor(UnstructuredRestClient::UnstructuredRestClientIo rest)
            : UnstructuredDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : UnstructuredDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Partitions the document and returns its element list
    /** @throw PARTITION-FILE-ERROR neither or both of \c file_data and \c file_path were given

        @throw REST-RESPONSE-ERROR the server rejected the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (exists req.file_data && req.file_path.val()) {
            throw "PARTITION-FILE-ERROR", "'file_data' and 'file_path' are mutually exclusive; "
                "give exactly one of them";
        }

        data body;
        *string filename = req.filename;
        if (exists req.file_data) {
            body = req.file_data;
            if (!filename.val()) {
                filename = DefaultFilename;
            }
        } else if (req.file_path.val()) {
            body = ReadOnlyFile::readBinaryFile(req.file_path);
            if (!filename.val()) {
                filename = basename(req.file_path);
            }
        } else {
            throw "PARTITION-FILE-ERROR", "one of 'file_data' or 'file_path' is required";
        }

        # NOTE the content type is both the multipart part's type and Unstructured's own
        # "content_type" hint, so it stays in the options as well
        string content_type = req.content_type ?? MimeTypeUnknown;
        hash<auto> opts = req - ("file_data", "file_path", "filename");
        list<hash<auto>> elt_list = rest.partition(filename, content_type, body, opts);
        return {"elt_list": elt_list};
    }
}
}
