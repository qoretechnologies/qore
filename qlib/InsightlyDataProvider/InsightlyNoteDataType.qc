# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyNoteDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {

public class InsightlyNoteDataType inherits HashDataType {
    public {
        const Fields = {
            "NOTE_ID": {
                "type": SoftIntOrNothingType, "display_name": "Note ID",
                "short_desc": "The unique note identifier",
                "desc": "The unique identifier of the note",
            },
            "TITLE": {
                "type": SoftStringOrNothingType, "display_name": "Title",
                "short_desc": "Note title", "desc": "The title of the note",
            },
            "BODY": {
                "type": SoftStringOrNothingType, "display_name": "Body",
                "short_desc": "Note body content", "desc": "The body content of the note",
            },
            "LINK_SUBJECT_ID": {
                "type": SoftIntOrNothingType, "display_name": "Link Subject ID",
                "short_desc": "Linked subject identifier",
                "desc": "The ID of the object this note is linked to",
            },
            "LINK_SUBJECT_TYPE": {
                "type": SoftStringOrNothingType, "display_name": "Link Subject Type",
                "short_desc": "Linked subject type",
                "desc": "The type of object this note is linked to (e.g., `Contact`, "
                    "`Organisation`)",
            },
            "OWNER_USER_ID": {
                "type": SoftIntOrNothingType, "display_name": "Owner User ID",
                "short_desc": "The owner user identifier",
                "desc": "The ID of the user who owns this note",
            },
            "VISIBLE_TO": {
                "type": SoftStringOrNothingType, "display_name": "Visible To",
                "short_desc": "Visibility setting",
                "desc": "Who can see this note",
                "allowed_values": VisibleToAllowedValues,
            },
            "NOTELINKS": {
                "type": new ListDataType("InsightlyNoteLinkList",
                    new InsightlyNoteLinkDataType(), True),
                "display_name": "Note Links",
                "short_desc": "Related object links",
                "desc": "A list of links to related objects",
            },
            "DATE_CREATED_UTC": {
                "type": SoftDateOrNothingType, "display_name": "Date Created",
                "short_desc": "Creation timestamp (UTC)",
                "desc": "The UTC timestamp when the note was created",
            },
            "DATE_UPDATED_UTC": {
                "type": SoftDateOrNothingType, "display_name": "Date Updated",
                "short_desc": "Last update timestamp (UTC)",
                "desc": "The UTC timestamp when the note was last updated",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class InsightlyCreateNoteRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "TITLE": {
                "type": SoftStringType, "display_name": "Title",
                "required": True, "short_desc": "Note title",
                "desc": "The title of the note (required)",
                "example_value": "Meeting notes",
            },
            "BODY": {
                "type": SoftStringOrNothingType, "display_name": "Body",
                "short_desc": "Note body content",
                "desc": "The body content of the note",
                "preselected": True,
            },
            "LINK_SUBJECT_ID": {
                "type": SoftIntOrNothingType, "display_name": "Link Subject ID",
                "short_desc": "Linked subject identifier",
                "desc": "The ID of the object to link this note to",
                "preselected": True,
            },
            "LINK_SUBJECT_TYPE": {
                "type": SoftStringOrNothingType, "display_name": "Link Subject Type",
                "short_desc": "Linked subject type",
                "desc": "The type of object to link this note to (e.g., `Contact`, "
                    "`Organisation`)",
                "preselected": True,
            },
            "OWNER_USER_ID": {
                "type": SoftIntOrNothingType, "display_name": "Owner User ID",
                "short_desc": "The owner user identifier",
                "desc": "The ID of the user who owns this note",
                "ref_data": "users",
            },
            "VISIBLE_TO": {
                "type": SoftStringOrNothingType, "display_name": "Visible To",
                "short_desc": "Visibility setting",
                "desc": "Who can see this note",
                "allowed_values": VisibleToAllowedValues,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class InsightlyUpdateNoteRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "NOTE_ID": {
                "type": SoftIntType, "display_name": "Note ID",
                "required": True, "short_desc": "The note ID to update",
                "desc": "The unique identifier of the note to update",
                "ref_data": "notes",
            },
        } + InsightlyDataProviderBase::makeFieldsOptional(
            InsightlyCreateNoteRequestDataType::Fields);
    }
    constructor() { addQoreFields(Fields); }
}

public class InsightlyDeleteNoteRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "NOTE_ID": {
                "type": SoftIntType, "display_name": "Note ID",
                "required": True, "short_desc": "The note ID to delete",
                "desc": "The unique identifier of the note to delete",
                "ref_data": "notes",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace InsightlyDataProvider
