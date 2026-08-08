# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinkedInOrganizationsDataProvider module definition

/** LinkedInOrganizationsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the LinkedInOrganizationsDataProvider module
public namespace LinkedInOrganizationsDataProvider {
#! The LinkedIn Organizations data provider class
public class LinkedInOrganizationsDataProvider inherits LinkedInOrganizationsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "linkedinorg",
            "type": "LinkedInOrganizationsDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
            "children_can_support_observers": True,
        };
    }

    private {
        #! Dynamic child data providers
        static hash<string, Reflection::Class> childMap = DefaultChildMap;

        #! Child data providers
        const DefaultChildMap = {
            # Identity
            "get-current-member": Class::forName(
                "LinkedInOrganizationsDataProvider::LinkedInGetCurrentMemberDataProvider"),
            # Organizations
            "list-organizations": Class::forName(
                "LinkedInOrganizationsDataProvider::LinkedInListOrganizationsDataProvider"),
            "get-organization": Class::forName(
                "LinkedInOrganizationsDataProvider::LinkedInGetOrganizationDataProvider"),
            # Content
            "list-posts": Class::forName(
                "LinkedInOrganizationsDataProvider::LinkedInListPostsDataProvider"),
            "get-post": Class::forName(
                "LinkedInOrganizationsDataProvider::LinkedInGetOrgPostDataProvider"),
            # Engagement
            "list-comments": Class::forName(
                "LinkedInOrganizationsDataProvider::LinkedInListCommentsDataProvider"),
            "list-reactions": Class::forName(
                "LinkedInOrganizationsDataProvider::LinkedInListReactionsDataProvider"),
            "get-social-metadata": Class::forName(
                "LinkedInOrganizationsDataProvider::LinkedInGetSocialMetadataDataProvider"),
            # Audience
            "list-followers": Class::forName(
                "LinkedInOrganizationsDataProvider::LinkedInListFollowersDataProvider"),
            "list-following": Class::forName(
                "LinkedInOrganizationsDataProvider::LinkedInListFollowingDataProvider"),
            # Analytics
            "get-follower-statistics": Class::forName(
                "LinkedInOrganizationsDataProvider::LinkedInGetFollowerStatisticsDataProvider"),
            "get-visitor-statistics": Class::forName(
                "LinkedInOrganizationsDataProvider::LinkedInGetVisitorStatisticsDataProvider"),
            # Events
            "events": Class::forName(
                "LinkedInOrganizationsDataProvider::LinkedInOrganizationsEventsDataProvider"),
        };

        #! Children that take an options hash rather than only a client
        /** The watch providers are configured with the organization whose feed they follow, so the
            container is constructed with the options this provider was given rather than with the
            client alone.
        */
        const OptionChildren = {
            "events": True,
        };
    }

    private {
        #! The options this provider was created with, passed on to option-taking children
        *hash<auto> child_options;
    }

    #! Creates the object from a REST client
    constructor(LinkedInRestClient::LinkedInRestClientIo rest)
            : LinkedInOrganizationsDataProviderBase(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : LinkedInOrganizationsDataProviderBase(options) {
        child_options = options;
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Register a new child data provider
    static registerChild(string pathname, Reflection::Class cls) {
        if (childMap{pathname}) {
            throw "LINKEDIN-CHILD-ERROR", sprintf("child %y already registered", pathname);
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
        if (OptionChildren{name}) {
            return cls.newObject(rest, child_options);
        }
        return cls.newObject(rest);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
