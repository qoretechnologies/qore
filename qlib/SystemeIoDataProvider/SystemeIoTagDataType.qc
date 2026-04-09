# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SystemeIoDataProvider module definition

/*  SystemeIoTagDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SystemeIoDataProvider module
public namespace SystemeIoDataProvider {

#! Systeme.io tag record type
public class SystemeIoTagDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The tag ID",
                "desc": "The unique identifier of the tag",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Tag name",
                "desc": "The name of the tag",
            },
            "createdAt": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The date and time when the tag was created",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Systeme.io create tag request data type
public class SystemeIoCreateTagRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Tag name",
                "desc": "The name of the tag to create",
                "required": True,
                "example_value": "VIP Customer",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Systeme.io update tag request data type
public class SystemeIoUpdateTagRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Tag ID",
                "short_desc": "The tag ID to update",
                "desc": "The unique identifier of the tag to update",
                "required": True,
                "ref_data": "tags",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "New tag name",
                "desc": "The new name for the tag",
                "required": True,
                "example_value": "Premium Customer",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Systeme.io delete tag request data type
public class SystemeIoDeleteTagRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Tag ID",
                "short_desc": "The tag ID to delete",
                "desc": "The unique identifier of the tag to delete",
                "required": True,
                "ref_data": "tags",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Systeme.io assign contact tag request data type
public class SystemeIoAssignContactTagRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID",
                "desc": "The unique identifier of the contact to assign the tag to",
                "required": True,
                "ref_data": "contacts",
            },
            "tagId": {
                "type": IntType,
                "display_name": "Tag ID",
                "short_desc": "The tag ID to assign",
                "desc": "The unique identifier of the tag to assign to the contact",
                "required": True,
                "ref_data": "tags",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Systeme.io remove contact tag request data type
public class SystemeIoRemoveContactTagRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID",
                "desc": "The unique identifier of the contact to remove the tag from",
                "required": True,
                "ref_data": "contacts",
            },
            "tagId": {
                "type": IntType,
                "display_name": "Tag ID",
                "short_desc": "The tag ID to remove",
                "desc": "The unique identifier of the tag to remove from the contact",
                "required": True,
                "ref_data": "tags",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SystemeIoDataProvider
