# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BitlyDataProvider module definition

/*  BitlyDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BitlyDataProvider module
public namespace BitlyDataProvider {

#! The main Bitly data provider class (root provider)
public class BitlyDataProvider inherits BitlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "bitly",
            "desc": "Bitly data provider for URL shortening, branded links, QR codes, "
                "campaigns, and click analytics",
            "type": "BitlyDataProvider",
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
            "bitlinks": Class::forName("BitlyBitlinksDataProvider"),
            "groups": Class::forName("BitlyGroupsDataProvider"),
            "organizations": Class::forName("BitlyOrganizationsDataProvider"),
            "campaigns": Class::forName("BitlyCampaignsDataProvider"),
            "bsds": Class::forName("BitlyBsdsDataProvider"),
            "qr-codes": Class::forName("BitlyQrCodesDataProvider"),
            "events": Class::forName("BitlyEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : BitlyDataProviderBase(options) {
    }

    #! Creates the object with a REST client (async)
    constructor(RestClientIo::RestClientIo rest) : BitlyDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(BitlyRestClient::BitlyRestConnection conn) : BitlyDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "bitly";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Bitly data provider for URL shortening, branded links, QR codes, "
            "campaigns, and click analytics";
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

} # namespace BitlyDataProvider
