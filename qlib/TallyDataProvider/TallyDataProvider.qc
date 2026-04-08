# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the TallyDataProvider module
public namespace TallyDataProvider {
#! The Tally data provider class
public class TallyDataProvider inherits TallyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "tally",
            "type": "TallyDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_observers": True,
        };

        #! Application name for action catalog
        const AppName = "Tally";
    }

    private {
        #! Dynamic child data providers
        static hash<string, Reflection::Class> childMap = DefaultChildMap;

        #! Child data providers
        const DefaultChildMap = {
            # Collection providers (DPAT_FIND)
            "forms": Class::forName("TallyDataProvider::TallyFormsDataProvider"),
            "submissions": Class::forName("TallyDataProvider::TallySubmissionsDataProvider"),
            "questions": Class::forName("TallyDataProvider::TallyQuestionsDataProvider"),
            "webhooks": Class::forName("TallyDataProvider::TallyWebhooksDataProvider"),
            # Action providers (DPAT_API)
            "create-form": Class::forName("TallyDataProvider::TallyCreateFormDataProvider"),
            "get-form": Class::forName("TallyDataProvider::TallyGetFormDataProvider"),
            "update-form": Class::forName("TallyDataProvider::TallyUpdateFormDataProvider"),
            "delete-form": Class::forName("TallyDataProvider::TallyDeleteFormDataProvider"),
            "get-submission": Class::forName("TallyDataProvider::TallyGetSubmissionDataProvider"),
            "delete-submission": Class::forName("TallyDataProvider::TallyDeleteSubmissionDataProvider"),
            "create-webhook": Class::forName("TallyDataProvider::TallyCreateWebhookDataProvider"),
            "update-webhook": Class::forName("TallyDataProvider::TallyUpdateWebhookDataProvider"),
            "delete-webhook": Class::forName("TallyDataProvider::TallyDeleteWebhookDataProvider"),
            "get-current-user": Class::forName("TallyDataProvider::TallyGetCurrentUserDataProvider"),
            # Events
            "events": Class::forName("TallyDataProvider::TallyEventsDataProvider"),
        };
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest) : TallyDataProviderBase(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : TallyDataProviderBase(options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Register a new child data provider
    static registerChild(string pathname, Reflection::Class cls) {
        if (childMap{pathname}) {
            throw "TALLY-CHILD-ERROR", sprintf("child %y already registered", pathname);
        }
        childMap{pathname} = cls;
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), childMap.iterator();
    }

    #! Returns a list of child data provider names, if any
    private *list<string> getChildProviderNamesImpl() {
        return keys childMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = childMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
