# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerJournalLogDataType.qc journal log record type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! BaseLinker getJournalList log entry
public class BaseLinkerJournalLogDataType inherits HashDataType {
    public {
        const Fields = {
            "log_id": {
                "display_name": "Log ID",
                "short_desc": "Monotonic journal entry identifier",
                "type": SoftIntType,
            },
            "log_type": {
                "display_name": "Log Type",
                "short_desc": "Numeric event type code",
                "type": SoftIntType,
                "desc": "Numeric event type (1-22); see BaseLinker `getJournalList` documentation.",
            },
            "order_id": {
                "display_name": "Order ID",
                "short_desc": "Affected order identifier",
                "type": SoftIntOrNothingType,
            },
            "object_id": {
                "display_name": "Object ID",
                "short_desc": "Identifier of the affected sub-object",
                "type": SoftStringOrNothingType,
                "desc": "For example, the product id for `order-product-added`, the invoice id "
                    "for `invoice-issued`, etc.",
            },
            "date": {
                "display_name": "Date",
                "short_desc": "Event timestamp (Unix seconds)",
                "type": SoftIntOrNothingType,
                "desc": "Event timestamp as Unix seconds since epoch.",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerJournalLogDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
