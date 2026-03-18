# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedditDataProvider module definition

/*  RedditTrophyDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Reddit trophy record type
public class RedditTrophyDataType inherits HashDataType {
    public {
        #! Field definitions for Reddit trophies
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The trophy name",
                "desc": "The name of the trophy",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Trophy description",
                "desc": "The description of the trophy",
            },
            "id": {
                "type": StringOrNothingType,
                "display_name": "ID",
                "short_desc": "The trophy ID",
                "desc": "The unique identifier of the trophy",
            },
            "icon_70": {
                "type": StringOrNothingType,
                "display_name": "Icon (70px)",
                "short_desc": "Trophy icon URL (70px)",
                "desc": "URL of the trophy icon (70x70 pixels)",
            },
            "icon_40": {
                "type": StringOrNothingType,
                "display_name": "Icon (40px)",
                "short_desc": "Trophy icon URL (40px)",
                "desc": "URL of the trophy icon (40x40 pixels)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace RedditDataProvider
