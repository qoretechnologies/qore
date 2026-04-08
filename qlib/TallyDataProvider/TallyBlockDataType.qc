# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyBlockDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Form content block data type
public class TallyBlockDataType inherits HashDataType {
    public {
        const Fields = {
            "uuid": {
                "display_name": "Block UUID",
                "short_desc": "Unique block identifier",
                "desc": "Unique identifier (UUID) for this block",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "550e8400-e29b-41d4-a716-446655440000",
            },
            "type": {
                "display_name": "Block Type",
                "short_desc": "The type of block",
                "desc": "The block type discriminator that determines the block's behavior and payload "
                    "structure",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "FORM_TITLE", "display_name": "Form Title"},
                    <AllowedValueInfo>{"value": "TEXT", "display_name": "Text"},
                    <AllowedValueInfo>{"value": "LABEL", "display_name": "Label"},
                    <AllowedValueInfo>{"value": "TITLE", "display_name": "Title"},
                    <AllowedValueInfo>{"value": "HEADING_1", "display_name": "Heading 1"},
                    <AllowedValueInfo>{"value": "HEADING_2", "display_name": "Heading 2"},
                    <AllowedValueInfo>{"value": "HEADING_3", "display_name": "Heading 3"},
                    <AllowedValueInfo>{"value": "DIVIDER", "display_name": "Divider"},
                    <AllowedValueInfo>{"value": "PAGE_BREAK", "display_name": "Page Break"},
                    <AllowedValueInfo>{"value": "IMAGE", "display_name": "Image"},
                    <AllowedValueInfo>{"value": "EMBED", "display_name": "Embed"},
                    <AllowedValueInfo>{"value": "EMBED_VIDEO", "display_name": "Embed Video"},
                    <AllowedValueInfo>{"value": "EMBED_AUDIO", "display_name": "Embed Audio"},
                    <AllowedValueInfo>{"value": "QUESTION", "display_name": "Question"},
                    <AllowedValueInfo>{"value": "MATRIX", "display_name": "Matrix"},
                    <AllowedValueInfo>{"value": "INPUT_TEXT", "display_name": "Text Input"},
                    <AllowedValueInfo>{"value": "INPUT_NUMBER", "display_name": "Number Input"},
                    <AllowedValueInfo>{"value": "INPUT_EMAIL", "display_name": "Email Input"},
                    <AllowedValueInfo>{"value": "INPUT_LINK", "display_name": "Link Input"},
                    <AllowedValueInfo>{"value": "INPUT_PHONE_NUMBER", "display_name": "Phone Number Input"},
                    <AllowedValueInfo>{"value": "INPUT_DATE", "display_name": "Date Input"},
                    <AllowedValueInfo>{"value": "INPUT_TIME", "display_name": "Time Input"},
                    <AllowedValueInfo>{"value": "TEXTAREA", "display_name": "Text Area"},
                    <AllowedValueInfo>{"value": "FILE_UPLOAD", "display_name": "File Upload"},
                    <AllowedValueInfo>{"value": "LINEAR_SCALE", "display_name": "Linear Scale"},
                    <AllowedValueInfo>{"value": "RATING", "display_name": "Rating"},
                    <AllowedValueInfo>{"value": "HIDDEN_FIELDS", "display_name": "Hidden Fields"},
                    <AllowedValueInfo>{"value": "MULTIPLE_CHOICE_OPTION",
                        "display_name": "Multiple Choice Option"},
                    <AllowedValueInfo>{"value": "CHECKBOX", "display_name": "Checkbox"},
                    <AllowedValueInfo>{"value": "DROPDOWN_OPTION", "display_name": "Dropdown Option"},
                    <AllowedValueInfo>{"value": "RANKING_OPTION", "display_name": "Ranking Option"},
                    <AllowedValueInfo>{"value": "MULTI_SELECT_OPTION",
                        "display_name": "Multi Select Option"},
                    <AllowedValueInfo>{"value": "PAYMENT", "display_name": "Payment"},
                    <AllowedValueInfo>{"value": "SIGNATURE", "display_name": "Signature"},
                    <AllowedValueInfo>{"value": "MATRIX_ROW", "display_name": "Matrix Row"},
                    <AllowedValueInfo>{"value": "MATRIX_COLUMN", "display_name": "Matrix Column"},
                    <AllowedValueInfo>{"value": "WALLET_CONNECT", "display_name": "Wallet Connect"},
                    <AllowedValueInfo>{"value": "CONDITIONAL_LOGIC",
                        "display_name": "Conditional Logic"},
                    <AllowedValueInfo>{"value": "CALCULATED_FIELDS",
                        "display_name": "Calculated Fields"},
                    <AllowedValueInfo>{"value": "CAPTCHA", "display_name": "Captcha"},
                    <AllowedValueInfo>{"value": "RESPONDENT_COUNTRY",
                        "display_name": "Respondent Country"},
                ),
            },
            "groupUuid": {
                "display_name": "Group UUID",
                "short_desc": "Groups related blocks together",
                "desc": "UUID that groups related blocks together; for standalone blocks, use the block's "
                    "own `uuid`; for child blocks (e.g., dropdown options), use the parent block's `uuid`",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "550e8400-e29b-41d4-a716-446655440000",
            },
            "groupType": {
                "display_name": "Group Type",
                "short_desc": "The group type for this block",
                "desc": "The group type identifier that determines how blocks are grouped; varies per "
                    "block type (e.g., `InputTextBlock`, `MultipleChoiceBlock`, `FormTitleBlock`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "InputTextBlock",
            },
            "payload": {
                "display_name": "Payload",
                "short_desc": "Block-specific configuration",
                "desc": "Type-specific configuration object; common fields include `isRequired` (boolean), "
                    "`isHidden` (boolean), `placeholder` (string), `html` (string for rich text content), "
                    "and `hasDefaultAnswer` (boolean). Option-type blocks also use `index` (number), "
                    "`isFirst` (boolean), and `isLast` (boolean)",
                "type": AbstractDataProviderTypeMap."auto",
                "required": True,
            },
        };
    }

    #! Creates the object
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}
}
