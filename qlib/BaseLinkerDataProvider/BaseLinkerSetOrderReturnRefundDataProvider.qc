# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerSetOrderReturnRefundDataProvider.qc BaseLinker setOrderReturnRefund action
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker `setOrderReturnRefund` action data provider
public class BaseLinkerSetOrderReturnRefundDataProvider inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "set-order-return-refund",
            "desc": "Record a refund against an order return (`setOrderReturnRefund`).",
            "type": "BaseLinkerSetOrderReturnRefundDataProvider",
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

    string getName() { return "set-order-return-refund"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BaseLinkerSetOrderReturnRefundRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BaseLinkerSuccessIdResponseDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return callMethod("setOrderReturnRefund", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
