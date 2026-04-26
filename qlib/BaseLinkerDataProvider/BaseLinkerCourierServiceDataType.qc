# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerCourierServiceDataType.qc courier-service record
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! A single courier service (from getCourierServices)
/** BaseLinker returns services as a flat hash `{"<service_id>": "<service_name>"}`;
    the collection provider flattens each entry into this record shape.
*/
public class BaseLinkerCourierServiceDataType inherits HashDataType {
    public {
        const Fields = {
            "service_id": {
                "display_name": "Service ID",
                "short_desc": "Courier service identifier",
                "type": SoftStringType,
            },
            "service_name": {
                "display_name": "Service Name",
                "short_desc": "Courier service display name",
                "type": SoftStringOrNothingType,
            },
        };
    }
    constructor() : HashDataType("BaseLinkerCourierServiceDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
