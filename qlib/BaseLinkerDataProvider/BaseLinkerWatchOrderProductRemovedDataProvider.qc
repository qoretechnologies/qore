# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerWatchOrderProductRemovedDataProvider.qc watch provider for BaseLinker Order Product Removed

/*  BaseLinkerWatchOrderProductRemovedDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker polling watch provider: Order Product Removed
/** Fires whenever A product is removed from an order (BaseLinker journal `log_type` 14).
*/
public class BaseLinkerWatchOrderProductRemovedDataProvider inherits BaseLinkerWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_ORDER_PRODUCT_REMOVED,
            "desc": "Polling-based watch provider that fires when a product is removed from an order "
                "(BaseLinker journal `log_type` 14).",
            "type": "BaseLinkerWatchOrderProductRemovedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with an async REST client
    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : BaseLinkerWatchDataProviderBase(rest, options) {
    }

    #! Creates the object with a connection
    constructor(BaseLinkerRestClient::BaseLinkerRestConnection conn, *hash<auto> options)
            : BaseLinkerWatchDataProviderBase(conn, options) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : BaseLinkerWatchDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_ORDER_PRODUCT_REMOVED;
    }

    #! Returns the provider description
    *string getDesc() {
        return ProviderInfo.desc;
    }

    #! Returns the BaseLinker numeric log_type code
    int getLogType() {
        return 14;
    }

    #! Returns the event name constant
    string getEventName() {
        return EVENT_ORDER_PRODUCT_REMOVED;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
