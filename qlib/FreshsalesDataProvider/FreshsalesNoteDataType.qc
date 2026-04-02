# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshsalesDataProvider module definition

/*  FreshsalesNoteDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshsalesDataProvider module
public namespace FreshsalesDataProvider {

#! Freshsales note record type
public class FreshsalesNoteDataType inherits HashDataType {
    public {
        #! Field definitions for Freshsales notes
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The note ID",
                "desc": "The unique identifier of the note",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Note content",
                "desc": "The content/body of the note",
            },
            "targetable_id": {
                "type": IntOrNothingType,
                "display_name": "Target ID",
                "short_desc": "Associated record ID",
                "desc": "The ID of the contact, account, or deal associated with this note",
            },
            "targetable_type": {
                "type": StringOrNothingType,
                "display_name": "Target Type",
                "short_desc": "Associated record type",
                "desc": "The type of record associated with this note",
            },
            "creater_id": {
                "type": IntOrNothingType,
                "display_name": "Creator ID",
                "short_desc": "Creator user ID",
                "desc": "The ID of the user who created the note",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The timestamp when the note was created",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "The timestamp when the note was last updated",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Freshsales note request data type for create operations
public class FreshsalesCreateNoteRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a note
        const Fields = {
            "description": {
                "type": StringType,
                "display_name": "Description",
                "required": True,
                "short_desc": "Note content",
                "desc": "The content/body of the note",
                "example_value": "Follow-up call scheduled for next week",
            },
            "targetable_id": {
                "type": SoftIntType,
                "display_name": "Target ID",
                "required": True,
                "short_desc": "Associated record ID",
                "desc": "The ID of the contact, account, or deal to associate",
            },
            "targetable_type": {
                "type": StringType,
                "display_name": "Target Type",
                "required": True,
                "short_desc": "Associated record type",
                "desc": "The type of record to associate: `Contact`, `SalesAccount`, or `Deal`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Contact", "display_name": "Contact"},
                    <AllowedValueInfo>{"value": "SalesAccount", "display_name": "Account"},
                    <AllowedValueInfo>{"value": "Deal", "display_name": "Deal"},
                ),
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Freshsales note request data type for update operations
public class FreshsalesUpdateNoteRequestDataType inherits HashDataType {
    public {
        #! Field definitions for updating a note
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "Note ID",
                "required": True,
                "short_desc": "The note ID to update",
                "desc": "The unique identifier of the note to update",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Note content",
                "desc": "The updated content/body of the note",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Freshsales note request data type for delete operations
public class FreshsalesDeleteNoteRequestDataType inherits HashDataType {
    public {
        #! Field definitions for deleting a note
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "Note ID",
                "required": True,
                "short_desc": "The note ID to delete",
                "desc": "The unique identifier of the note to delete",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FreshsalesDataProvider
