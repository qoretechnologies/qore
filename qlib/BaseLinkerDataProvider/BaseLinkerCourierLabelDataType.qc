# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerCourierLabelDataType.qc courier-label response record
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Courier label payload (from getLabel)
public class BaseLinkerCourierLabelDataType inherits HashDataType {
    public {
        const Fields = {
            "label": {
                "display_name": "Label Data",
                "short_desc": "Base64-encoded label content",
                "type": SoftStringOrNothingType,
                "desc": "Base64-encoded binary label content; decode to obtain the printable label.",
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
    constructor() : HashDataType("BaseLinkerCourierLabelDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
