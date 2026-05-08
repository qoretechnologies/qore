# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Payment payload (sub-record)
public class EasyPostPaymentResultType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "Payment ID", "type": SoftStringOrNothingType, "desc": "Payment ID"},
            "amount": {"display_name": "Amount", "type": SoftStringOrNothingType, "desc": "Payment amount"},
            "currency": {"display_name": "Currency", "type": SoftStringOrNothingType, "desc": "ISO currency code"},
            "status": {"display_name": "Status", "type": SoftStringOrNothingType, "desc": "Payment status"},
            "object": {"display_name": "Object", "type": SoftStringOrNothingType, "desc": "Always `Payment`"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Payment event payload (`payment.completed` / `payment.failed`)
public class EasyPostPaymentEventType inherits HashDataType {
    public {
        const Fields = EasyPostBaseEventType::Fields - "result" + {
            "result": {"display_name": "Result", "type": new EasyPostPaymentResultType(), "desc": "Payment payload"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
