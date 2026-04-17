# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsDataProvider.qc root ClickFunnels data provider

/*  ClickFunnelsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ClickFunnelsDataProvider module
public namespace ClickFunnelsDataProvider {

#! The main ClickFunnels data provider class (root provider)
public class ClickFunnelsDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "clickfunnels",
            "desc": "ClickFunnels data provider for contacts, orders, products, funnels, "
                "courses, forms, fulfillments, and more",
            "type": "ClickFunnelsDataProvider",
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
            "contacts": Class::forName("ClickFunnelsContactsDataProvider"),
            "contact-tags": Class::forName("ClickFunnelsContactTagsDataProvider"),
            "applied-tags": Class::forName("ClickFunnelsAppliedTagsDataProvider"),
            "orders": Class::forName("ClickFunnelsOrdersDataProvider"),
            "products": Class::forName("ClickFunnelsProductsDataProvider"),
            "funnels": Class::forName("ClickFunnelsFunnelsDataProvider"),
            "courses": Class::forName("ClickFunnelsCoursesDataProvider"),
            "enrollments": Class::forName("ClickFunnelsEnrollmentsDataProvider"),
            "forms": Class::forName("ClickFunnelsFormsDataProvider"),
            "form-submissions": Class::forName("ClickFunnelsFormSubmissionsDataProvider"),
            "fulfillments": Class::forName("ClickFunnelsFulfillmentsDataProvider"),
            # Events container
            "events": Class::forName("ClickFunnelsEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : ClickFunnelsDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest)
            : ClickFunnelsDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ClickFunnelsRestClient::ClickFunnelsRestConnection conn)
            : ClickFunnelsDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "clickfunnels";
    }

    #! Returns the provider description
    *string getDesc() {
        return "ClickFunnels data provider for contacts, orders, products, funnels, "
            "courses, forms, fulfillments, and more";
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

} # namespace ClickFunnelsDataProvider
