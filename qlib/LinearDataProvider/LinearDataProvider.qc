# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearDataProvider.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LinearDataProvider module
public namespace LinearDataProvider {

#! The main Linear data provider class (root provider)
public class LinearDataProvider inherits LinearDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "linear",
            "desc": "Linear data provider for issue tracking and project management",
            "type": "LinearDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map
        const ChildMap = {
            "issues": Class::forName("LinearIssuesDataProvider"),
            "teams": Class::forName("LinearTeamsDataProvider"),
            "projects": Class::forName("LinearProjectsDataProvider"),
            "cycles": Class::forName("LinearCyclesDataProvider"),
            "labels": Class::forName("LinearLabelsDataProvider"),
            "events": Class::forName("LinearEventsDataProvider"),
            "create-issue": Class::forName("LinearCreateIssueDataProvider"),
            "create-comment": Class::forName("LinearCreateCommentDataProvider"),
            "create-project": Class::forName("LinearCreateProjectDataProvider"),
            "create-cycle": Class::forName("LinearCreateCycleDataProvider"),
            "create-label": Class::forName("LinearCreateLabelDataProvider"),
            "update-issue": Class::forName("LinearUpdateIssueDataProvider"),
            "archive-issue": Class::forName("LinearArchiveIssueDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : LinearDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(LinearRestClient::LinearRestClient rest) : LinearDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(LinearRestClient::LinearRestConnection conn) : LinearDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "linear";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Linear data provider for issue tracking and project management";
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

} # namespace LinearDataProvider
