# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ChargebeeDataProvider module
public namespace ChargebeeDataProvider {

#! The main Chargebee data provider class (root provider)
public class ChargebeeDataProvider inherits ChargebeeDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "chargebee",
            "desc": "Chargebee data provider for subscription billing and revenue management",
            "type": "ChargebeeDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map
        const ChildMap = {
            # Collection providers
            "customers": Class::forName("ChargebeeCustomersDataProvider"),
            "subscriptions": Class::forName("ChargebeeSubscriptionsDataProvider"),
            "invoices": Class::forName("ChargebeeInvoicesDataProvider"),
            "coupons": Class::forName("ChargebeeCouponsDataProvider"),
            "promotional-credits": Class::forName("ChargebeePromotionalCreditsDataProvider"),

            # Customer actions
            "create-customer": Class::forName("ChargebeeCreateCustomerDataProvider"),
            "update-customer": Class::forName("ChargebeeUpdateCustomerDataProvider"),
            "delete-customer": Class::forName("ChargebeeDeleteCustomerDataProvider"),

            # Subscription actions
            "create-subscription": Class::forName("ChargebeeCreateSubscriptionDataProvider"),
            "update-subscription": Class::forName("ChargebeeUpdateSubscriptionDataProvider"),
            "delete-subscription": Class::forName("ChargebeeDeleteSubscriptionDataProvider"),

            # Invoice actions
            "create-invoice": Class::forName("ChargebeeCreateInvoiceDataProvider"),
            "update-invoice": Class::forName("ChargebeeUpdateInvoiceDataProvider"),
            "delete-invoice": Class::forName("ChargebeeDeleteInvoiceDataProvider"),
            "collect-invoice-payment": Class::forName("ChargebeeCollectInvoicePaymentDataProvider"),
            "record-invoice-payment": Class::forName("ChargebeeRecordInvoicePaymentDataProvider"),
            "refund-invoice": Class::forName("ChargebeeRefundInvoiceDataProvider"),
            "get-invoice-link": Class::forName("ChargebeeGetInvoiceLinkDataProvider"),

            # Coupon actions
            "create-coupon": Class::forName("ChargebeeCreateCouponDataProvider"),
            "update-coupon": Class::forName("ChargebeeUpdateCouponDataProvider"),
            "delete-coupon": Class::forName("ChargebeeDeleteCouponDataProvider"),

            # Promotional credit actions
            "add-promotional-credit": Class::forName("ChargebeeAddPromotionalCreditDataProvider"),
            "deduct-promotional-credit": Class::forName("ChargebeeDeductPromotionalCreditDataProvider"),
            "set-promotional-credit": Class::forName("ChargebeeSetPromotionalCreditDataProvider"),

            # Transaction actions
            "get-transaction": Class::forName("ChargebeeGetTransactionDataProvider"),

            # Event providers
            "events": Class::forName("ChargebeeEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : ChargebeeDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(ChargebeeRestClient::ChargebeeRestClient rest)
            : ChargebeeDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ChargebeeRestClient::ChargebeeRestConnection conn) : ChargebeeDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "chargebee";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Chargebee data provider for subscription billing and revenue management";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ChargebeeDataProvider
