# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedditDataProvider module definition

/*  RedditFlairDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the RedditDataProvider module
public namespace RedditDataProvider {

#! Reddit flair record type
public class RedditFlairDataType inherits HashDataType {
    public {
        #! Field definitions for Reddit flairs
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The flair template ID",
                "desc": "The unique identifier of the flair template",
            },
            "text": {
                "type": StringOrNothingType,
                "display_name": "Text",
                "short_desc": "The flair text",
                "desc": "The display text of the flair",
            },
            "text_color": {
                "type": StringOrNothingType,
                "display_name": "Text Color",
                "short_desc": "Flair text color",
                "desc": "The text color of the flair (dark or light)",
            },
            "background_color": {
                "type": StringOrNothingType,
                "display_name": "Background Color",
                "short_desc": "Flair background color",
                "desc": "The background color of the flair (hex code)",
            },
            "css_class": {
                "type": StringOrNothingType,
                "display_name": "CSS Class",
                "short_desc": "Flair CSS class",
                "desc": "The CSS class of the flair for custom styling",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "Flair type",
                "desc": "The flair type (text, richtext)",
            },
            "text_editable": {
                "type": SoftBoolOrNothingType,
                "display_name": "Editable",
                "short_desc": "Whether the text is editable",
                "desc": "Indicates if users can edit the flair text",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace RedditDataProvider
