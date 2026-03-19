# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KitDataProvider module definition

/*  KitDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KitDataProvider module
public namespace KitDataProvider {

#! The main Kit data provider class (root provider)
public class KitDataProvider inherits KitDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "kit",
            "desc": "Kit data provider for subscribers, tags, custom fields, broadcasts, "
                "forms, sequences, and purchases",
            "type": "KitDataProvider",
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
            # Collection providers
            "subscribers": Class::forName("KitSubscribersDataProvider"),
            "tags": Class::forName("KitTagsDataProvider"),
            "custom-fields": Class::forName("KitCustomFieldsDataProvider"),
            "broadcasts": Class::forName("KitBroadcastsDataProvider"),
            "forms": Class::forName("KitFormsDataProvider"),
            "sequences": Class::forName("KitSequencesDataProvider"),
            "purchases": Class::forName("KitPurchasesDataProvider"),
            # Events container
            "events": Class::forName("KitEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : KitDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(KitRestClient::KitRestClient rest) : KitDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(KitRestClient::KitRestConnection conn) : KitDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "kit";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Kit data provider for subscribers, tags, custom fields, broadcasts, "
            "forms, sequences, and purchases";
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

} # namespace KitDataProvider
