# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoInvoiceParserResponseDataType.qc defines the response data type for PDF.co AI invoice parser

/*  PdfCoInvoiceParserResponseDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the PdfCoDataProvider module
public namespace PdfCoDataProvider {

#! Postal address sub-structure used in invoice and customer data
public class PdfCoAddressDataType inherits HashDataType {
    public {
        const Fields = {
            "streetAddress": {
                "type": StringOrNothingType,
                "display_name": "Street Address",
                "short_desc": "Street address",
                "desc": "Street address",
            },
            "city": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "City",
                "desc": "City",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "State or province",
                "desc": "State or province",
            },
            "postalCode": {
                "type": StringOrNothingType,
                "display_name": "Postal Code",
                "short_desc": "Postal or ZIP code",
                "desc": "Postal or ZIP code",
            },
            "country": {
                "type": StringOrNothingType,
                "display_name": "Country",
                "short_desc": "Country",
                "desc": "Country",
            },
        };
    }

    constructor() : HashDataType("Address") {
        addQoreFields(Fields);
    }
}

#! Contact information sub-structure
public class PdfCoContactInformationDataType inherits HashDataType {
    public {
        const Fields = {
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "Phone number",
            },
            "fax": {
                "type": StringOrNothingType,
                "display_name": "Fax",
                "short_desc": "Fax number",
                "desc": "Fax number",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "Email address",
                "desc": "Email address",
            },
        };
    }

    constructor() : HashDataType("ContactInformation") {
        addQoreFields(Fields);
    }
}

#! Vendor information returned by the invoice parser
public class PdfCoInvoiceVendorDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Vendor name",
                "desc": "Vendor name",
            },
            "address": {
                "type": (new PdfCoAddressDataType()).getOrNothingType(),
                "display_name": "Address",
                "short_desc": "Vendor postal address",
                "desc": "Vendor postal address",
            },
            "contactInformation": {
                "type": (new PdfCoContactInformationDataType()).getOrNothingType(),
                "display_name": "Contact Information",
                "short_desc": "Vendor contact details",
                "desc": "Vendor contact details (phone, fax, email)",
            },
            "entityId": {
                "type": StringOrNothingType,
                "display_name": "Entity ID",
                "short_desc": "Vendor tax/business identifier",
                "desc": "Vendor tax or business identifier",
            },
        };
    }

    constructor() : HashDataType("InvoiceVendor") {
        addQoreFields(Fields);
    }
}

#! Customer party (used for both billTo and shipTo)
public class PdfCoInvoicePartyDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Party name",
                "desc": "Party name",
            },
            "address": {
                "type": (new PdfCoAddressDataType()).getOrNothingType(),
                "display_name": "Address",
                "short_desc": "Party postal address",
                "desc": "Party postal address",
            },
            "contactInformation": {
                "type": (new PdfCoContactInformationDataType()).getOrNothingType(),
                "display_name": "Contact Information",
                "short_desc": "Party contact details",
                "desc": "Party contact details (phone, fax, email)",
            },
            "entityId": {
                "type": StringOrNothingType,
                "display_name": "Entity ID",
                "short_desc": "Party tax/business identifier",
                "desc": "Party tax or business identifier",
            },
        };
    }

    constructor() : HashDataType("InvoiceParty") {
        addQoreFields(Fields);
    }
}

#! Customer block (billTo and shipTo)
public class PdfCoInvoiceCustomerDataType inherits HashDataType {
    public {
        const Fields = {
            "billTo": {
                "type": (new PdfCoInvoicePartyDataType()).getOrNothingType(),
                "display_name": "Bill To",
                "short_desc": "Billing party",
                "desc": "Billing party details",
            },
            "shipTo": {
                "type": (new PdfCoInvoicePartyDataType()).getOrNothingType(),
                "display_name": "Ship To",
                "short_desc": "Shipping party",
                "desc": "Shipping party details",
            },
        };
    }

    constructor() : HashDataType("InvoiceCustomer") {
        addQoreFields(Fields);
    }
}

#! Invoice header information
public class PdfCoInvoiceHeaderDataType inherits HashDataType {
    public {
        const Fields = {
            "invoiceNo": {
                "type": StringOrNothingType,
                "display_name": "Invoice Number",
                "short_desc": "Invoice number",
                "desc": "Invoice number",
            },
            "invoiceDate": {
                "type": StringOrNothingType,
                "display_name": "Invoice Date",
                "short_desc": "Invoice date",
                "desc": "Invoice date",
            },
            "poNo": {
                "type": StringOrNothingType,
                "display_name": "PO Number",
                "short_desc": "Purchase order number",
                "desc": "Purchase order number",
            },
            "orderNo": {
                "type": StringOrNothingType,
                "display_name": "Order Number",
                "short_desc": "Order number",
                "desc": "Order number",
            },
        };
    }

    constructor() : HashDataType("InvoiceHeader") {
        addQoreFields(Fields);
    }
}

#! Banking information sub-structure
public class PdfCoBankingInformationDataType inherits HashDataType {
    public {
        const Fields = {
            "bankName": {
                "type": StringOrNothingType,
                "display_name": "Bank Name",
                "short_desc": "Bank name",
                "desc": "Bank name",
            },
            "accountHolderName": {
                "type": StringOrNothingType,
                "display_name": "Account Holder Name",
                "short_desc": "Account holder name",
                "desc": "Account holder name",
            },
            "accountNumber": {
                "type": StringOrNothingType,
                "display_name": "Account Number",
                "short_desc": "Account number",
                "desc": "Account number",
            },
            "iban": {
                "type": StringOrNothingType,
                "display_name": "IBAN",
                "short_desc": "International Bank Account Number",
                "desc": "International Bank Account Number",
            },
            "swiftBicCode": {
                "type": StringOrNothingType,
                "display_name": "SWIFT/BIC Code",
                "short_desc": "SWIFT or BIC code",
                "desc": "SWIFT or BIC code",
            },
            "bankAddress": {
                "type": (new PdfCoAddressDataType()).getOrNothingType(),
                "display_name": "Bank Address",
                "short_desc": "Bank postal address",
                "desc": "Bank postal address",
            },
            "bankRoutingCode": {
                "type": StringOrNothingType,
                "display_name": "Bank Routing Code",
                "short_desc": "Bank routing code",
                "desc": "Bank routing code",
            },
            "bankCode": {
                "type": StringOrNothingType,
                "display_name": "Bank Code",
                "short_desc": "Bank code",
                "desc": "Bank code",
            },
            "branchNumber": {
                "type": StringOrNothingType,
                "display_name": "Branch Number",
                "short_desc": "Branch number",
                "desc": "Branch number",
            },
            "purposeCode": {
                "type": StringOrNothingType,
                "display_name": "Purpose Code",
                "short_desc": "Payment purpose code",
                "desc": "Payment purpose code",
            },
            "additionalNotes": {
                "type": StringOrNothingType,
                "display_name": "Additional Notes",
                "short_desc": "Additional banking notes",
                "desc": "Additional banking notes",
            },
        };
    }

    constructor() : HashDataType("BankingInformation") {
        addQoreFields(Fields);
    }
}

#! Payment details block
public class PdfCoPaymentDetailsDataType inherits HashDataType {
    public {
        const Fields = {
            "paymentTerms": {
                "type": StringOrNothingType,
                "display_name": "Payment Terms",
                "short_desc": "Payment terms",
                "desc": "Payment terms",
            },
            "dueDate": {
                "type": StringOrNothingType,
                "display_name": "Due Date",
                "short_desc": "Payment due date",
                "desc": "Payment due date",
            },
            "total": {
                "type": StringOrNothingType,
                "display_name": "Total",
                "short_desc": "Invoice total",
                "desc": "Invoice total amount",
            },
            "subtotal": {
                "type": StringOrNothingType,
                "display_name": "Subtotal",
                "short_desc": "Invoice subtotal",
                "desc": "Invoice subtotal amount",
            },
            "tax": {
                "type": StringOrNothingType,
                "display_name": "Tax",
                "short_desc": "Tax amount",
                "desc": "Tax amount",
            },
            "discount": {
                "type": StringOrNothingType,
                "display_name": "Discount",
                "short_desc": "Discount amount",
                "desc": "Discount amount",
            },
            "shipping": {
                "type": StringOrNothingType,
                "display_name": "Shipping",
                "short_desc": "Shipping amount",
                "desc": "Shipping amount",
            },
            "bankingInformation": {
                "type": (new PdfCoBankingInformationDataType()).getOrNothingType(),
                "display_name": "Banking Information",
                "short_desc": "Banking details for payment",
                "desc": "Banking details for payment",
            },
        };
    }

    constructor() : HashDataType("PaymentDetails") {
        addQoreFields(Fields);
    }
}

#! Miscellaneous extracted fields
public class PdfCoInvoiceOthersDataType inherits HashDataType {
    public {
        const Fields = {
            "notes": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Free-form notes from the invoice",
                "desc": "Free-form notes extracted from the invoice",
            },
        };
    }

    constructor() : HashDataType("InvoiceOthers") {
        addQoreFields(Fields);
    }
}

#! Body payload returned by the AI invoice parser
public class PdfCoInvoiceParserBodyDataType inherits HashDataType {
    public {
        const Fields = {
            "vendor": {
                "type": (new PdfCoInvoiceVendorDataType()).getOrNothingType(),
                "display_name": "Vendor",
                "short_desc": "Vendor information",
                "desc": "Vendor information extracted from the invoice",
            },
            "customer": {
                "type": (new PdfCoInvoiceCustomerDataType()).getOrNothingType(),
                "display_name": "Customer",
                "short_desc": "Customer billing and shipping information",
                "desc": "Customer billing and shipping information",
            },
            "invoice": {
                "type": (new PdfCoInvoiceHeaderDataType()).getOrNothingType(),
                "display_name": "Invoice",
                "short_desc": "Invoice header fields",
                "desc": "Invoice header fields (number, date, PO, order)",
            },
            "paymentDetails": {
                "type": (new PdfCoPaymentDetailsDataType()).getOrNothingType(),
                "display_name": "Payment Details",
                "short_desc": "Payment terms, totals, and banking information",
                "desc": "Payment terms, totals, and banking information",
            },
            "lineItems": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "Invoice line items",
                "desc": "Invoice line items. Schema depends on the `lineItemStructure` request "
                    "parameter (typically includes `quantity`, `description`, `unit_price`, `total`).",
            },
            "others": {
                "type": (new PdfCoInvoiceOthersDataType()).getOrNothingType(),
                "display_name": "Others",
                "short_desc": "Miscellaneous extracted fields",
                "desc": "Miscellaneous extracted fields (notes, etc.)",
            },
        };
    }

    constructor() : HashDataType("InvoiceParserBody") {
        addQoreFields(Fields);
    }
}

#! Response data type for PDF.co AI invoice parser operations
public class PdfCoInvoiceParserResponseDataType inherits HashDataType {
    public {
        #! Field definitions for the invoice parser response
        const Fields = {
            "body": {
                "type": (new PdfCoInvoiceParserBodyDataType()).getOrNothingType(),
                "display_name": "Body",
                "short_desc": "Structured invoice data with vendor, customer, invoice, payment, and line item sections",
                "desc": "Structured invoice data containing `vendor`, `customer`, `invoice`, "
                    "`paymentDetails`, `lineItems`, and `others` sections extracted by the AI parser",
            },
            "pageCount": {
                "type": SoftIntOrNothingType,
                "display_name": "Page Count",
                "short_desc": "Number of pages processed",
                "desc": "The number of pages processed by the invoice parser",
            },
            "status": {
                "type": SoftIntOrNothingType,
                "display_name": "Status",
                "short_desc": "HTTP status code",
                "desc": "The HTTP status code of the response (e.g. `200` for success)",
            },
            "message": {
                "type": StringOrNothingType,
                "display_name": "Message",
                "short_desc": "Response message",
                "desc": "A message from the API describing the result of the operation",
            },
            "jobId": {
                "type": StringOrNothingType,
                "display_name": "Job ID",
                "short_desc": "The job identifier for async operations",
                "desc": "The unique job identifier that can be used to check the status of "
                    "asynchronous operations via the job check endpoint",
            },
            "credits": {
                "type": SoftIntOrNothingType,
                "display_name": "Credits",
                "short_desc": "Number of credits consumed",
                "desc": "The number of PDF.co credits consumed by this operation",
            },
            "remainingCredits": {
                "type": SoftIntOrNothingType,
                "display_name": "Remaining Credits",
                "short_desc": "Credits remaining in the account",
                "desc": "The number of PDF.co credits remaining in the account after this operation",
            },
            "duration": {
                "type": SoftIntOrNothingType,
                "display_name": "Duration",
                "short_desc": "Processing time in milliseconds",
                "desc": "The processing time for the operation in milliseconds",
            },
            "error": {
                "type": SoftBoolOrNothingType,
                "display_name": "Error",
                "short_desc": "Indicates if the request resulted in an error",
                "desc": "`True` if the request resulted in an error, `False` otherwise",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace PdfCoDataProvider
