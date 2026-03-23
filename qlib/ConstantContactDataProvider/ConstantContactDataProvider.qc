# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ConstantContactDataProvider module definition

/*  ConstantContactDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ConstantContactDataProvider module
public namespace ConstantContactDataProvider {

#! The main Constant Contact data provider class (root provider)
public class ConstantContactDataProvider inherits ConstantContactDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "constantcontact",
            "desc": "Constant Contact data provider for email marketing, contacts, lists, "
                "campaigns, tags, segments, and custom fields",
            "type": "ConstantContactDataProvider",
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
            "contacts": Class::forName("ConstantContactContactsDataProvider"),
            "contact-lists": Class::forName("ConstantContactContactListsDataProvider"),
            "tags": Class::forName("ConstantContactTagsDataProvider"),
            "custom-fields": Class::forName("ConstantContactCustomFieldsDataProvider"),
            "segments": Class::forName("ConstantContactSegmentsDataProvider"),
            "email-campaigns": Class::forName("ConstantContactEmailCampaignsDataProvider"),
            # Action providers
            "activities": Class::forName("ConstantContactActivitiesDataProvider"),
            "reports": Class::forName("ConstantContactReportsDataProvider"),
            "account": Class::forName("ConstantContactAccountDataProvider"),
            # Events container
            "events": Class::forName("ConstantContactEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : ConstantContactDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest)
            : ConstantContactDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ConstantContactRestClient::ConstantContactRestConnection conn)
            : ConstantContactDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "constantcontact";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Constant Contact data provider for email marketing, contacts, lists, "
            "campaigns, tags, segments, and custom fields";
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

} # namespace ConstantContactDataProvider
