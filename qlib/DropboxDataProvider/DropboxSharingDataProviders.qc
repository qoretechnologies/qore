# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DropboxSharingDataProviders.qc sharing action providers for the DropboxDataProvider module

/*  DropboxSharingDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Creates a shared link for a file or folder
public class DropboxCreateSharedLinkDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-shared-link",
            "type": "DropboxCreateSharedLinkDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxCreateSharedLinkRequestDataType;
        const ResponseType = DropboxSharedLinkDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> settings = {};
        if (req.requested_visibility.val()) {
            settings.requested_visibility = {".tag": req.requested_visibility};
        }
        if (req.link_password.val()) {
            settings.link_password = req.link_password;
        }
        if (req.expires) {
            # Dropbox accepts an expiry only in UTC, in the "Z" form
            settings.expires = gmtime(req.expires).format("YYYY-MM-DDTHH:mm:SS") + "Z";
        }

        # "auto!" keeps the single-key literal from narrowing to hash<string, string>, which would then
        # reject the settings hash
        hash<auto!> body = {"path": DropboxDataProviderBase::normalizePath(req.path)};
        if (settings) {
            body.settings = settings;
        }
        return DropboxDataProviderBase::getSharedLinkInfo(
            doRpc("/2/sharing/create_shared_link_with_settings", body));
    }
}

#! Lists the shared links in the account
public class DropboxListSharedLinksDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-shared-links",
            "type": "DropboxListSharedLinksDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxListSharedLinksRequestDataType;
        const ResponseType = DropboxListSharedLinksResponseDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # "auto!" keeps the single-key literal from narrowing to hash<string, bool>, which would then reject
        # the path
        hash<auto!> arg = {"direct_only": req.direct_only ?? False};
        if (req.path.val()) {
            # Dropbox rejects a listing that names the root, so an unset path is omitted rather than sent empty
            string path = DropboxDataProviderBase::normalizePath(req.path);
            if (path != RootPath) {
                arg.path = path;
            }
        }
        hash<auto> body = doRpc("/2/sharing/list_shared_links", arg);
        return {
            "links": (map DropboxDataProviderBase::getSharedLinkInfo($1), body.links),
            "cursor": body.cursor,
            "has_more": body.has_more ?? False,
        };
    }
}

#! Revokes a shared link
public class DropboxRevokeSharedLinkDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "revoke-shared-link",
            "type": "DropboxRevokeSharedLinkDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxRevokeSharedLinkRequestDataType;
        const ResponseType = DropboxSuccessResponseDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # this endpoint answers with an empty body on success, so there is nothing to report but the outcome
        doRpc("/2/sharing/revoke_shared_link", {"url": req.url});
        return {"success": True};
    }
}
}
