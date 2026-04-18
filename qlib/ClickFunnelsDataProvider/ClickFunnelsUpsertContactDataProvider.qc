# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsUpsertContactDataProvider.qc ClickFunnels upsert contact action

/*  ClickFunnelsUpsertContactDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace ClickFunnelsDataProvider {

#! ClickFunnels upsert contact action data provider
public class ClickFunnelsUpsertContactDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "upsert",
            "desc": "Upsert a contact in ClickFunnels (create or update by email)",
            "type": "ClickFunnelsUpsertContactDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType ResponseType = new ClickFunnelsContactDataType();
        static *AbstractDataProviderType RequestType = new ClickFunnelsCreateContactRequestDataType();
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest)
            : ClickFunnelsDataProviderBase(rest) {
    }

    string getName() {
        return "upsert";
    }

    *string getDesc() {
        return "Upsert a contact in ClickFunnels (create or update by email)";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ClickFunnelsCreateContactRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ClickFunnelsContactDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = setWorkspaceIdFromReq(req);
        return doPost(getWorkspacePath("contacts/upsert"), {"contact": req});
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ClickFunnelsDataProvider
