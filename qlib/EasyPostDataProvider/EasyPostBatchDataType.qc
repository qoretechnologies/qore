# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Batch status counts (per-state shipment counts inside a Batch)
public class EasyPostBatchStatusCountsType inherits HashDataType {
    public {
        const Fields = {
            "created": {"display_name": "Created", "type": SoftIntOrNothingType, "desc": "Number of shipments in the `created` state"},
            "queued_for_purchase": {"display_name": "Queued For Purchase", "type": SoftIntOrNothingType, "desc": "Number of shipments queued for purchase"},
            "creation_failed": {"display_name": "Creation Failed", "type": SoftIntOrNothingType, "desc": "Number of shipments that failed creation"},
            "postage_purchased": {"display_name": "Postage Purchased", "type": SoftIntOrNothingType, "desc": "Number of shipments with postage purchased"},
            "postage_purchase_failed": {"display_name": "Postage Purchase Failed", "type": SoftIntOrNothingType, "desc": "Number of shipments where postage purchase failed"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! Batch scan-form summary (sub-record of batch)
public class EasyPostScanFormType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Scan form ID (`sf_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `ScanForm`"},
            "status": {"display_name": "Status", "type": SoftStringOrNothingType, "desc": "Scan form status"},
            "message": {"display_name": "Message", "type": SoftStringOrNothingType, "desc": "Status message, if any"},
            "tracking_codes": {"display_name": "Tracking Codes", "type": new ListDataType("EasyPostScanFormTrackingCodeList", SoftStringType, True), "desc": "Tracking codes covered by the scan form"},
            "form_url": {"display_name": "Form URL", "type": SoftStringOrNothingType, "desc": "URL of the scan form image"},
            "form_file_type": {"display_name": "Form File Type", "type": SoftStringOrNothingType, "desc": "Scan form file type (e.g. `pdf`)"},
            "batch_id": {"display_name": "Batch ID", "type": SoftStringOrNothingType, "desc": "Associated batch ID"},
            "address": {"display_name": "Address", "type": new EasyPostAddressType(AutoHashOrNothingType), "desc": "Sender address used on the scan form"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! Batch shipment summary (sub-record of batch)
public class EasyPostBatchShipmentType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Shipment ID"},
            "batch_status": {"display_name": "Batch Status", "type": SoftStringOrNothingType, "desc": "Batch status of the shipment"},
            "batch_message": {"display_name": "Batch Message", "type": SoftStringOrNothingType, "desc": "Status message"},
            "tracking_code": {"display_name": "Tracking Code", "type": SoftStringOrNothingType, "desc": "Tracking code"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! EasyPost batch record
public class EasyPostBatchType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Batch ID (`batch_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `Batch`"},
            "mode": {"display_name": "Mode", "type": SoftStringOrNothingType, "desc": "`production` or `test`"},
            "state": {
                "display_name": "State",
                "type": SoftStringOrNothingType,
                "desc": "Batch state",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "creating", "desc": "Creating"},
                    <AllowedValueInfo>{"value": "creation_failed", "desc": "Creation failed"},
                    <AllowedValueInfo>{"value": "created", "desc": "Created"},
                    <AllowedValueInfo>{"value": "purchasing", "desc": "Purchasing"},
                    <AllowedValueInfo>{"value": "purchase_failed", "desc": "Purchase failed"},
                    <AllowedValueInfo>{"value": "purchased", "desc": "Purchased"},
                    <AllowedValueInfo>{"value": "label_generating", "desc": "Label generating"},
                    <AllowedValueInfo>{"value": "label_generated", "desc": "Label generated"},
                ),
            },
            "num_shipments": {"display_name": "Num Shipments", "type": SoftIntOrNothingType, "desc": "Total shipments in the batch"},
            "reference": {"display_name": "Reference", "type": SoftStringOrNothingType, "desc": "Caller-supplied reference"},
            "label_url": {"display_name": "Label URL", "type": SoftStringOrNothingType, "desc": "URL of the batched label PDF/ZPL"},
            "scan_form": {"display_name": "Scan Form", "type": new EasyPostScanFormType(AutoHashOrNothingType), "desc": "Scan form metadata"},
            "shipments": {"display_name": "Shipments", "type": new ListDataType("EasyPostBatchShipmentList", new EasyPostBatchShipmentType(), True), "desc": "Shipment summaries"},
            "status": {"display_name": "Status", "type": new EasyPostBatchStatusCountsType(AutoHashOrNothingType), "desc": "Hash with shipment counts by status (e.g. `postage_purchased`, `creation_failed`)"},
            "pickup": {"display_name": "Pickup", "type": new EasyPostPickupType(AutoHashOrNothingType), "desc": "Associated pickup, if any"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
