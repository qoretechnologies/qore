# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore HuggingFaceDataProvider shared data type definitions

/** HuggingFaceDataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the HuggingFaceDataProvider module
public namespace HuggingFaceDataProvider {
#! Request type for listing resources with search/filter/pagination
public class HuggingFaceListRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "search": {
                "display_name": "Search",
                "short_desc": "Filter by search query",
                "desc": "A string that will be contained in the returned models, datasets, or spaces",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "llama",
            },
            "author": {
                "display_name": "Author",
                "short_desc": "Filter by author or organization",
                "desc": "Filter results by a specific author or organization (e.g. `meta-llama`)",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "meta-llama",
            },
            "filter": {
                "display_name": "Filter",
                "short_desc": "Filter by tags",
                "desc": "Filter by tags. Multiple tags can be comma-separated "
                    "(e.g. `text-generation,pytorch`)",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "text-generation",
            },
            "sort": {
                "display_name": "Sort",
                "short_desc": "Property to use for sorting",
                "desc": "Property to sort results by",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "downloads", "display_name": "Downloads"},
                    <AllowedValueInfo>{"value": "likes", "display_name": "Likes"},
                    <AllowedValueInfo>{"value": "lastModified", "display_name": "Last Modified"},
                    <AllowedValueInfo>{"value": "trendingScore", "display_name": "Trending"},
                    <AllowedValueInfo>{"value": "createdAt", "display_name": "Created At"},
                ),
            },
            "direction": {
                "display_name": "Direction",
                "short_desc": "Sort direction",
                "desc": "Sort direction",
                "type": AbstractDataProviderTypeMap."*int",
                "allowed_values": (
                    <AllowedValueInfo>{"value": -1, "display_name": "Descending"},
                ),
                "default_value": -1,
            },
            "limit": {
                "display_name": "Limit",
                "short_desc": "Maximum number of results to return",
                "desc": "Maximum number of results to return. Default: `30`",
                "type": AbstractDataProviderTypeMap."*int",
                "default_value": 30,
                "example_value": 10,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceListRequestDataType = new HuggingFaceListRequestDataType();

#! Request type for listing models with model-specific filters
public class HuggingFaceModelListRequestDataType inherits HashDataType {
    public {
        const Fields = HuggingFaceListRequestDataType::Fields + {
            "pipeline_tag": {
                "display_name": "Pipeline Tag",
                "short_desc": "Filter by ML task type",
                "desc": "Filter models by their pipeline task type (e.g. `text-generation`, "
                    "`text-classification`, `image-classification`)",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "text-generation",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceModelListRequestDataType = new HuggingFaceModelListRequestDataType();

#! Request type for getting a single resource by repo ID
public class HuggingFaceRepoIdRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "repo_id": {
                "display_name": "Repository ID",
                "short_desc": "Repository ID in namespace/name format",
                "desc": "The repository ID in `namespace/name` format (e.g. `meta-llama/Llama-3.1-8B`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "meta-llama/Llama-3.1-8B",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceRepoIdRequestDataType = new HuggingFaceRepoIdRequestDataType();

#! Request type for getting a model by repo ID with reference data
public class HuggingFaceModelIdRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "repo_id": {
                "display_name": "Repository ID",
                "short_desc": "Model repository ID in namespace/name format",
                "desc": "The model repository ID in `namespace/name` format "
                    "(e.g. `meta-llama/Llama-3.1-8B`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "models",
                "example_value": "meta-llama/Llama-3.1-8B",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceModelIdRequestDataType = new HuggingFaceModelIdRequestDataType();

#! Request type for getting a dataset by repo ID with reference data
public class HuggingFaceDatasetIdRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "repo_id": {
                "display_name": "Repository ID",
                "short_desc": "Dataset repository ID in namespace/name format",
                "desc": "The dataset repository ID in `namespace/name` format "
                    "(e.g. `stanfordnlp/imdb`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "datasets",
                "example_value": "stanfordnlp/imdb",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceDatasetIdRequestDataType = new HuggingFaceDatasetIdRequestDataType();

#! Request type for getting a space by repo ID with reference data
public class HuggingFaceSpaceIdRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "repo_id": {
                "display_name": "Repository ID",
                "short_desc": "Space repository ID in namespace/name format",
                "desc": "The Space repository ID in `namespace/name` format "
                    "(e.g. `gradio/chatbot`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "spaces",
                "example_value": "gradio/chatbot",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceSpaceIdRequestDataType = new HuggingFaceSpaceIdRequestDataType();

#! Request type for creating a repository
public class HuggingFaceRepoCreateRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "display_name": "Name",
                "short_desc": "Repository name",
                "desc": "The name of the repository to create",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "my-model",
            },
            "organization": {
                "display_name": "Organization",
                "short_desc": "Organization namespace",
                "desc": "Organization namespace. If not specified, the repository will be created "
                    "under the authenticated user's namespace",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "private": {
                "display_name": "Private",
                "short_desc": "Whether the repository is private",
                "desc": "Whether the repository should be private. Default: `False`",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "license": {
                "display_name": "License",
                "short_desc": "Repository license identifier",
                "desc": "The license identifier for the repository "
                    "(e.g. `apache-2.0`, `mit`, `gpl-3.0`, `cc-by-4.0`)",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "apache-2.0",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceRepoCreateRequestDataType = new HuggingFaceRepoCreateRequestDataType();

#! Request type for creating a space (extends repo create with sdk/hardware)
public class HuggingFaceSpaceCreateRequestDataType inherits HashDataType {
    public {
        const Fields = HuggingFaceRepoCreateRequestDataType::Fields + {
            "space_sdk": {
                "display_name": "SDK",
                "short_desc": "Space SDK",
                "desc": "The SDK to use for the Space",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "gradio", "display_name": "Gradio"},
                    <AllowedValueInfo>{"value": "streamlit", "display_name": "Streamlit"},
                    <AllowedValueInfo>{"value": "docker", "display_name": "Docker"},
                    <AllowedValueInfo>{"value": "static", "display_name": "Static"},
                ),
                "example_value": "gradio",
            },
            "space_hardware": {
                "display_name": "Hardware",
                "short_desc": "Space hardware tier",
                "desc": "The hardware tier for the Space",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "cpu-basic", "display_name": "CPU Basic"},
                    <AllowedValueInfo>{"value": "cpu-upgrade", "display_name": "CPU Upgrade"},
                    <AllowedValueInfo>{"value": "cpu-performance", "display_name": "CPU Performance"},
                    <AllowedValueInfo>{"value": "cpu-xl", "display_name": "CPU XL"},
                    <AllowedValueInfo>{"value": "zero-a10g", "display_name": "ZeroGPU A10G"},
                    <AllowedValueInfo>{"value": "t4-small", "display_name": "T4 Small"},
                    <AllowedValueInfo>{"value": "t4-medium", "display_name": "T4 Medium"},
                    <AllowedValueInfo>{"value": "l4x1", "display_name": "L4 x1"},
                    <AllowedValueInfo>{"value": "l4x4", "display_name": "L4 x4"},
                    <AllowedValueInfo>{"value": "l40sx1", "display_name": "L40S x1"},
                    <AllowedValueInfo>{"value": "l40sx4", "display_name": "L40S x4"},
                    <AllowedValueInfo>{"value": "l40sx8", "display_name": "L40S x8"},
                    <AllowedValueInfo>{"value": "a10g-small", "display_name": "A10G Small"},
                    <AllowedValueInfo>{"value": "a10g-large", "display_name": "A10G Large"},
                    <AllowedValueInfo>{"value": "a10g-largex2", "display_name": "A10G Large x2"},
                    <AllowedValueInfo>{"value": "a10g-largex4", "display_name": "A10G Large x4"},
                    <AllowedValueInfo>{"value": "a100-large", "display_name": "A100 Large"},
                    <AllowedValueInfo>{"value": "a100x4", "display_name": "A100 x4"},
                    <AllowedValueInfo>{"value": "a100x8", "display_name": "A100 x8"},
                    <AllowedValueInfo>{"value": "h200", "display_name": "H200"},
                    <AllowedValueInfo>{"value": "h200x2", "display_name": "H200 x2"},
                    <AllowedValueInfo>{"value": "h200x4", "display_name": "H200 x4"},
                    <AllowedValueInfo>{"value": "h200x8", "display_name": "H200 x8"},
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceSpaceCreateRequestDataType = new HuggingFaceSpaceCreateRequestDataType();

#! Common update fields without repo_id
public const HuggingFaceRepoUpdateFieldsBase = {
    "visibility": {
        "display_name": "Visibility",
        "short_desc": "Repository visibility",
        "desc": "Repository visibility setting",
        "type": AbstractDataProviderTypeMap."*string",
        "allowed_values": (
            <AllowedValueInfo>{"value": "public", "display_name": "Public"},
            <AllowedValueInfo>{"value": "private", "display_name": "Private"},
        ),
    },
    "gated": {
        "display_name": "Gated",
        "short_desc": "Gating mode for the repository",
        "desc": "Gating mode for the repository",
        "type": AbstractDataProviderTypeMap."*string",
        "allowed_values": (
            <AllowedValueInfo>{"value": "false", "display_name": "Not Gated"},
            <AllowedValueInfo>{"value": "auto", "display_name": "Auto"},
            <AllowedValueInfo>{"value": "manual", "display_name": "Manual"},
        ),
    },
};

#! Update fields for Spaces (includes "protected" visibility)
public const HuggingFaceSpaceUpdateFieldsBase = {
    "visibility": {
        "display_name": "Visibility",
        "short_desc": "Space visibility",
        "desc": "Space visibility setting",
        "type": AbstractDataProviderTypeMap."*string",
        "allowed_values": (
            <AllowedValueInfo>{"value": "public", "display_name": "Public"},
            <AllowedValueInfo>{"value": "private", "display_name": "Private"},
            <AllowedValueInfo>{"value": "protected", "display_name": "Protected"},
        ),
    },
    "gated": {
        "display_name": "Gated",
        "short_desc": "Gating mode for the Space",
        "desc": "Gating mode for the Space",
        "type": AbstractDataProviderTypeMap."*string",
        "allowed_values": (
            <AllowedValueInfo>{"value": "false", "display_name": "Not Gated"},
            <AllowedValueInfo>{"value": "auto", "display_name": "Auto"},
            <AllowedValueInfo>{"value": "manual", "display_name": "Manual"},
        ),
    },
};

#! Request type for updating a model repository
public class HuggingFaceModelUpdateRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "repo_id": {
                "display_name": "Repository ID",
                "short_desc": "Model repository ID in namespace/name format",
                "desc": "The model repository ID in `namespace/name` format "
                    "(e.g. `meta-llama/Llama-3.1-8B`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "models",
                "example_value": "my-org/my-model",
            },
        } + HuggingFaceRepoUpdateFieldsBase;
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceModelUpdateRequestDataType = new HuggingFaceModelUpdateRequestDataType();

#! Request type for updating a dataset repository
public class HuggingFaceDatasetUpdateRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "repo_id": {
                "display_name": "Repository ID",
                "short_desc": "Dataset repository ID in namespace/name format",
                "desc": "The dataset repository ID in `namespace/name` format "
                    "(e.g. `stanfordnlp/imdb`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "datasets",
                "example_value": "my-org/my-dataset",
            },
        } + HuggingFaceRepoUpdateFieldsBase;
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceDatasetUpdateRequestDataType = new HuggingFaceDatasetUpdateRequestDataType();

#! Request type for updating a Space
public class HuggingFaceSpaceUpdateRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "repo_id": {
                "display_name": "Repository ID",
                "short_desc": "Space repository ID in namespace/name format",
                "desc": "The Space repository ID in `namespace/name` format "
                    "(e.g. `gradio/chatbot`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "spaces",
                "example_value": "my-org/my-space",
            },
        } + HuggingFaceSpaceUpdateFieldsBase;
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceSpaceUpdateRequestDataType = new HuggingFaceSpaceUpdateRequestDataType();

#! Request type for deleting a model repository
public class HuggingFaceModelDeleteRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "repo_id": {
                "display_name": "Repository ID",
                "short_desc": "Model repository ID in namespace/name format",
                "desc": "The model repository ID in `namespace/name` format to delete",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "models",
                "example_value": "my-org/my-model",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceModelDeleteRequestDataType = new HuggingFaceModelDeleteRequestDataType();

#! Request type for deleting a dataset repository
public class HuggingFaceDatasetDeleteRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "repo_id": {
                "display_name": "Repository ID",
                "short_desc": "Dataset repository ID in namespace/name format",
                "desc": "The dataset repository ID in `namespace/name` format to delete",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "datasets",
                "example_value": "my-org/my-dataset",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceDatasetDeleteRequestDataType = new HuggingFaceDatasetDeleteRequestDataType();

#! Request type for deleting a Space
public class HuggingFaceSpaceDeleteRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "repo_id": {
                "display_name": "Repository ID",
                "short_desc": "Space repository ID in namespace/name format",
                "desc": "The Space repository ID in `namespace/name` format to delete",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "spaces",
                "example_value": "my-org/my-space",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceSpaceDeleteRequestDataType = new HuggingFaceSpaceDeleteRequestDataType();

#! Generic response type for repo create/update/delete operations
public class HuggingFaceRepoResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "url": {
                "display_name": "URL",
                "short_desc": "Repository URL",
                "desc": "The URL of the repository",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "repoUrl": {
                "display_name": "Repository URL",
                "short_desc": "Full repository URL",
                "desc": "The full URL of the repository on Hugging Face Hub",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const HuggingFaceRepoResponseDataType = new HuggingFaceRepoResponseDataType();
}
