# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyCreateFormRequestDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Create form request data type
public class TallyCreateFormRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "title": {
                "display_name": "Form Title",
                "short_desc": "The title of the form",
                "desc": "The title displayed at the top of the form; automatically creates the required "
                    "`FORM_TITLE` block as the first block",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "blocks": {
                "display_name": "Blocks",
                "short_desc": "Form content blocks",
                "desc": "List of form content blocks defining the form structure (questions, text, images, "
                    "etc.); a `FORM_TITLE` block is automatically prepended from the `title` field",
                "type": new ListDataType("TallyBlockList", new TallyBlockDataType(), True),
                "required": False,
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Initial form status",
                "desc": "The initial status of the form",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "BLANK", "display_name": "Blank"},
                    <AllowedValueInfo>{"value": "DRAFT", "display_name": "Draft"},
                    <AllowedValueInfo>{"value": "PUBLISHED", "display_name": "Published"},
                ),
            },
            "workspaceId": {
                "display_name": "Workspace ID",
                "short_desc": "Target workspace",
                "desc": "The workspace to create the form in; defaults to the user's default workspace",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "templateId": {
                "display_name": "Template ID",
                "short_desc": "Template to base form on",
                "desc": "Optional template ID to use as a base for the new form",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "settings": {
                "display_name": "Settings",
                "short_desc": "Form settings",
                "desc": "Form configuration including closure, notifications, and display options",
                "type": new TallyFormSettingsDataType(AutoHashOrNothingType),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const TallyCreateFormRequestDataType = new TallyCreateFormRequestDataType();
}
