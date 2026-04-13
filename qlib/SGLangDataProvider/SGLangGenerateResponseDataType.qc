# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SGLangDataProvider module definition

/** SGLangGenerateResponseDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SGLangDataProvider module
public namespace SGLangDataProvider {
#! Data type for SGLang native `/generate` responses
public class SGLangGenerateResponseDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "text": {
                "display_name": "Generated Text",
                "desc": "The generated completion text.  When a constraint (`json_schema`, `regex`, "
                    "`ebnf`) was applied, this string is guaranteed to satisfy it.",
                "type": AbstractDataProviderTypeMap."softstring",
            },
            "meta_info": {
                "display_name": "Meta Info",
                "desc": "Hash of generation metadata including `prompt_tokens`, `completion_tokens`, "
                    "`finish_reason`, `cached_tokens`, and (when requested) per-token log "
                    "probabilities.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "index": {
                "display_name": "Index",
                "desc": "Zero-based index of this completion when multiple prompts or multiple "
                    "samples were requested.",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
