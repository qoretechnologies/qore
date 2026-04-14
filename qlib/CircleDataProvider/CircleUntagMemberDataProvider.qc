# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CircleUntagMemberDataProvider.qc Circle untag member action data provider

/*  CircleUntagMemberDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Circle untag member response data type
public class CircleUntagMemberResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "success": {
                "type": SoftBoolType,
                "display_name": "Success",
                "short_desc": "Whether the untag was successful",
                "desc": "Whether the tag was successfully removed from the member",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Circle untag member action data provider
public class CircleUntagMemberDataProvider inherits CircleDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "untag-member",
            "desc": "Removes a tag from a specific community member in Circle",
            "type": "CircleUntagMemberDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static CircleUntagMemberRequestDataType RequestType();
        static CircleUntagMemberResponseDataType ResponseType();
    }

    constructor(RestClientIo::RestClientIo rest) : CircleDataProviderBase(rest) {
    }

    string getName() {
        return "untag-member";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete("community_members/" + string(req.id) + "/tags/" + string(req.tag_id));
        return {"success": True};
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CircleDataProvider
