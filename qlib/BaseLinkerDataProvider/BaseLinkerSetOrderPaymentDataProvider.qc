# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerSetOrderPaymentDataProvider.qc BaseLinker setOrderPayment action
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker `setOrderPayment` action data provider
public class BaseLinkerSetOrderPaymentDataProvider inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "set-order-payment",
            "desc": "Record a payment against an order (`setOrderPayment`).",
            "type": "BaseLinkerSetOrderPaymentDataProvider",
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

    string getName() { return "set-order-payment"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BaseLinkerSetOrderPaymentRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BaseLinkerSuccessIdResponseDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = req;
        # payment_date is a DateType in the request schema (so the UI gets a date picker);
        # the BaseLinker setOrderPayment API expects Unix seconds, so convert here.
        if (body.payment_date instanceof date) {
            body.payment_date = int(get_epoch_seconds(body.payment_date));
        }
        return callMethod("setOrderPayment", body);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
