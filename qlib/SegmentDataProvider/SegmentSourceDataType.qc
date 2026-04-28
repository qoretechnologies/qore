# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SegmentDataProvider module definition

/*  SegmentSourceDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SegmentDataProvider module
public namespace SegmentDataProvider {

#! Segment source record type
public class SegmentSourceDataType inherits HashDataType {
    public {
        #! Field definitions for Segment sources
        const Fields = {
            "id": {
                "type": StringOrNothingType,
                "display_name": "ID",
                "short_desc": "Source ID",
                "desc": "The unique identifier for the source",
            },
            "slug": {
                "type": StringOrNothingType,
                "display_name": "Slug",
                "short_desc": "Source slug",
                "desc": "The slug (URL-friendly name) of the source",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Source name",
                "desc": "The human-readable name of the source",
            },
            "metadata": {
                "type": new SegmentMetadataDataType(AutoHashOrNothingType),
                "display_name": "Metadata",
                "short_desc": "Source metadata",
                "desc": "Metadata about the source including its catalog ID, name, slug, and categories",
            },
            "workspaceId": {
                "type": StringOrNothingType,
                "display_name": "Workspace ID",
                "short_desc": "Workspace ID",
                "desc": "The ID of the workspace that owns this source",
            },
            "enabled": {
                "type": BoolOrNothingType,
                "display_name": "Enabled",
                "short_desc": "Whether the source is enabled",
                "desc": "Whether the source is currently enabled (`True`) or disabled (`False`)",
            },
            "writeKeys": {
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "display_name": "Write Keys",
                "short_desc": "Source write keys",
                "desc": "The write keys associated with this source for sending data",
            },
            "settings": {
                "type": AutoHashOrNothingType,
                "display_name": "Settings",
                "short_desc": "Source settings",
                "desc": "Configuration settings for the source",
            },
            "labels": {
                "type": new ListDataType("LabelItems", new SegmentLabelItemDataType(AutoHashOrNothingType),
                    True),
                "display_name": "Labels",
                "short_desc": "Labels",
                "desc": "Labels associated with the source as key-value pairs for organization",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SegmentDataProvider
