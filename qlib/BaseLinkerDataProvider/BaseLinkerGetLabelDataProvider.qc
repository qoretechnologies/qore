# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerGetLabelDataProvider.qc BaseLinker getLabel action
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker `getLabel` action data provider
/** Returns the courier label file for a given package. The raw BaseLinker
    response (`label` = base64 string, `extension` = file format) is transformed
    into a @ref DataProvider::FileDataType wrapper with `content`, `name`, and
    `mime_type` populated.
*/
public class BaseLinkerGetLabelDataProvider inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-label",
            "desc": "Fetch the courier label file for a package (`getLabel`).",
            "type": "BaseLinkerGetLabelDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        #! Map of BaseLinker `extension` values to IANA MIME types
        const ExtensionMimeMap = {
            "pdf": "application/pdf",
            "html": "text/html",
            "gif": "image/gif",
            "png": "image/png",
            "epl": "application/vnd.eltron-epl",
            "zpl": "application/vnd.zebra-zpl",
            "dpl": "application/vnd.datamax-dpl",
        };
    }

    constructor(*RestClientIo::RestClientIo rest) : BaseLinkerDataProviderBase(rest) {
    }
    constructor(BaseLinkerRestClient::BaseLinkerRestConnection conn)
            : BaseLinkerDataProviderBase(conn) {
    }
    constructor(*hash<auto> options) : BaseLinkerDataProviderBase(options) {
    }

    string getName() { return "get-label"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BaseLinkerGetLabelRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BaseLinkerCourierLabelDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> response = callMethod("getLabel", req);
        *string ext = response.extension;
        *string mime = ext.val() ? ExtensionMimeMap{ext.lwr()} : NOTHING;
        hash<auto> rv = response + {
            "label": response.label.val()
                ? {
                    "name": ext.val() ? sprintf("label.%s", ext.lwr()) : "label",
                    "content": response.label,
                    "mime_type": mime ?? "application/octet-stream",
                }
                : NOTHING,
        };
        return rv;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
