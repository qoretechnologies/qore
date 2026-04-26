# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerOrderSourceDataType.qc order-source record
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! An order source (from getOrderSources)
/** BaseLinker returns sources grouped by type (`shop`, `marketplace`, `personal`)
    keyed by source ID; the collection provider flattens each entry into this
    record shape.
*/
public class BaseLinkerOrderSourceDataType inherits HashDataType {
    public {
        const Fields = {
            "source_type": {
                "display_name": "Source Type",
                "short_desc": "Source category",
                "type": SoftStringType,
                "required": True,
                "desc": "Category of the source, e.g. `shop`, `marketplace`, `personal`.",
            },
            "source_id": {
                "display_name": "Source ID",
                "short_desc": "Source identifier within its type",
                "type": SoftStringType,
                "required": True,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Display name of the source",
                "type": SoftStringOrNothingType,
            },
        };
    }
    constructor() : HashDataType("BaseLinkerOrderSourceDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
