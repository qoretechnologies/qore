# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore HuggingFaceDataProvider module definition

/** HuggingFaceSpaceDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Space data type for Hugging Face Spaces
public class HuggingFaceSpaceDataType inherits HashDataType {
    public {
        const Fields = {
            "_id": {
                "display_name": "Internal ID",
                "short_desc": "Internal MongoDB ID",
                "desc": "Internal database identifier",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "id": {
                "display_name": "Space ID",
                "short_desc": "Space identifier in namespace/name format",
                "desc": "The Space identifier in `namespace/name` format",
                "type": AbstractDataProviderTypeMap."string",
            },
            "author": {
                "display_name": "Author",
                "short_desc": "Space author or organization",
                "desc": "The author or organization that owns the Space",
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
                "short_desc": "Whether the Space is private",
                "desc": "Whether the Space repository is private",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "disabled": {
                "display_name": "Disabled",
                "short_desc": "Whether the Space is disabled",
                "desc": "Whether the Space has been disabled",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "sdk": {
                "display_name": "SDK",
                "short_desc": "Space SDK type",
                "desc": "The SDK used by the Space (e.g. `gradio`, `streamlit`, `docker`, `static`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "likes": {
                "display_name": "Likes",
                "short_desc": "Number of likes",
                "desc": "The number of likes the Space has received",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "createdAt": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "Timestamp when the Space was created",
                "type": AbstractDataProviderTypeMap."*date",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const HuggingFaceSpaceDataType = new HuggingFaceSpaceDataType();
}
