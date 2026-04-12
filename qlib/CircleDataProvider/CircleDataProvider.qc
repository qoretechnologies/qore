# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CircleDataProvider module definition

/*  CircleDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CircleDataProvider module
public namespace CircleDataProvider {

#! The root Circle data provider
public class CircleDataProvider inherits CircleDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "circle",
            "desc": "Circle data provider; provides access to Circle community platform "
                "resources including posts, comments, members, spaces, and messaging",
            "type": "CircleDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_observers": True,
            "constructor_options": BaseConstructorOptions,
        };

        #! Child provider map
        const ChildMap = {
            "posts": Class::forName("CirclePostsDataProvider"),
            "comments": Class::forName("CircleCommentsDataProvider"),
            "members": Class::forName("CircleMembersDataProvider"),
            "spaces": Class::forName("CircleSpacesDataProvider"),
            "communities": Class::forName("CircleCommunitiesDataProvider"),
            "create-post": Class::forName("CircleCreatePostDataProvider"),
            "update-post": Class::forName("CircleUpdatePostDataProvider"),
            "delete-post": Class::forName("CircleDeletePostDataProvider"),
            "create-comment": Class::forName("CircleCreateCommentDataProvider"),
            "delete-comment": Class::forName("CircleDeleteCommentDataProvider"),
            "invite-member": Class::forName("CircleInviteMemberDataProvider"),
            "remove-member": Class::forName("CircleRemoveMemberDataProvider"),
            "tag-member": Class::forName("CircleTagMemberDataProvider"),
            "untag-member": Class::forName("CircleUntagMemberDataProvider"),
            "add-member-to-space": Class::forName("CircleAddMemberToSpaceDataProvider"),
            "remove-member-from-space": Class::forName("CircleRemoveMemberFromSpaceDataProvider"),
            "add-member-to-space-group": Class::forName("CircleAddMemberToSpaceGroupDataProvider"),
            "remove-member-from-space-group":
                Class::forName("CircleRemoveMemberFromSpaceGroupDataProvider"),
            "create-space": Class::forName("CircleCreateSpaceDataProvider"),
            "update-space": Class::forName("CircleUpdateSpaceDataProvider"),
            "delete-space": Class::forName("CircleDeleteSpaceDataProvider"),
            "send-message": Class::forName("CircleSendMessageDataProvider"),
            "execute-api-call": Class::forName("CircleExecuteApiCallDataProvider"),
            "events": Class::forName("CircleEventsContainerDataProvider"),
        };
    }

    #! Creates the object with a REST client (async)
    constructor(RestClientIo::RestClientIo rest) : CircleDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(CircleRestClient::CircleRestConnection conn) : CircleDataProviderBase(conn) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : CircleDataProviderBase(options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    private *string getDescImpl() {
        return ProviderInfo.desc;
    }

    #! Returns data provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns a list of child data provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }
}

} # namespace CircleDataProvider
