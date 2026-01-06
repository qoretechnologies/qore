# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearCommentDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LinearDataProvider module
public namespace LinearDataProvider {

#! Linear comment record type
public class LinearCommentDataType inherits HashDataType {
    public {
        #! Field definitions for Linear comments
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "The unique identifier of the comment",
                "desc": "The unique identifier of the comment",
                "type": StringType,
            },
            "body": {
                "display_name": "Body",
                "short_desc": "The comment body",
                "desc": "The body/content of the comment in markdown format",
                "type": StringType,
            },
            "url": {
                "display_name": "URL",
                "short_desc": "The comment URL",
                "desc": "The URL to the comment in Linear",
                "type": StringOrNothingType,
            },
            "user": {
                "display_name": "User",
                "short_desc": "The comment author",
                "desc": "The user who created the comment",
                "type": AutoHashOrNothingType,
            },
            "issue": {
                "display_name": "Issue",
                "short_desc": "The issue",
                "desc": "The issue the comment belongs to",
                "type": AutoHashOrNothingType,
            },
            "createdAt": {
                "display_name": "Created At",
                "short_desc": "When the comment was created",
                "desc": "The date and time when the comment was created",
                "type": DateOrNothingType,
            },
            "updatedAt": {
                "display_name": "Updated At",
                "short_desc": "When the comment was last updated",
                "desc": "The date and time when the comment was last updated",
                "type": DateOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor() {
        map addField(new QoreDataField({"name": $1.key} + $1.value)), Fields.pairIterator();
    }
}

} # namespace LinearDataProvider
