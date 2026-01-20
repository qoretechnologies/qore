# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JotformDataProvider module definition

/*  JotformFieldDataTypes.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! JotForm full name field type (control_fullname)
/** Used for capturing a person's name with first, middle, last, prefix, and suffix components.
    The JotForm API expects data in the format: submission[qid_first], submission[qid_last], etc.
*/
public class JotformFullNameDataType inherits HashDataType {
    private {
        #! Field definitions for full name
        const Fields = {
            "first": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The person's first name",
                "desc": "The first (given) name",
                "example_value": "John",
            },
            "middle": {
                "type": StringOrNothingType,
                "display_name": "Middle Name",
                "short_desc": "The person's middle name",
                "desc": "The middle name (if configured in the form)",
                "example_value": "William",
            },
            "last": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The person's last name",
                "desc": "The last (family) name",
                "example_value": "Smith",
            },
            "prefix": {
                "type": StringOrNothingType,
                "display_name": "Prefix",
                "short_desc": "Name prefix/title",
                "desc": "The name prefix or title (e.g., Mr., Mrs., Dr.)",
                "example_value": "Mr.",
            },
            "suffix": {
                "type": StringOrNothingType,
                "display_name": "Suffix",
                "short_desc": "Name suffix",
                "desc": "The name suffix (e.g., Jr., Sr., III)",
                "example_value": "Jr.",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("FullName") {
        addQoreFields(Fields);
    }
}

#! JotForm address field type (control_address)
/** Used for capturing a complete address with street, city, state, postal code, and country.
    The JotForm API expects data in the format: submission[qid_addr_line1], submission[qid_city], etc.
*/
public class JotformAddressDataType inherits HashDataType {
    private {
        #! Field definitions for address
        const Fields = {
            "addr_line1": {
                "type": StringOrNothingType,
                "display_name": "Address Line 1",
                "short_desc": "Street address line 1",
                "desc": "The primary street address",
                "example_value": "123 Main Street",
            },
            "addr_line2": {
                "type": StringOrNothingType,
                "display_name": "Address Line 2",
                "short_desc": "Street address line 2",
                "desc": "Additional address information (apt, suite, building, etc.)",
                "example_value": "Suite 100",
            },
            "city": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "City name",
                "desc": "The city",
                "example_value": "New York",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State/Province",
                "short_desc": "State or province",
                "desc": "The state, province, or region",
                "example_value": "NY",
            },
            "postal": {
                "type": StringOrNothingType,
                "display_name": "Postal Code",
                "short_desc": "ZIP or postal code",
                "desc": "The ZIP or postal code",
                "example_value": "10001",
            },
            "country": {
                "type": StringOrNothingType,
                "display_name": "Country",
                "short_desc": "Country",
                "desc": "The country (if configured in the form)",
                "example_value": "United States",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("Address") {
        addQoreFields(Fields);
    }
}

#! JotForm phone field type (control_phone)
/** Used for capturing a phone number with area code and phone number components.
    The JotForm API expects data in the format: submission[qid_area], submission[qid_phone], etc.
*/
public class JotformPhoneDataType inherits HashDataType {
    private {
        #! Field definitions for phone
        const Fields = {
            "area": {
                "type": StringOrNothingType,
                "display_name": "Area Code",
                "short_desc": "The area code",
                "desc": "The phone area code",
                "example_value": "212",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "The phone number",
                "desc": "The phone number (without area code)",
                "example_value": "555-1234",
            },
            "full": {
                "type": StringOrNothingType,
                "display_name": "Full Number",
                "short_desc": "The full phone number",
                "desc": "The complete phone number (for international format)",
                "example_value": "+1 212 555-1234",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("Phone") {
        addQoreFields(Fields);
    }
}

#! JotForm email field type (control_email)
/** While email is typically a simple string, JotForm can return it in a structured format
    with the email address and a confirmation field.
*/
public class JotformEmailDataType inherits HashDataType {
    private {
        #! Field definitions for email
        const Fields = {
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email Address",
                "short_desc": "The email address",
                "desc": "The primary email address",
                "example_value": "user@example.com",
            },
            "confirmation": {
                "type": StringOrNothingType,
                "display_name": "Confirmation Email",
                "short_desc": "The confirmation email",
                "desc": "The confirmation email address (if email confirmation is enabled)",
                "example_value": "user@example.com",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("Email") {
        addQoreFields(Fields);
    }
}

#! JotForm date/time field type (control_datetime)
/** Used for capturing date and time with separate components.
*/
public class JotformDateTimeDataType inherits HashDataType {
    private {
        #! Field definitions for datetime
        const Fields = {
            "year": {
                "type": StringOrNothingType,
                "display_name": "Year",
                "short_desc": "The year",
                "desc": "The year (YYYY format)",
                "example_value": "2025",
            },
            "month": {
                "type": StringOrNothingType,
                "display_name": "Month",
                "short_desc": "The month",
                "desc": "The month (1-12)",
                "example_value": "01",
            },
            "day": {
                "type": StringOrNothingType,
                "display_name": "Day",
                "short_desc": "The day",
                "desc": "The day of the month (1-31)",
                "example_value": "15",
            },
            "hour": {
                "type": StringOrNothingType,
                "display_name": "Hour",
                "short_desc": "The hour",
                "desc": "The hour (0-23 or 1-12 depending on format)",
                "example_value": "14",
            },
            "min": {
                "type": StringOrNothingType,
                "display_name": "Minute",
                "short_desc": "The minute",
                "desc": "The minute (0-59)",
                "example_value": "30",
            },
            "ampm": {
                "type": StringOrNothingType,
                "display_name": "AM/PM",
                "short_desc": "AM or PM",
                "desc": "AM or PM (for 12-hour format)",
                "example_value": "PM",
            },
            "datetime": {
                "type": StringOrNothingType,
                "display_name": "Date/Time String",
                "short_desc": "The formatted date/time",
                "desc": "The full date/time string",
                "example_value": "2025-01-15 14:30",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("DateTime") {
        addQoreFields(Fields);
    }
}

#! JotForm birthdate field type (control_birthdate)
/** Used for capturing date of birth with separate components.
*/
public class JotformBirthDateDataType inherits HashDataType {
    private {
        #! Field definitions for birthdate
        const Fields = {
            "year": {
                "type": StringOrNothingType,
                "display_name": "Year",
                "short_desc": "Birth year",
                "desc": "The birth year (YYYY format)",
                "example_value": "1990",
            },
            "month": {
                "type": StringOrNothingType,
                "display_name": "Month",
                "short_desc": "Birth month",
                "desc": "The birth month (1-12 or month name)",
                "example_value": "January",
            },
            "day": {
                "type": StringOrNothingType,
                "display_name": "Day",
                "short_desc": "Birth day",
                "desc": "The birth day (1-31)",
                "example_value": "15",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("BirthDate") {
        addQoreFields(Fields);
    }
}

#! JotForm time field type (control_time)
/** Used for capturing time only.
*/
public class JotformTimeDataType inherits HashDataType {
    private {
        #! Field definitions for time
        const Fields = {
            "hourSelect": {
                "type": StringOrNothingType,
                "display_name": "Hour",
                "short_desc": "The hour",
                "desc": "The hour value",
                "example_value": "14",
            },
            "minuteSelect": {
                "type": StringOrNothingType,
                "display_name": "Minute",
                "short_desc": "The minute",
                "desc": "The minute value",
                "example_value": "30",
            },
            "ampm": {
                "type": StringOrNothingType,
                "display_name": "AM/PM",
                "short_desc": "AM or PM",
                "desc": "AM or PM (for 12-hour format)",
                "example_value": "PM",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("Time") {
        addQoreFields(Fields);
    }
}

#! JotForm matrix field type (control_matrix)
/** Used for capturing matrix/grid responses where rows and columns form a grid of choices.
*/
public class JotformMatrixDataType inherits HashDataType {
    private {
        #! Field definitions for matrix - dynamic based on row/column configuration
        const Fields = {};
    }

    #! Creates the type
    /** Matrix fields are dynamic - each row becomes a key with the selected column value.
        Since the structure depends on form configuration, we use a flexible hash.
    */
    constructor() : HashDataType("Matrix") {
        # Matrix fields have dynamic structure based on form configuration
        # Each row ID maps to the selected column value(s)
        # We allow any string keys with string or list values
    }
}

#! JotForm payment field type (control_payment)
/** Used for capturing payment information.
*/
public class JotformPaymentDataType inherits HashDataType {
    private {
        #! Field definitions for payment
        const Fields = {
            "paymentArray": {
                "type": AutoHashOrNothingType,
                "display_name": "Payment Details",
                "short_desc": "Payment item details",
                "desc": "Array of payment items with quantities and prices",
            },
            "total": {
                "type": StringOrNothingType,
                "display_name": "Total",
                "short_desc": "Total amount",
                "desc": "The total payment amount",
                "example_value": "99.99",
            },
            "currency": {
                "type": StringOrNothingType,
                "display_name": "Currency",
                "short_desc": "Payment currency",
                "desc": "The currency code (USD, EUR, etc.)",
                "example_value": "USD",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("Payment") {
        addQoreFields(Fields);
    }
}

#! JotForm signature field type (control_signature)
/** Used for capturing electronic signatures.
*/
public class JotformSignatureDataType inherits HashDataType {
    private {
        #! Field definitions for signature
        const Fields = {
            "signature": {
                "type": StringOrNothingType,
                "display_name": "Signature Data",
                "short_desc": "Base64 encoded signature",
                "desc": "The signature image data (base64 encoded)",
            },
            "signedName": {
                "type": StringOrNothingType,
                "display_name": "Signed Name",
                "short_desc": "Name of signer",
                "desc": "The typed name of the signer",
                "example_value": "John Smith",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("Signature") {
        addQoreFields(Fields);
    }
}

} # namespace JotformDataProvider
