# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CustomerIoDataProvider module
public namespace CustomerIoDataProvider {

#! The main Customer.io data provider class (root provider)
public class CustomerIoDataProvider inherits CustomerIoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "customerio",
            "desc": "Customer.io data provider for customer messaging and engagement",
            "type": "CustomerIoDataProvider",
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
            # App API Collection providers (DPAT_FIND)
            "people": Class::forName("CustomerIoPeopleDataProvider"),
            "segments": Class::forName("CustomerIoSegmentsDataProvider"),
            "campaigns": Class::forName("CustomerIoCampaignsDataProvider"),
            "broadcasts": Class::forName("CustomerIoBroadcastsDataProvider"),
            "collections": Class::forName("CustomerIoCollectionsDataProvider"),
            "snippets": Class::forName("CustomerIoSnippetsDataProvider"),
            "messages": Class::forName("CustomerIoMessagesDataProvider"),
            "activities": Class::forName("CustomerIoActivitiesDataProvider"),
            "newsletters": Class::forName("CustomerIoNewslettersDataProvider"),

            # App API Action providers
            "get-person": Class::forName("CustomerIoGetPersonDataProvider"),
            "get-segment": Class::forName("CustomerIoGetSegmentDataProvider"),
            "get-segment-people": Class::forName("CustomerIoGetSegmentPeopleDataProvider"),
            "get-campaign": Class::forName("CustomerIoGetCampaignDataProvider"),
            "get-campaign-metrics": Class::forName("CustomerIoGetCampaignMetricsDataProvider"),
            "get-campaign-actions": Class::forName("CustomerIoGetCampaignActionsDataProvider"),
            "get-broadcast": Class::forName("CustomerIoGetBroadcastDataProvider"),
            "trigger-broadcast": Class::forName("CustomerIoTriggerBroadcastDataProvider"),
            "send-transactional-email": Class::forName("CustomerIoSendTransactionalEmailDataProvider"),
            "send-transactional-push": Class::forName("CustomerIoSendTransactionalPushDataProvider"),
            "get-collection": Class::forName("CustomerIoGetCollectionDataProvider"),
            "create-collection": Class::forName("CustomerIoCreateCollectionDataProvider"),
            "update-collection": Class::forName("CustomerIoUpdateCollectionDataProvider"),
            "delete-collection": Class::forName("CustomerIoDeleteCollectionDataProvider"),
            "get-snippet": Class::forName("CustomerIoGetSnippetDataProvider"),
            "create-snippet": Class::forName("CustomerIoCreateSnippetDataProvider"),
            "update-snippet": Class::forName("CustomerIoUpdateSnippetDataProvider"),
            "delete-snippet": Class::forName("CustomerIoDeleteSnippetDataProvider"),
            "create-export": Class::forName("CustomerIoCreateExportDataProvider"),
            "get-export-status": Class::forName("CustomerIoGetExportStatusDataProvider"),
            "download-export": Class::forName("CustomerIoDownloadExportDataProvider"),
            "get-message": Class::forName("CustomerIoGetMessageDataProvider"),
            "get-newsletter": Class::forName("CustomerIoGetNewsletterDataProvider"),

            # Track API Action providers
            "create-update-customer": Class::forName("CustomerIoCreateUpdateCustomerDataProvider"),
            "delete-customer": Class::forName("CustomerIoDeleteCustomerDataProvider"),
            "suppress-customer": Class::forName("CustomerIoSuppressCustomerDataProvider"),
            "unsuppress-customer": Class::forName("CustomerIoUnsuppressCustomerDataProvider"),
            "merge-customers": Class::forName("CustomerIoMergeCustomersDataProvider"),
            "track-event": Class::forName("CustomerIoTrackEventDataProvider"),
            "track-anonymous-event": Class::forName("CustomerIoTrackAnonymousEventDataProvider"),
            "track-page-view": Class::forName("CustomerIoTrackPageViewDataProvider"),
            "add-device": Class::forName("CustomerIoAddDeviceDataProvider"),
            "delete-device": Class::forName("CustomerIoDeleteDeviceDataProvider"),
            "create-update-object": Class::forName("CustomerIoCreateUpdateObjectDataProvider"),
            "delete-object": Class::forName("CustomerIoDeleteObjectDataProvider"),
            "add-relationships": Class::forName("CustomerIoAddRelationshipsDataProvider"),
            "delete-relationships": Class::forName("CustomerIoDeleteRelationshipsDataProvider"),

            # Event providers
            "events": Class::forName("CustomerIoEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : CustomerIoDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(CustomerIoRestClient::CustomerIoRestClient rest)
            : CustomerIoDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(CustomerIoRestClient::CustomerIoRestConnection conn) : CustomerIoDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "customerio";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Customer.io data provider for customer messaging and engagement";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            CustomerIoDataProviderBase child = cls.newObject(rest);
            # Propagate Track API credentials to child providers
            child.setTrackApiCredentials(site_id, tracking_api_key, region);
            return child;
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CustomerIoDataProvider
