# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DropboxAccountDataProviders.qc account-level action providers for the DropboxDataProvider module

/*  DropboxAccountDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DropboxDataProvider module
public namespace DropboxDataProvider {
#! Returns information about the account the connection authenticates as
/** The connection requests the \c account_info.read scope, and until this action existed nothing in the module
    used it -- so the consent screen asked for access to the account profile that the integration then had no way
    to read.
*/
public class DropboxGetAccountInfoDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-account-info",
            "type": "DropboxGetAccountInfoDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxEmptyRequestDataType;
        const ResponseType = DropboxAccountInfoDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # NOTE: this endpoint takes no arguments at all, and Dropbox rejects a request that sends any; the body
        # is therefore omitted rather than sent as an empty hash
        hash<auto> body = doRpc("/2/users/get_current_account");
        return {
            "account_id": body.account_id,
            "display_name": body.name.display_name,
            "given_name": body.name.given_name,
            "surname": body.name.surname,
            "email": body.email,
            "email_verified": body.email_verified,
            "disabled": body.disabled,
            # Dropbox reports a tagged union as a hash carrying the selected member's name under ".tag"
            "account_type": body.account_type.".tag",
            "country": body.country,
            "locale": body.locale,
            "is_paired": body.is_paired,
            "team_name": body.team.name,
            "team_member_id": body.team_member_id,
            "root_namespace_id": body.root_info.root_namespace_id,
            "home_namespace_id": body.root_info.home_namespace_id,
            "profile_photo_url": body.profile_photo_url,
            "referral_link": body.referral_link,
        };
    }
}

#! Returns how much of the account's storage is in use
public class DropboxGetSpaceUsageDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-space-usage",
            "type": "DropboxGetSpaceUsageDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxEmptyRequestDataType;
        const ResponseType = DropboxSpaceUsageDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # NOTE: as with get_current_account, this endpoint takes no arguments
        hash<auto> body = doRpc("/2/users/get_space_usage");
        # the allocation is a tagged union: an individual account reports its own quota, while a team member
        # reports both the personal allowance and the team total, and either may be absent
        *string tag = body.allocation.".tag";
        *int allocated = body.allocation.allocated;
        int used = body.used ?? 0;
        # NOTE: "auto!"; a literal built from int values narrows to hash<string, int> at runtime, and the float
        # percentage and the string tag would then be rejected
        hash<auto!> rv = {
            "used": used,
            "allocated": allocated,
            "allocation_type": tag,
        };
        if (allocated) {
            rv.used_percent = float(used) * 100.0 / float(allocated);
        }
        if (tag == "team") {
            rv.team_used = body.allocation.used;
            rv.team_allocated = body.allocation.allocated;
        }
        return rv;
    }
}
}
