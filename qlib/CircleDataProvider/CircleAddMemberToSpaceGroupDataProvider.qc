# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CircleAddMemberToSpaceGroupDataProvider.qc Circle add member to space group action

/*  CircleAddMemberToSpaceGroupDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Circle add member to space group action data provider
public class CircleAddMemberToSpaceGroupDataProvider inherits CircleDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "add-member-to-space-group",
            "desc": "Adds a member to a specific space group in Circle",
            "type": "CircleAddMemberToSpaceGroupDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static CircleAddMemberToSpaceGroupRequestDataType RequestType();
        static CircleMemberDataType ResponseType();
    }

    constructor(RestClientIo::RestClientIo rest) : CircleDataProviderBase(rest) {
    }

    string getName() {
        return "add-member-to-space-group";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("space_groups/" + string(req.space_group_id) + "/community_members",
            {"email": req.email});
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CircleDataProvider
