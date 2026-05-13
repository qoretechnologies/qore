# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! EasyPost order record
public class EasyPostOrderType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Order ID (`order_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `Order`"},
            "mode": {"display_name": "Mode", "type": SoftStringOrNothingType, "desc": "`production` or `test`"},
            "reference": {"display_name": "Reference", "type": SoftStringOrNothingType, "desc": "Caller-supplied reference"},
            "to_address": {"display_name": "To Address", "type": new EasyPostAddressType(), "desc": "Destination address"},
            "from_address": {"display_name": "From Address", "type": new EasyPostAddressType(), "desc": "Origin address"},
            "return_address": {"display_name": "Return Address", "type": new EasyPostAddressType(), "desc": "Return address"},
            "buyer_address": {"display_name": "Buyer Address", "type": new EasyPostAddressType(), "desc": "Buyer address"},
            "shipments": {"display_name": "Shipments", "type": new ListDataType("EasyPostOrderShipmentList", new EasyPostShipmentType(), True), "desc": "Shipments contained in the order"},
            "rates": {"display_name": "Rates", "type": new ListDataType("EasyPostOrderRateList", new EasyPostRateType(), True), "desc": "Available rates"},
            "messages": {"display_name": "Messages", "type": new ListDataType("EasyPostOrderMessageList", new EasyPostMessageType(), True), "desc": "Carrier messages"},
            "is_return": {"display_name": "Is Return", "type": SoftBoolOrNothingType, "desc": "Whether the order is a return"},
            "customs_info": {"display_name": "Customs Info", "type": new EasyPostCustomsInfoType(), "desc": "Customs information"},
            "service": {"display_name": "Service", "type": SoftStringOrNothingType, "desc": "Carrier service used"},
            "carrier_accounts": {"display_name": "Carrier Accounts", "type": new ListDataType("EasyPostOrderCarrierAccountIdList", SoftStringType, True), "desc": "List of carrier account IDs"},
            "carrier": {"display_name": "Carrier", "type": SoftStringOrNothingType, "desc": "Carrier used"},
            "tax_identifiers": {"display_name": "Tax Identifiers", "type": new ListDataType("EasyPostOrderTaxIdList", new EasyPostTaxIdentifierType(), True), "desc": "Tax identifiers"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
