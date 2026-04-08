# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyQuestionDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the TallyDataProvider module
public namespace TallyDataProvider {
#! Question response data type
public class TallyQuestionDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Question ID",
                "short_desc": "Unique question identifier",
                "desc": "The unique identifier for the question",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "type": {
                "display_name": "Block Type",
                "short_desc": "The question block type",
                "desc": "The block type of the question (e.g., `INPUT_TEXT`, `MULTIPLE_CHOICE`, `DROPDOWN`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "title": {
                "display_name": "Title",
                "short_desc": "The question title",
                "desc": "The display title of the question",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "isTitleModifiedByUser": {
                "display_name": "Title Modified",
                "short_desc": "Whether the title was customized",
                "desc": "`True` if the question title was manually modified by the form creator",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "formId": {
                "display_name": "Form ID",
                "short_desc": "The form this question belongs to",
                "desc": "The ID of the form containing this question",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "isDeleted": {
                "display_name": "Is Deleted",
                "short_desc": "Whether the question is deleted",
                "desc": "`True` if the question has been deleted",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "numberOfResponses": {
                "display_name": "Number of Responses",
                "short_desc": "Total response count",
                "desc": "The total number of responses to this question",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "createdAt": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "ISO 8601 timestamp when the question was created",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "updatedAt": {
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "ISO 8601 timestamp when the question was last updated",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "fields": {
                "display_name": "Fields",
                "short_desc": "Question fields",
                "desc": "List of question field objects with metadata including `uuid`, `type`, "
                    "`blockGroupUuid`, and `title`",
                "type": new ListDataType("TallyQuestionFieldList",
                    new TallyQuestionFieldDataType(AutoHashOrNothingType), True),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const TallyQuestionDataType = new TallyQuestionDataType();
}
