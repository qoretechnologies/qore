# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Katana batch transaction sub-type (used in manufacturing orders)
public class KatanaBatchTransactionDataType inherits HashDataType {
    public {
        const Fields = {
            "batch_id": {
                "type": SoftIntType,
                "display_name": "Batch ID",
                "required": True,
                "short_desc": "Batch ID",
                "desc": "The ID of the batch involved in the transaction",
                "ref_data": "batch_stocks",
            },
            "quantity": {
                "type": SoftFloatType,
                "display_name": "Quantity",
                "required": True,
                "short_desc": "Quantity",
                "desc": "Quantity moved in/out of the batch",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
