# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GroqListModelsResponseDataType class definition

/** GroqListModelsResponseDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GroqDataProvider module
public namespace GroqDataProvider {
#! Data type for a single model entry returned by the Groq `/v1/models` endpoint
public class GroqModelDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "Model ID",
                "desc": "The model ID; use this value in the `model` field of requests.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "object": {
                "display_name": "Object Type",
                "desc": "Object type discriminator (typically 'model').",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "created": {
                "display_name": "Created",
                "desc": "When the model was created.  Groq emits a Unix-seconds integer on the "
                    "wire; the data provider converts it to a Qore date before returning.",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "owned_by": {
                "display_name": "Owned By",
                "desc": "The organization that owns the model.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "active": {
                "display_name": "Active",
                "desc": "Whether the model is currently active and can serve requests.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
            "context_window": {
                "display_name": "Context Window",
                "desc": "The model's context window size in tokens.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "max_completion_tokens": {
                "display_name": "Max Completion Tokens",
                "desc": "The maximum number of completion tokens the model can emit.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "public_apps": {
                "display_name": "Public Apps",
                "desc": "Public application information for the model, if any.",
                "type": AbstractDataProviderTypeMap."auto",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for the Groq `/v1/models` list response
public class GroqListModelsResponseDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "object": {
                "display_name": "Object Type",
                "desc": "Object type discriminator (typically 'list').",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "data": {
                "display_name": "Models",
                "desc": "The list of models currently served by the Groq API.",
                "type": new ListDataType("GroqModelList", new GroqModelDataType(), True),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
