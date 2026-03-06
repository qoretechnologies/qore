# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapCreateContactDataProvider.qc Keap create contact action data provider

/*  KeapCreateContactDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Request type for creating a contact
public class KeapCreateContactRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "given_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The contact's first name",
                "desc": "The given (first) name of the contact",
                "example_value": "John",
            },
            "family_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The contact's last name",
                "desc": "The family (last) name of the contact",
                "example_value": "Doe",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The contact's email address",
                "desc": "The email address of the contact",
                "example_value": "john@example.com",
            },
            "phone1": {
                "type": StringOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "The contact's phone number",
                "desc": "The primary phone number of the contact",
                "example_value": "+1-555-123-4567",
            },
            "company_id": {
                "type": IntOrNothingType,
                "display_name": "Company ID",
                "short_desc": "The company to associate",
                "desc": "The ID of the company to associate with the contact",
            },
            "job_title": {
                "type": StringOrNothingType,
                "display_name": "Job Title",
                "short_desc": "The contact's job title",
                "desc": "The job title of the contact",
                "example_value": "Software Engineer",
            },
            "lead_source_id": {
                "type": IntOrNothingType,
                "display_name": "Lead Source ID",
                "short_desc": "The lead source ID",
                "desc": "The ID of the lead source for this contact",
            },
            "opt_in_reason": {
                "type": StringOrNothingType,
                "display_name": "Opt-In Reason",
                "short_desc": "The reason for opting in",
                "desc": "The reason the contact opted in to communications",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Keap create contact action data provider
public class KeapCreateContactDataProvider inherits KeapDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new contact in Keap",
            "type": "KeapCreateContactDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new contact in Keap";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KeapCreateContactRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KeapContactDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("contacts", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KeapDataProvider
