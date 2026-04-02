# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GorgiasDataProvider module definition

/*  GorgiasTagDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GorgiasDataProvider module
public namespace GorgiasDataProvider {

#! Data type for tag decoration (visual styling)
public class GorgiasTagDecorationDataType inherits HashDataType {
    public {
        #! Field definitions for tag decoration
        const Fields = {
            "color": {
                "type": StringOrNothingType,
                "display_name": "Color",
                "short_desc": "Tag color",
                "desc": "The hex color code for the tag (e.g., `#F58D86`)",
                "example_value": "#F58D86",
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Gorgias tag record type
public class GorgiasTagDataType inherits HashDataType {
    public {
        #! Field definitions for Gorgias tags
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Tag ID",
                "short_desc": "The tag identifier",
                "desc": "The unique identifier of the tag",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Tag name",
                "desc": "The name of the tag (max 256 characters)",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Tag description",
                "desc": "A short description of the tag",
            },
            "uri": {
                "type": StringOrNothingType,
                "display_name": "URI",
                "short_desc": "The tag URI",
                "desc": "The URI path for the tag resource",
            },
            "decoration": {
                "type": new GorgiasTagDecorationDataType(AutoHashOrNothingType),
                "display_name": "Decoration",
                "short_desc": "Visual decoration",
                "desc": "Visual decoration settings for the tag, including color",
            },
            "usage": {
                "type": IntOrNothingType,
                "display_name": "Usage",
                "short_desc": "Number of tagged tickets",
                "desc": "The number of tickets that have this tag applied",
            },
            "created_datetime": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The ISO 8601 timestamp when the tag was created",
            },
            "updated_datetime": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "The ISO 8601 timestamp when the tag was last updated",
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

} # namespace GorgiasDataProvider
