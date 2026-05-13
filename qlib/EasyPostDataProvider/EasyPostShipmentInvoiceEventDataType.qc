# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Shipment invoice payload (sub-record)
public class EasyPostShipmentInvoiceResultType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "Invoice ID", "type": SoftStringOrNothingType, "desc": "Invoice ID"},
            "shipment_id": {"display_name": "Shipment ID", "type": SoftStringOrNothingType, "desc": "Associated shipment ID"},
            "amount": {"display_name": "Amount", "type": SoftStringOrNothingType, "desc": "Invoice amount as string"},
            "currency": {"display_name": "Currency", "type": SoftStringOrNothingType, "desc": "ISO currency code"},
            "status": {"display_name": "Status", "type": SoftStringOrNothingType, "desc": "Invoice status"},
            "object": {"display_name": "Object", "type": SoftStringOrNothingType, "desc": "Always `ShipmentInvoice`"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Shipment invoice event payload (`shipment.invoice.created`)
public class EasyPostShipmentInvoiceEventType inherits HashDataType {
    public {
        const Fields = EasyPostBaseEventType::Fields - "result" + {
            "result": {"display_name": "Result", "type": new EasyPostShipmentInvoiceResultType(), "desc": "Shipment invoice payload"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
