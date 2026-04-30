# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SegmentDataProvider module definition

/*  SegmentDestinationDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Segment destination record type
public class SegmentDestinationDataType inherits HashDataType {
    public {
        #! Field definitions for Segment destinations
        const Fields = {
            "id": {
                "type": StringOrNothingType,
                "display_name": "ID",
                "short_desc": "Destination ID",
                "desc": "The unique identifier for the destination",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Destination name",
                "desc": "The human-readable name of the destination",
            },
            "enabled": {
                "type": BoolOrNothingType,
                "display_name": "Enabled",
                "short_desc": "Whether the destination is enabled",
                "desc": "Whether the destination is currently enabled (`True`) or disabled (`False`)",
            },
            "metadata": {
                "type": new SegmentMetadataDataType(AutoHashOrNothingType),
                "display_name": "Metadata",
                "short_desc": "Destination metadata",
                "desc": "Metadata about the destination including its catalog ID, name, and categories",
            },
            "sourceId": {
                "type": StringOrNothingType,
                "display_name": "Source ID",
                "short_desc": "Connected source ID",
                "desc": "The ID of the source connected to this destination",
            },
            "settings": {
                "type": AutoHashOrNothingType,
                "display_name": "Settings",
                "short_desc": "Destination settings",
                "desc": "Configuration settings for the destination",
            },
            "labels": {
                "type": new ListDataType("LabelItems", new SegmentLabelItemDataType(AutoHashOrNothingType),
                    True),
                "display_name": "Labels",
                "short_desc": "Labels",
                "desc": "Labels associated with the destination as key-value pairs for organization",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SegmentDataProvider
