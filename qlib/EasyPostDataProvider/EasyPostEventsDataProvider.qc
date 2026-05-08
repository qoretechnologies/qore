# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Container for EasyPost webhook event providers
public class EasyPostEventsDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "desc": "EasyPost webhook event providers - watch for tracker, batch, insurance, refund, payment events",
            "type": "EasyPostEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };

        const ChildMap = {
            "watch-tracker-created": Class::forName("EasyPostDataProvider::EasyPostWatchTrackerCreatedEventDataProvider"),
            "watch-tracker-updated": Class::forName("EasyPostDataProvider::EasyPostWatchTrackerUpdatedEventDataProvider"),
            "watch-batch-created": Class::forName("EasyPostDataProvider::EasyPostWatchBatchCreatedEventDataProvider"),
            "watch-batch-updated": Class::forName("EasyPostDataProvider::EasyPostWatchBatchUpdatedEventDataProvider"),
            "watch-insurance-purchased": Class::forName("EasyPostDataProvider::EasyPostWatchInsurancePurchasedEventDataProvider"),
            "watch-refund-successful": Class::forName("EasyPostDataProvider::EasyPostWatchRefundSuccessfulEventDataProvider"),
            "watch-shipment-invoice-created": Class::forName("EasyPostDataProvider::EasyPostWatchShipmentInvoiceCreatedEventDataProvider"),
            "watch-payment-completed": Class::forName("EasyPostDataProvider::EasyPostWatchPaymentCompletedEventDataProvider"),
            "watch-payment-failed": Class::forName("EasyPostDataProvider::EasyPostWatchPaymentFailedEventDataProvider"),
        };
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "events";
    }

    *string getDesc() {
        return "EasyPost webhook event providers";
    }

    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace EasyPostDataProvider
