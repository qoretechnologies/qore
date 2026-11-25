# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

class VendorAppBillingTest inherits QUnit::Test {
    constructor(string name) : Test(name, "1.0", \ARGV) {
    }

    # Check the catalog values a consumer sees after lazy action registration.
    addVendor(string mod, string app, string recipient, string model, string source) {
        addTestCase(app, sub () {
            load_module(mod + "DataProvider");
            hash<DataProviderAppInfo> info = DataProviderActionCatalog::getAppEx(app);
            hash<DataProviderProcessingInfo> processing = <DataProviderProcessingInfo>{
                "locality": "external", "external_recipient": recipient,
            };
            hash<DataProviderBillingInfo> billing = <DataProviderBillingInfo>{
                "model": model, "source": source, "effective_date": 2026-11-25,
            };
            assertEq(processing, info.processing);
            assertEq(billing, info.billing);
            list<hash<DataProviderActionInfo>> actions = DataProviderActionCatalog::getActions(app);
            assertTrue(actions.size() > 0);
            foreach hash<DataProviderActionInfo> action in (actions) {
                assertEq(processing, DataProviderActionCatalog::getActionProcessing(app, action.action), action.action);
                assertEq(billing, DataProviderActionCatalog::getActionBilling(app, action.action), action.action);
            }
        });
    }
}
