# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsFormDataType.qc ClickFunnels form data type

/*  ClickFunnelsFormDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace ClickFunnelsDataProvider {

#! ClickFunnels form record type
public class ClickFunnelsFormDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The form ID",
                "desc": "The unique identifier of the form",
            },
            "public_id": {
                "type": StringOrNothingType,
                "display_name": "Public ID",
                "short_desc": "The public identifier",
                "desc": "The public-facing unique identifier of the form",
            },
            "workspace_id": {
                "type": IntOrNothingType,
                "display_name": "Workspace ID",
                "short_desc": "The workspace ID",
                "desc": "The ID of the workspace this form belongs to",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The form name",
                "desc": "The name of the form",
            },
            "redirect_url": {
                "type": StringOrNothingType,
                "display_name": "Redirect URL",
                "short_desc": "The redirect URL after submission",
                "desc": "The URL to redirect to after form submission",
            },
            "created_at": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the form was created",
                "desc": "The timestamp when the form was created",
            },
            "updated_at": {
                "type": DateOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the form was last updated",
                "desc": "The timestamp when the form was last updated",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ClickFunnelsDataProvider
