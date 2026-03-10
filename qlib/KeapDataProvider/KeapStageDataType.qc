# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapStageDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Keap pipeline stage data type
public class KeapStageDataType inherits HashDataType {
    public {
        #! Field definitions for pipeline stages
        const Fields = {
            "id": {
                "type": IntOrNothingType,
                "display_name": "Stage ID",
                "short_desc": "The stage ID",
                "desc": "The unique identifier of the pipeline stage",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Stage Name",
                "short_desc": "The stage name",
                "desc": "The name of the pipeline stage",
                "example_value": "Prospecting",
            },
            "details": {
                "type": AutoHashOrNothingType,
                "display_name": "Details",
                "short_desc": "Additional stage details",
                "desc": "Additional details about the pipeline stage",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("KeapStageDataType") {
        addQoreFields(Fields);
    }

    #! Creates the type with a base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "KeapStageDataType") {
        addQoreFields(Fields);
    }
}

} # namespace KeapDataProvider
