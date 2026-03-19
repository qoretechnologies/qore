# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ConstantContactSegmentDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace ConstantContactDataProvider {

#! Constant Contact segment data type
public class ConstantContactSegmentDataType inherits HashDataType {
    public {
        const Fields = {
            "segment_id": {
                "type": IntType,
                "display_name": "Segment ID",
                "short_desc": "The unique segment identifier",
                "desc": "The unique identifier for the segment (integer)",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The segment name",
                "desc": "The name of the segment",
                "example_value": "Active Email Openers",
            },
            "segment_criteria": {
                "type": StringOrNothingType,
                "display_name": "Segment Criteria",
                "short_desc": "The segment filter criteria",
                "desc": "The criteria used to define the segment in a single-string JSON format",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the segment was created",
                "desc": "The timestamp when the segment was created (ISO-8601 format)",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the segment was last updated",
                "desc": "The timestamp when the segment was last updated (ISO-8601 format)",
            },
            "contact_count": {
                "type": IntOrNothingType,
                "display_name": "Contact Count",
                "short_desc": "Number of contacts in the segment",
                "desc": "The approximate number of contacts that match the segment criteria",
            },
            "edited_at": {
                "type": StringOrNothingType,
                "display_name": "Edited At",
                "short_desc": "When criteria were last edited",
                "desc": "The timestamp when the segment criteria was last edited (ISO-8601 format)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact create segment request data type
public class ConstantContactCreateSegmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The segment name",
                "desc": "The name of the segment",
                "example_value": "Active Email Openers",
            },
            "segment_criteria": {
                "type": StringType,
                "display_name": "Segment Criteria",
                "short_desc": "The segment filter criteria",
                "desc": "The criteria used to define the segment in a single-string JSON format",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact update segment request data type
public class ConstantContactUpdateSegmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "segment_id": {
                "type": IntType,
                "display_name": "Segment ID",
                "short_desc": "The segment ID to update",
                "desc": "The unique identifier of the segment to update",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The segment name",
                "desc": "The name of the segment",
            },
            "segment_criteria": {
                "type": StringType,
                "display_name": "Segment Criteria",
                "short_desc": "The segment filter criteria",
                "desc": "The criteria used to define the segment in a single-string JSON format",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ConstantContactDataProvider
