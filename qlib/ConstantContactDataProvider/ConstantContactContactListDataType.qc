# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ConstantContactContactListDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Constant Contact contact list data type
public class ConstantContactContactListDataType inherits HashDataType {
    public {
        const Fields = {
            "list_id": {
                "type": StringType,
                "display_name": "List ID",
                "short_desc": "The unique list identifier",
                "desc": "The unique identifier for the contact list (UUID format)",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The list name",
                "desc": "The name of the contact list",
                "example_value": "Newsletter Subscribers",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The list description",
                "desc": "A description of the contact list",
            },
            "favorite": {
                "type": BoolOrNothingType,
                "display_name": "Favorite",
                "short_desc": "Whether the list is a favorite",
                "desc": "Whether the contact list is marked as a favorite. "
                    "`True` if marked as favorite, `False` otherwise",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the list was created",
                "desc": "The timestamp when the contact list was created (ISO-8601 format)",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the list was last updated",
                "desc": "The timestamp when the contact list was last updated (ISO-8601 format)",
            },
            "membership_count": {
                "type": IntOrNothingType,
                "display_name": "Membership Count",
                "short_desc": "Number of contacts in the list",
                "desc": "The number of contacts that are members of the list",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact create contact list request data type
public class ConstantContactCreateContactListRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The list name",
                "desc": "The name of the contact list (must be unique)",
                "example_value": "Newsletter Subscribers",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The list description",
                "desc": "A description of the contact list",
            },
            "favorite": {
                "type": SoftBoolOrNothingType,
                "display_name": "Favorite",
                "short_desc": "Mark as favorite",
                "desc": "Set to `True` to mark the list as a favorite",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact update contact list request data type
public class ConstantContactUpdateContactListRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "list_id": {
                "type": StringType,
                "display_name": "List ID",
                "short_desc": "The list ID to update",
                "desc": "The unique identifier of the contact list to update",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The list name",
                "desc": "The name of the contact list",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The list description",
                "desc": "A description of the contact list",
            },
            "favorite": {
                "type": SoftBoolOrNothingType,
                "display_name": "Favorite",
                "short_desc": "Mark as favorite",
                "desc": "Set to `True` to mark the list as a favorite",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ConstantContactDataProvider
