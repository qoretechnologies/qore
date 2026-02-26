# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JotformDataProvider module definition

/*  JotformWebhookEventDataTypes.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the JotformDataProvider module
public namespace JotformDataProvider {

#! JotForm submission webhook event data type
/** This represents the data sent by JotForm webhooks when a new submission is received.
    JotForm sends submission data in JSON format with the form submission details.
*/
public class JotformSubmissionWebhookEventDataType inherits HashDataType {
    public {
        const Fields = {
            "submissionID": {
                "type": StringType,
                "display_name": "Submission ID",
                "short_desc": "The unique identifier of the submission",
                "desc": "The unique identifier of the new submission",
                "example_value": "5432109876543210987",
            },
            "formID": {
                "type": StringType,
                "display_name": "Form ID",
                "short_desc": "The form that received the submission",
                "desc": "The ID of the form that received the submission",
                "example_value": "123456789012345678",
            },
            "formTitle": {
                "type": StringOrNothingType,
                "display_name": "Form Title",
                "short_desc": "The title of the form",
                "desc": "The title of the form that received the submission",
                "example_value": "Contact Form",
            },
            "ip": {
                "type": StringOrNothingType,
                "display_name": "IP Address",
                "short_desc": "The submitter's IP address",
                "desc": "The IP address of the user who submitted the form",
                "example_value": "192.168.1.100",
            },
            "createdAt": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the submission was created",
                "desc": "The date and time when the submission was created",
                "example_value": "2025-01-15 10:30:45",
            },
            "answers": {
                "type": AutoHashOrNothingType,
                "display_name": "Answers",
                "short_desc": "The form submission answers",
                "desc": "The form field answers as a hash. The structure depends on the form fields.",
            },
            "rawRequest": {
                "type": AutoHashOrNothingType,
                "display_name": "Raw Request",
                "short_desc": "The raw webhook request data",
                "desc": "The complete raw request data sent by JotForm webhook",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! JotForm payment webhook event data type
/** This represents the data sent by JotForm webhooks when a payment is received on a form.
    This includes both the submission data and payment-specific information.
*/
public class JotformPaymentWebhookEventDataType inherits HashDataType {
    public {
        const Fields = {
            "submissionID": {
                "type": StringType,
                "display_name": "Submission ID",
                "short_desc": "The unique identifier of the submission",
                "desc": "The unique identifier of the submission with payment",
                "example_value": "5432109876543210987",
            },
            "formID": {
                "type": StringType,
                "display_name": "Form ID",
                "short_desc": "The form that received the payment",
                "desc": "The ID of the form that received the payment",
                "example_value": "123456789012345678",
            },
            "formTitle": {
                "type": StringOrNothingType,
                "display_name": "Form Title",
                "short_desc": "The title of the form",
                "desc": "The title of the form that received the payment",
                "example_value": "Order Form",
            },
            "ip": {
                "type": StringOrNothingType,
                "display_name": "IP Address",
                "short_desc": "The payer's IP address",
                "desc": "The IP address of the user who made the payment",
                "example_value": "192.168.1.100",
            },
            "createdAt": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the payment was made",
                "desc": "The date and time when the payment was completed",
                "example_value": "2025-01-15 10:30:45",
            },
            "paymentType": {
                "type": StringOrNothingType,
                "display_name": "Payment Type",
                "short_desc": "The payment processor used",
                "desc": "The payment processor/gateway used (e.g., PayPal, Stripe, Square)",
                "example_value": "stripe",
            },
            "paymentAmount": {
                "type": StringOrNothingType,
                "display_name": "Payment Amount",
                "short_desc": "The total payment amount",
                "desc": "The total amount of the payment",
                "example_value": "99.99",
            },
            "paymentCurrency": {
                "type": StringOrNothingType,
                "display_name": "Payment Currency",
                "short_desc": "The payment currency",
                "desc": "The currency code for the payment (e.g., USD, EUR)",
                "example_value": "USD",
            },
            "paymentStatus": {
                "type": StringOrNothingType,
                "display_name": "Payment Status",
                "short_desc": "The status of the payment",
                "desc": "The status of the payment (e.g., COMPLETED, PENDING)",
                "example_value": "COMPLETED",
            },
            "transactionID": {
                "type": StringOrNothingType,
                "display_name": "Transaction ID",
                "short_desc": "The payment transaction ID",
                "desc": "The unique transaction ID from the payment processor",
                "example_value": "txn_1234567890abcdef",
            },
            "answers": {
                "type": AutoHashOrNothingType,
                "display_name": "Answers",
                "short_desc": "The form submission answers",
                "desc": "The form field answers as a hash",
            },
            "rawRequest": {
                "type": AutoHashOrNothingType,
                "display_name": "Raw Request",
                "short_desc": "The raw webhook request data",
                "desc": "The complete raw request data sent by JotForm webhook",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace JotformDataProvider
