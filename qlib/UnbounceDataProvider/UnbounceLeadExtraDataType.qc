# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbounceLeadExtraDataType.qc Unbounce lead extra data type

/*  UnbounceLeadExtraDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnbounceDataProvider module
public namespace UnbounceDataProvider {

#! Unbounce lead extra data type for additional metadata captured with lead submissions
public class UnbounceLeadExtraDataType inherits HashDataType {
    public {
        const Fields = {
            "rapleaf": {
                "display_name": "Rapleaf",
                "short_desc": "Rapleaf enrichment data",
                "desc": "Enrichment data from Rapleaf (now TowerData) for the lead",
                "type": AutoHashOrNothingType,
            },
            "cookies": {
                "display_name": "Cookies",
                "short_desc": "Browser cookies",
                "desc": "Browser cookie data captured with the lead submission",
                "type": AutoHashOrNothingType,
            },
            "request_id": {
                "display_name": "Request ID",
                "short_desc": "API request identifier",
                "desc": "The request ID when the lead was created via API",
                "type": SoftStringOrNothingType,
            },
            "created_by": {
                "display_name": "Created By",
                "short_desc": "Lead creation source",
                "desc": "How the lead was created",
                "type": SoftStringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "api", "display_name": "API",
                        "desc": "Lead created via the Unbounce API"},
                ),
            },
        };
    }

    constructor() : HashDataType(AutoHashOrNothingType, "UnbounceLeadExtraDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnbounceDataProvider
