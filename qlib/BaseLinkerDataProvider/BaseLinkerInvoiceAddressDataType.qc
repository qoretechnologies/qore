# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerInvoiceAddressDataType.qc invoice address sub-type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Invoice / billing address fields used by addOrder / setOrderFields
public class BaseLinkerInvoiceAddressDataType inherits HashDataType {
    public {
        const Fields = {
            "invoice_fullname": {
                "display_name": "Invoice Full Name",
                "short_desc": "Billing recipient full name",
                "type": SoftStringOrNothingType,
            },
            "invoice_company": {
                "display_name": "Invoice Company",
                "short_desc": "Billing company name",
                "type": SoftStringOrNothingType,
            },
            "invoice_nip": {
                "display_name": "Invoice Tax ID",
                "short_desc": "NIP / VAT / tax identifier",
                "type": SoftStringOrNothingType,
                "desc": "Tax ID (NIP in Poland, VAT ID in the EU, etc.).",
            },
            "invoice_address": {
                "display_name": "Invoice Address",
                "short_desc": "Billing street address",
                "type": SoftStringOrNothingType,
            },
            "invoice_city": {
                "display_name": "Invoice City",
                "short_desc": "Billing city",
                "type": SoftStringOrNothingType,
            },
            "invoice_state": {
                "display_name": "Invoice State/Province",
                "short_desc": "Billing state/province",
                "type": SoftStringOrNothingType,
            },
            "invoice_postcode": {
                "display_name": "Invoice Postcode",
                "short_desc": "Billing postal code",
                "type": SoftStringOrNothingType,
            },
            "invoice_country_code": {
                "display_name": "Invoice Country Code",
                "short_desc": "Billing 2-letter ISO country code",
                "type": SoftStringOrNothingType,
            },
        };
    }
    constructor() : HashDataType("BaseLinkerInvoiceAddressDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
