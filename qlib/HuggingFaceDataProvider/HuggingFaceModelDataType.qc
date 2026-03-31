# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore HuggingFaceDataProvider module definition

/** HuggingFaceModelDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Model data type for Hugging Face models
public class HuggingFaceModelDataType inherits HashDataType {
    public {
        const Fields = {
            "_id": {
                "display_name": "Internal ID",
                "short_desc": "Internal MongoDB ID",
                "desc": "Internal database identifier",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "id": {
                "display_name": "Model ID",
                "short_desc": "Model identifier in namespace/name format",
                "desc": "The model identifier in `namespace/name` format (e.g. `meta-llama/Llama-3.1-8B`)",
                "type": AbstractDataProviderTypeMap."string",
            },
            "modelId": {
                "display_name": "Full Model ID",
                "short_desc": "Full model identifier",
                "desc": "The full model identifier",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "author": {
                "display_name": "Author",
                "short_desc": "Model author or organization",
                "desc": "The author or organization that owns the model",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "sha": {
                "display_name": "SHA",
                "short_desc": "Latest commit SHA",
                "desc": "The SHA hash of the latest commit",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "lastModified": {
                "display_name": "Last Modified",
                "short_desc": "Last modification timestamp",
                "desc": "Timestamp of the last modification",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "private": {
                "display_name": "Private",
                "short_desc": "Whether the model is private",
                "desc": "Whether the model repository is private",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "disabled": {
                "display_name": "Disabled",
                "short_desc": "Whether the model is disabled",
                "desc": "Whether the model has been disabled",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "pipeline_tag": {
                "display_name": "Pipeline Tag",
                "short_desc": "Model pipeline/task type",
                "desc": "The pipeline task type (e.g. `text-generation`, `text-classification`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "downloads": {
                "display_name": "Downloads",
                "short_desc": "Total download count",
                "desc": "The total number of downloads",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "likes": {
                "display_name": "Likes",
                "short_desc": "Number of likes",
                "desc": "The number of likes the model has received",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "library_name": {
                "display_name": "Library Name",
                "short_desc": "ML library name",
                "desc": "The ML library used (e.g. `transformers`, `pytorch`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tags": {
                "display_name": "Tags",
                "short_desc": "Model tags",
                "desc": "A list of tags associated with the model (e.g. `text-generation`, `pytorch`, "
                    "`en`)",
                "type": SoftAutoListOrNothingType,
            },
            "gated": {
                "display_name": "Gated",
                "short_desc": "Gating status",
                "desc": "Gating status of the model. `False` if not gated, or a string value such as "
                    "`auto` or `manual` if gated",
                "type": AbstractDataProviderTypeMap."auto",
            },
            "trendingScore": {
                "display_name": "Trending Score",
                "short_desc": "Trending score",
                "desc": "The trending score of the model on Hugging Face Hub",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "createdAt": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "Timestamp when the model was created",
                "type": AbstractDataProviderTypeMap."*date",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const HuggingFaceModelDataType = new HuggingFaceModelDataType();
}
