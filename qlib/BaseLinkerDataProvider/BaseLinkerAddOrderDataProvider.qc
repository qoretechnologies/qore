# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerAddOrderDataProvider.qc BaseLinker addOrder action
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker `addOrder` action data provider
public class BaseLinkerAddOrderDataProvider inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "add-order",
            "desc": "Create a new order in BaseLinker (`addOrder`).",
            "type": "BaseLinkerAddOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(*RestClientIo::RestClientIo rest) : BaseLinkerDataProviderBase(rest) {
    }
    constructor(BaseLinkerRestClient::BaseLinkerRestConnection conn)
            : BaseLinkerDataProviderBase(conn) {
    }
    constructor(*hash<auto> options) : BaseLinkerDataProviderBase(options) {
    }

    string getName() { return "add-order"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BaseLinkerAddOrderRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BaseLinkerSuccessIdResponseDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = req;
        # date_add is a DateType in the request schema (so the UI gets a date picker);
        # the BaseLinker addOrder API expects Unix seconds, so convert here.
        if (body.date_add instanceof date) {
            body.date_add = int(get_epoch_seconds(body.date_add));
        }
        return callMethod("addOrder", body);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
