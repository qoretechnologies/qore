# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BgeM3FastApiDataProvider module definition

/** BgeM3FastApiEmbeddingsRequestDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the BgeM3FastApiDataProvider module
public namespace BgeM3FastApiDataProvider {
#! Data type for BGE-M3 FastAPI \c /embed requests
public class BgeM3FastApiEmbeddingsRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "texts": {
                "display_name": "Texts",
                "short_desc": "List of input texts to embed",
                "desc": "List of texts to convert into embedding vectors.\n\n"
                    "**Example**: `[\"How do I reset my password?\", \"Forgot password steps\"]`",
                "type": AbstractDataProviderTypeMap."softlist",
                "required": True,
                "example_value": ("The quick brown fox jumps over the lazy dog",),
            },
            "return_dense": {
                "display_name": "Return Dense Vectors",
                "short_desc": "Whether to return dense vectors",
                "desc": "If `True` (the default), the response includes a `dense` array "
                    "with one float vector per input text.  Set to `False` only if you "
                    "want sparse vectors exclusively.",
                "type": AbstractDataProviderTypeMap."*softbool",
                "default_value": True,
            },
            "return_sparse": {
                "display_name": "Return Sparse Vectors",
                "short_desc": "Whether to return sparse vectors for hybrid retrieval",
                "desc": "If `True`, the response includes a `sparse` array of "
                    "term-weight maps suitable for hybrid retrieval (combining BM25-style "
                    "lexical scoring with dense semantic similarity).  Default `True` on "
                    "the sidecar; pass `False` if you only need dense vectors.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
            "batch_size": {
                "display_name": "Batch Size",
                "short_desc": "Sidecar internal batch size",
                "desc": "Maximum number of texts the sidecar processes per internal "
                    "encoder batch.  When unset, the sidecar's own default (`256`) is "
                    "used.  Lower values reduce memory pressure at the cost of "
                    "throughput.",
                "type": AbstractDataProviderTypeMap."*int",
                "example_value": 256,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
