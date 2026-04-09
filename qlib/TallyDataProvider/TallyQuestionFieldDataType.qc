# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyQuestionFieldDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Question field data type
public class TallyQuestionFieldDataType inherits HashDataType {
    public {
        const Fields = {
            "uuid": {
                "display_name": "Field UUID",
                "short_desc": "Unique field identifier",
                "desc": "The unique identifier for the question field",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "21dd98ef-4c54-4e77-bcc6-7ec79409b3ea",
            },
            "type": {
                "display_name": "Field Type",
                "short_desc": "The field block type",
                "desc": "The block type of the field (e.g., `INPUT_FIELD`, `MULTIPLE_CHOICE_OPTION`, `CHECKBOX`)",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "INPUT_FIELD",
            },
            "blockGroupUuid": {
                "display_name": "Block Group UUID",
                "short_desc": "Parent block group identifier",
                "desc": "The UUID of the parent block group this field belongs to",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
            },
            "title": {
                "display_name": "Title",
                "short_desc": "The field title",
                "desc": "The display title of the question field",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "First name",
            },
        };
    }

    #! Creates the object
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}
}
