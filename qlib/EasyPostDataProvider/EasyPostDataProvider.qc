# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

/*  EasyPostDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! The main EasyPost data provider class (root provider)
public class EasyPostDataProvider inherits EasyPostDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "easypost",
            "desc": "EasyPost data provider for shipping and logistics services",
            "type": "EasyPostDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map
        const ChildMap = {
            # Collection / list providers
            "addresses": Class::forName("EasyPostDataProvider::EasyPostAddressesDataProvider"),
            "batches": Class::forName("EasyPostDataProvider::EasyPostBatchesDataProvider"),
            "carrier-accounts": Class::forName("EasyPostDataProvider::EasyPostCarrierAccountsDataProvider"),
            "events-list": Class::forName("EasyPostDataProvider::EasyPostEventsListDataProvider"),
            "insurances": Class::forName("EasyPostDataProvider::EasyPostInsuranceListDataProvider"),
            "pickups": Class::forName("EasyPostDataProvider::EasyPostPickupsDataProvider"),
            "refunds": Class::forName("EasyPostDataProvider::EasyPostRefundsDataProvider"),
            "shipments": Class::forName("EasyPostDataProvider::EasyPostShipmentsDataProvider"),
            "trackers": Class::forName("EasyPostDataProvider::EasyPostTrackersDataProvider"),
            "webhooks": Class::forName("EasyPostDataProvider::EasyPostWebhooksDataProvider"),

            # Address actions
            "create-address": Class::forName("EasyPostDataProvider::EasyPostCreateAddressDataProvider"),
            "get-address": Class::forName("EasyPostDataProvider::EasyPostGetAddressDataProvider"),
            "verify-address": Class::forName("EasyPostDataProvider::EasyPostVerifyAddressDataProvider"),
            "create-and-verify-address": Class::forName("EasyPostDataProvider::EasyPostCreateAndVerifyAddressDataProvider"),

            # Parcel actions
            "create-parcel": Class::forName("EasyPostDataProvider::EasyPostCreateParcelDataProvider"),
            "get-parcel": Class::forName("EasyPostDataProvider::EasyPostGetParcelDataProvider"),

            # Shipment actions
            "create-shipment": Class::forName("EasyPostDataProvider::EasyPostCreateShipmentDataProvider"),
            "get-shipment": Class::forName("EasyPostDataProvider::EasyPostGetShipmentDataProvider"),
            "buy-shipment": Class::forName("EasyPostDataProvider::EasyPostBuyShipmentDataProvider"),
            "refund-shipment": Class::forName("EasyPostDataProvider::EasyPostRefundShipmentDataProvider"),
            "get-shipment-label": Class::forName("EasyPostDataProvider::EasyPostGetShipmentLabelDataProvider"),
            "get-shipment-smartrates": Class::forName("EasyPostDataProvider::EasyPostGetShipmentSmartratesDataProvider"),
            "rerate-shipment": Class::forName("EasyPostDataProvider::EasyPostRerateShipmentDataProvider"),
            "insure-shipment": Class::forName("EasyPostDataProvider::EasyPostInsureShipmentDataProvider"),

            # Tracker actions
            "create-tracker": Class::forName("EasyPostDataProvider::EasyPostCreateTrackerDataProvider"),
            "get-tracker": Class::forName("EasyPostDataProvider::EasyPostGetTrackerDataProvider"),
            "delete-tracker": Class::forName("EasyPostDataProvider::EasyPostDeleteTrackerDataProvider"),
            "bulk-create-trackers": Class::forName("EasyPostDataProvider::EasyPostBulkCreateTrackersDataProvider"),

            # Insurance actions
            "create-insurance": Class::forName("EasyPostDataProvider::EasyPostCreateInsuranceDataProvider"),
            "get-insurance": Class::forName("EasyPostDataProvider::EasyPostGetInsuranceDataProvider"),
            "refund-insurance": Class::forName("EasyPostDataProvider::EasyPostRefundInsuranceDataProvider"),

            # Pickup actions
            "create-pickup": Class::forName("EasyPostDataProvider::EasyPostCreatePickupDataProvider"),
            "get-pickup": Class::forName("EasyPostDataProvider::EasyPostGetPickupDataProvider"),
            "buy-pickup": Class::forName("EasyPostDataProvider::EasyPostBuyPickupDataProvider"),
            "cancel-pickup": Class::forName("EasyPostDataProvider::EasyPostCancelPickupDataProvider"),

            # Order actions
            "get-order": Class::forName("EasyPostDataProvider::EasyPostGetOrderDataProvider"),
            "create-order": Class::forName("EasyPostDataProvider::EasyPostCreateOrderDataProvider"),
            "buy-order": Class::forName("EasyPostDataProvider::EasyPostBuyOrderDataProvider"),

            # CustomsInfo / CustomsItem actions
            "get-customs-info": Class::forName("EasyPostDataProvider::EasyPostGetCustomsInfoDataProvider"),
            "create-customs-info": Class::forName("EasyPostDataProvider::EasyPostCreateCustomsInfoDataProvider"),
            "get-customs-item": Class::forName("EasyPostDataProvider::EasyPostGetCustomsItemDataProvider"),
            "create-customs-item": Class::forName("EasyPostDataProvider::EasyPostCreateCustomsItemDataProvider"),

            # Refund actions
            "create-refund": Class::forName("EasyPostDataProvider::EasyPostCreateRefundDataProvider"),
            "get-refund": Class::forName("EasyPostDataProvider::EasyPostGetRefundDataProvider"),

            # Batch actions
            "create-batch": Class::forName("EasyPostDataProvider::EasyPostCreateBatchDataProvider"),
            "get-batch": Class::forName("EasyPostDataProvider::EasyPostGetBatchDataProvider"),
            "batch-add-shipments": Class::forName("EasyPostDataProvider::EasyPostBatchAddShipmentsDataProvider"),
            "batch-remove-shipments": Class::forName("EasyPostDataProvider::EasyPostBatchRemoveShipmentsDataProvider"),
            "batch-buy": Class::forName("EasyPostDataProvider::EasyPostBatchBuyDataProvider"),
            "batch-label": Class::forName("EasyPostDataProvider::EasyPostBatchLabelDataProvider"),
            "batch-scan-form": Class::forName("EasyPostDataProvider::EasyPostBatchScanFormDataProvider"),

            # Webhook actions
            "get-webhook": Class::forName("EasyPostDataProvider::EasyPostGetWebhookDataProvider"),
            "create-webhook": Class::forName("EasyPostDataProvider::EasyPostCreateWebhookDataProvider"),
            "update-webhook": Class::forName("EasyPostDataProvider::EasyPostUpdateWebhookDataProvider"),
            "delete-webhook": Class::forName("EasyPostDataProvider::EasyPostDeleteWebhookDataProvider"),

            # Event actions
            "get-event": Class::forName("EasyPostDataProvider::EasyPostGetEventDataProvider"),
            "list-event-payloads": Class::forName("EasyPostDataProvider::EasyPostGetEventPayloadsDataProvider"),
            "get-event-payload": Class::forName("EasyPostDataProvider::EasyPostGetEventPayloadDataProvider"),

            # Carrier account actions
            "get-carrier-account": Class::forName("EasyPostDataProvider::EasyPostGetCarrierAccountDataProvider"),
            "create-carrier-account": Class::forName("EasyPostDataProvider::EasyPostCreateCarrierAccountDataProvider"),
            "update-carrier-account": Class::forName("EasyPostDataProvider::EasyPostUpdateCarrierAccountDataProvider"),
            "delete-carrier-account": Class::forName("EasyPostDataProvider::EasyPostDeleteCarrierAccountDataProvider"),

            # Events container (event/watch providers)
            "events": Class::forName("EasyPostDataProvider::EasyPostEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : EasyPostDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the provider description
    *string getDesc() {
        return ProviderInfo.desc;
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace EasyPostDataProvider
