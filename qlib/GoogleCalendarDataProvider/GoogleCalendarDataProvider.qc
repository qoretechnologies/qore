# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GoogleCalendarDataProvider module definition

/** GoogleCalendarDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GoogleCalendarDataProvider module
public namespace GoogleCalendarDataProvider {
#! The parent class for Google calendar REST APIs
public class GoogleCalendarDataProvider inherits GoogleDataProvider::GoogleApiParentDataProvider {
    private {
        static hash<string, Reflection::Class> childMap;
    }

    #! Creates the object from a REST connection
    constructor(GoogleRestClient::GoogleRestClientIo rest, string id)
            : GoogleDataProvider::GoogleApiParentDataProvider(rest, GoogleDiscoveryCalendarApiName, "calendars", {
                "calendarId": id,
            }, NOTHING, "events", "acl") {
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        *list<hash<DataProvider::DataProviderSummaryInfo>> rv =
            GoogleApiParentDataProvider::getChildProviderSummaryInfo();
        map rv += $1.getConstant("ProviderSummaryInfo").getValue(), childMap.iterator();
        return rv;
    }

    #! Register a new child data provider for Google calendars
    static registerChild(string pathname, Reflection::Class cls) {
        if (childMap{pathname} || pathname == "events") {
            throw "GOOGLE-CALENDAR-CHILD-ERROR", sprintf("child %y already registered", pathname);
        }
        childMap{pathname} = cls;
        ImplicitModuleTransaction::add(sub () { GoogleCalendarDataProvider::deregisterChild(pathname); });
    }

    #! Deregister the named child data provider
    private static deregisterChild(string pathname) {
        remove childMap{pathname};
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        *list<string> rv = GoogleApiParentDataProvider::getChildProviderNamesImpl();
        map rv += $1, keys childMap;
        return rv;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        if (*GoogleDataProviderBase rv = GoogleApiParentDataProvider::getChildProviderImpl(name)) {
            return rv;
        }
        if (*Class cls = childMap{name}) {
            return cls.newObject(rest, path_args);
        }
    }
}
}
