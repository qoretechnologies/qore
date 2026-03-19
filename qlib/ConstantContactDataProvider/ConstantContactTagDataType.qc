# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ConstantContactTagDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Constant Contact tag data type
public class ConstantContactTagDataType inherits HashDataType {
    public {
        const Fields = {
            "tag_id": {
                "type": StringType,
                "display_name": "Tag ID",
                "short_desc": "The unique tag identifier",
                "desc": "The unique identifier for the tag (UUID format)",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The tag name",
                "desc": "The name of the tag",
                "example_value": "VIP Customers",
            },
            "contacts_count": {
                "type": IntOrNothingType,
                "display_name": "Contacts Count",
                "short_desc": "Number of contacts with this tag",
                "desc": "The number of contacts that have this tag assigned",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the tag was created",
                "desc": "The timestamp when the tag was created (ISO-8601 format)",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the tag was last updated",
                "desc": "The timestamp when the tag was last updated (ISO-8601 format)",
            },
            "tag_source": {
                "type": StringOrNothingType,
                "display_name": "Tag Source",
                "short_desc": "Source of the tag",
                "desc": "The source that created the tag",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact create tag request data type
public class ConstantContactCreateTagRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The tag name",
                "desc": "The name of the tag to create",
                "example_value": "VIP Customers",
            },
            "tag_source": {
                "type": StringOrNothingType,
                "display_name": "Tag Source",
                "short_desc": "Source of the tag",
                "desc": "The source creating the tag. Default is `Account`",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact update tag request data type
public class ConstantContactUpdateTagRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "tag_id": {
                "type": StringType,
                "display_name": "Tag ID",
                "short_desc": "The tag ID to update",
                "desc": "The unique identifier of the tag to update",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The new tag name",
                "desc": "The new name for the tag",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ConstantContactDataProvider
