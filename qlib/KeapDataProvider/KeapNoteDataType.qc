# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapNoteDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Keap note record type
public class KeapNoteDataType inherits HashDataType {
    private {
        #! Field definitions for Keap notes
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The note ID",
                "desc": "The unique identifier of the note",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The note title",
                "desc": "The title of the note",
                "example_value": "Follow-up call notes",
            },
            "body": {
                "type": StringOrNothingType,
                "display_name": "Body",
                "short_desc": "The note body",
                "desc": "The body content of the note",
            },
            "contact_id": {
                "type": IntOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The associated contact ID",
                "desc": "The ID of the contact this note is associated with",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The note type",
                "desc": "The type of the note",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Appointment"},
                    <AllowedValueInfo>{"value": "Call"},
                    <AllowedValueInfo>{"value": "Email"},
                    <AllowedValueInfo>{"value": "Fax"},
                    <AllowedValueInfo>{"value": "Letter"},
                    <AllowedValueInfo>{"value": "Other"},
                ),
            },
            "date_created": {
                "type": DateOrNothingType,
                "display_name": "Date Created",
                "short_desc": "When the note was created",
                "desc": "The timestamp when the note was created",
            },
            "last_updated": {
                "type": DateOrNothingType,
                "display_name": "Last Updated",
                "short_desc": "When the note was last updated",
                "desc": "The timestamp when the note was last updated",
            },
            "user_id": {
                "type": IntOrNothingType,
                "display_name": "User ID",
                "short_desc": "The user who created the note",
                "desc": "The ID of the user who created the note",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KeapDataProvider
