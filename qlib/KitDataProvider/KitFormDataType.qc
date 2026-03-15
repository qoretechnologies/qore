# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KitDataProvider module definition

/*  KitFormDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KitDataProvider module
public namespace KitDataProvider {

#! Kit form record type
public class KitFormDataType inherits HashDataType {
    public {
        #! Field definitions for Kit forms
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "ID",
                "short_desc": "The form ID",
                "desc": "The unique identifier of the form",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The form name",
                "desc": "The name of the form",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The form type",
                "desc": "The type of the form",
            },
            "format": {
                "type": StringOrNothingType,
                "display_name": "Format",
                "short_desc": "The form format",
                "desc": "The format of the form",
            },
            "embed_js": {
                "type": StringOrNothingType,
                "display_name": "Embed JS",
                "short_desc": "The JavaScript embed code",
                "desc": "The JavaScript code for embedding the form",
            },
            "embed_url": {
                "type": StringOrNothingType,
                "display_name": "Embed URL",
                "short_desc": "The embed URL",
                "desc": "The URL for embedding the form",
            },
            "archived": {
                "type": SoftBoolOrNothingType,
                "display_name": "Archived",
                "short_desc": "Whether the form is archived",
                "desc": "Indicates whether the form has been archived",
            },
            "created_at": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the form was created",
                "desc": "The timestamp when the form was created",
            },
            "uid": {
                "type": StringOrNothingType,
                "display_name": "UID",
                "short_desc": "The form UID",
                "desc": "The unique identifier string for the form",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KitDataProvider
