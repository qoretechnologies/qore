# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudDataProvider.qc root SendCloud data provider

/*  SendCloudDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SendCloudDataProvider module
public namespace SendCloudDataProvider {

#! The main SendCloud data provider class (root provider)
public class SendCloudDataProvider inherits SendCloudDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "sendcloud",
            "desc": "SendCloud data provider for shipping, labels, returns, and service points",
            "type": "SendCloudDataProvider",
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
            # Collection/List providers
            "parcels": Class::forName("SendCloudDataProvider::SendCloudParcelsDataProvider"),
            "shipping-methods": Class::forName("SendCloudDataProvider::SendCloudShippingMethodsDataProvider"),
            "sender-addresses": Class::forName("SendCloudDataProvider::SendCloudSenderAddressesDataProvider"),
            "service-points": Class::forName("SendCloudDataProvider::SendCloudServicePointsDataProvider"),
            "returns": Class::forName("SendCloudDataProvider::SendCloudReturnsDataProvider"),
            "brands": Class::forName("SendCloudDataProvider::SendCloudBrandsDataProvider"),
            "users": Class::forName("SendCloudDataProvider::SendCloudUsersDataProvider"),

            # Action providers - Parcels
            "get-parcel": Class::forName("SendCloudDataProvider::SendCloudGetParcelDataProvider"),
            "create-parcel": Class::forName("SendCloudDataProvider::SendCloudCreateParcelDataProvider"),
            "update-parcel": Class::forName("SendCloudDataProvider::SendCloudUpdateParcelDataProvider"),
            "cancel-parcel": Class::forName("SendCloudDataProvider::SendCloudCancelParcelDataProvider"),
            "get-parcel-label": Class::forName("SendCloudDataProvider::SendCloudGetParcelLabelDataProvider"),
            "get-parcel-documents": Class::forName(
                "SendCloudDataProvider::SendCloudGetParcelDocumentsDataProvider"),

            # Action providers - User
            "get-user": Class::forName("SendCloudDataProvider::SendCloudGetUserDataProvider"),

            # Action providers - Service Points
            "search-service-points":
                Class::forName("SendCloudDataProvider::SendCloudSearchServicePointsDataProvider"),

            # Action providers - Returns
            "get-return": Class::forName("SendCloudDataProvider::SendCloudGetReturnDataProvider"),
            "create-return": Class::forName("SendCloudDataProvider::SendCloudCreateReturnDataProvider"),
            "update-return": Class::forName("SendCloudDataProvider::SendCloudUpdateReturnDataProvider"),
            "cancel-return": Class::forName("SendCloudDataProvider::SendCloudCancelReturnDataProvider"),

            # Event providers (container)
            "events": Class::forName("SendCloudDataProvider::SendCloudEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : SendCloudDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(*RestClientIo::RestClientIo rest) : SendCloudDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(SendCloudRestClient::SendCloudRestConnection conn) : SendCloudDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "sendcloud";
    }

    #! Returns the provider description
    *string getDesc() {
        return "SendCloud data provider for shipping, labels, returns, and service points";
    }

    #! Returns child provider names (sorted for stable listing)
    private *list<string> getChildProviderNamesImpl() {
        return sort(keys ChildMap);
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            object child = cls.newObject(rest);
            # Propagate SendCloud credentials so children that talk to cross-host endpoints
            # (e.g. servicepoints.sendcloud.sc) can build their own authenticated client.
            if (sc_creds && child instanceof SendCloudDataProviderBase) {
                cast<SendCloudDataProviderBase>(child).setSendCloudCredentials(sc_creds);
            }
            return cast<AbstractDataProvider>(child);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SendCloudDataProvider
