# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore VoyageDataProvider module definition

/** VoyageRerankResponseDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the VoyageDataProvider module
public namespace VoyageDataProvider {
#! Data type for Voyage AI rerank response
public class VoyageRerankResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "object": {
                "display_name": "Object Type",
                "desc": "Always `list`",
                "type": AbstractDataProviderTypeMap."string",
            },
            "data": {
                "display_name": "Results",
                "desc": "List of reranked results, sorted by relevance score in descending order",
                "type": new ListDataType("VoyageRerankItemList", new VoyageRerankItemDataType(), True),
            },
            "model": {
                "display_name": "Model",
                "desc": "The model used for reranking",
                "type": AbstractDataProviderTypeMap."string",
            },
            "usage": {
                "display_name": "Usage",
                "desc": "Token usage information",
                "type": new VoyageUsageDataType(),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}
}
