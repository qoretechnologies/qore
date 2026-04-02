# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SegmentDataProvider module definition

/*  SegmentMetadataDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Segment metadata type for sources, destinations, and warehouses
public class SegmentMetadataDataType inherits HashDataType {
    public {
        #! Field definitions for Segment metadata
        const Fields = {
            "id": {
                "type": StringOrNothingType,
                "display_name": "Catalog ID",
                "short_desc": "Catalog ID",
                "desc": "The catalog ID of the integration (e.g., `catalog/sources/javascript`)",
            },
            "slug": {
                "type": StringOrNothingType,
                "display_name": "Slug",
                "short_desc": "Integration slug",
                "desc": "The URL-friendly slug of the integration type",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Integration name",
                "desc": "The human-readable name of the integration type",
            },
            "categories": {
                "type": AutoListOrNothingType,
                "display_name": "Categories",
                "short_desc": "Integration categories",
                "desc": "The categories this integration belongs to (e.g., `Analytics`, `Advertising`)",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Integration description",
                "desc": "A description of the integration type",
            },
            "logos": {
                "type": AutoHashOrNothingType,
                "display_name": "Logos",
                "short_desc": "Integration logos",
                "desc": "Logo URLs for the integration (contains `default`, `mark`, and `alt` keys)",
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Segment label item type for label key-value pairs on resources
public class SegmentLabelItemDataType inherits HashDataType {
    public {
        #! Field definitions for Segment label items
        const Fields = {
            "key": {
                "type": StringOrNothingType,
                "display_name": "Key",
                "short_desc": "Label key",
                "desc": "The key of the label",
            },
            "value": {
                "type": StringOrNothingType,
                "display_name": "Value",
                "short_desc": "Label value",
                "desc": "The value of the label",
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

} # namespace SegmentDataProvider
