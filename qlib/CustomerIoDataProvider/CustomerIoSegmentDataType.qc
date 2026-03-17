# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoSegmentDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CustomerIoDataProvider module
public namespace CustomerIoDataProvider {

#! Customer.io segment record type
/** Represents a segment in Customer.io.
*/
public class CustomerIoSegmentDataType inherits HashDataType {
    public {
        #! Field definitions for Customer.io segments
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "Segment ID",
                "desc": "The unique identifier for the segment",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Segment name",
                "desc": "The name of the segment",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Description",
                "desc": "The description of the segment",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "State",
                "desc": "The state of the segment (e.g., `finished`)",
            },
            "progress": {
                "type": IntOrNothingType,
                "display_name": "Progress",
                "short_desc": "Progress percentage",
                "desc": "The progress percentage of the segment",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "Segment type",
                "desc": "The type of the segment",
            },
            "deduplicated": {
                "type": BoolOrNothingType,
                "display_name": "Deduplicated",
                "short_desc": "Whether deduplicated",
                "desc": "Whether the segment has been deduplicated",
            },
            "tags": {
                "type": AutoListOrNothingType,
                "display_name": "Tags",
                "short_desc": "Tags",
                "desc": "Tags associated with the segment",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CustomerIoDataProvider
