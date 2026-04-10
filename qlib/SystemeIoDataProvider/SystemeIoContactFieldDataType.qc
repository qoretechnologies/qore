# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SystemeIoDataProvider module definition

/*  SystemeIoContactFieldDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Systeme.io contact field record type
public class SystemeIoContactFieldDataType inherits HashDataType {
    public {
        const Fields = {
            "slug": {
                "type": StringOrNothingType,
                "display_name": "Slug",
                "short_desc": "Field slug identifier",
                "desc": "The unique slug identifier of the custom contact field",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Field display name",
                "desc": "The display name of the custom contact field",
            },
            "createdAt": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The date and time when the field was created",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Systeme.io create contact field request data type
public class SystemeIoCreateContactFieldRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Field display name",
                "desc": "The display name of the custom contact field to create",
                "required": True,
                "example_value": "Company",
            },
            "slug": {
                "type": StringType,
                "display_name": "Slug",
                "short_desc": "Field slug identifier",
                "desc": "The unique slug identifier for the custom contact field",
                "required": True,
                "example_value": "company",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Systeme.io update contact field request data type
public class SystemeIoUpdateContactFieldRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "slug": {
                "type": StringType,
                "display_name": "Slug",
                "short_desc": "Field slug to update",
                "desc": "The unique slug identifier of the custom contact field to update",
                "required": True,
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "New field display name",
                "desc": "The new display name for the custom contact field",
                "attr": {"preselected": True},
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Systeme.io delete contact field request data type
public class SystemeIoDeleteContactFieldRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "slug": {
                "type": StringType,
                "display_name": "Slug",
                "short_desc": "Field slug to delete",
                "desc": "The unique slug identifier of the custom contact field to delete",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SystemeIoDataProvider
