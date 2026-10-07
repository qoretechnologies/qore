# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerCourierLabelDataType.qc courier-label response record
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Courier label payload (from getLabel)
/** BaseLinker returns <tt>label</tt> as a base64-encoded file whose format is given by
    <tt>extension</tt>. The raw API response is transformed before being exposed so
    that <tt>label</tt> is a @ref DataProvider::FileDataType with <tt>name</tt>, <tt>content</tt>
    (base64), and <tt>mime_type</tt> already populated.
*/
public class BaseLinkerCourierLabelDataType inherits HashDataType {
    public {
        const Fields = {
            "label": {
                "type": DataProvider::FileDataType.getOrNothingType(),
                "display_name": "Label File",
                "short_desc": "Decoded label file (base64 content with MIME type)",
                "desc": "Label file with `content` (base64-encoded), `name`, and `mime_type` "
                    "derived from the BaseLinker `extension` field.",
            },
            "extension": {
                "display_name": "Extension",
                "short_desc": "Label file extension",
                "type": SoftStringOrNothingType,
                "desc": "File format of the label, e.g. `pdf`, `zpl`, `png`.",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "BaseLinker response status",
                "type": SoftStringOrNothingType,
            },
        };
    }
    constructor() : HashDataType("BaseLinkerCourierLabelDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
