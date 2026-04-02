# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SegmentDataProvider module definition

/*  SegmentLabelDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SegmentDataProvider module
public namespace SegmentDataProvider {

#! Segment label record type
public class SegmentLabelDataType inherits HashDataType {
    public {
        #! Field definitions for Segment labels
        const Fields = {
            "key": {
                "type": StringOrNothingType,
                "display_name": "Key",
                "short_desc": "Label key",
                "desc": "The key of the label",
            },
            "value": {
                "type": StringOrNothingType,
                "display_name": "Value",
                "short_desc": "Label value",
                "desc": "The value of the label",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Label description",
                "desc": "A description of the label",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SegmentDataProvider
