# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LemlistDataProvider module
public namespace LemlistDataProvider {

#! The main Lemlist data provider class (root provider)
public class LemlistDataProvider inherits LemlistDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "lemlist",
            "desc": "Lemlist data provider for sales outreach and email campaign management",
            "type": "LemlistDataProvider",
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
            "campaigns": Class::forName("LemlistCampaignsDataProvider"),
            "leads": Class::forName("LemlistLeadsDataProvider"),
            "activities": Class::forName("LemlistActivitiesDataProvider"),
            "tasks": Class::forName("LemlistTasksDataProvider"),
            "contacts": Class::forName("LemlistContactsDataProvider"),

            # Action providers - Campaigns
            "create-campaign": Class::forName("LemlistCreateCampaignDataProvider"),
            "pause-campaign": Class::forName("LemlistPauseCampaignDataProvider"),
            "start-campaign": Class::forName("LemlistStartCampaignDataProvider"),

            # Action providers - Leads
            "create-lead": Class::forName("LemlistCreateLeadDataProvider"),
            "update-lead": Class::forName("LemlistUpdateLeadDataProvider"),
            "delete-lead": Class::forName("LemlistDeleteLeadDataProvider"),

            # Action providers - Tasks
            "create-task": Class::forName("LemlistCreateTaskDataProvider"),
            "update-task": Class::forName("LemlistUpdateTaskDataProvider"),
            "delete-task": Class::forName("LemlistDeleteTaskDataProvider"),

            # Action providers - Contacts
            "create-contact": Class::forName("LemlistCreateContactDataProvider"),
            "update-contact": Class::forName("LemlistUpdateContactDataProvider"),
            "delete-contact": Class::forName("LemlistDeleteContactDataProvider"),

            # Event providers
            "events": Class::forName("LemlistEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : LemlistDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(LemlistRestClient::LemlistRestClient rest)
            : LemlistDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(LemlistRestClient::LemlistRestConnection conn) : LemlistDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "lemlist";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Lemlist data provider for sales outreach and email campaign management";
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

} # namespace LemlistDataProvider
