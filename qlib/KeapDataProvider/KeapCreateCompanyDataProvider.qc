# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapCreateCompanyDataProvider.qc Keap create company action data provider

/*  KeapCreateCompanyDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a company
public class KeapCreateCompanyRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "company_name": {
                "type": StringType,
                "display_name": "Company Name",
                "short_desc": "The company name",
                "desc": "The name of the company to create",
                "required": True,
                "example_value": "Acme Corp",
            },
            "email_address": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The company's email address",
                "desc": "The email address of the company",
                "example_value": "info@acme.com",
            },
            "phone_number": {
                "type": StringOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "The company's phone number",
                "desc": "The phone number of the company",
                "example_value": "+1-555-987-6543",
            },
            "website": {
                "type": StringOrNothingType,
                "display_name": "Website",
                "short_desc": "The company's website",
                "desc": "The website URL of the company",
                "example_value": "https://www.acme.com",
            },
            "address": {
                "type": AutoHashOrNothingType,
                "display_name": "Address",
                "short_desc": "The company's address",
                "desc": "The physical address of the company (hash with line1, line2, "
                    "locality, region, zip_code, country_code)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Keap create company action data provider
public class KeapCreateCompanyDataProvider inherits KeapDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new company in Keap",
            "type": "KeapCreateCompanyDataProvider",
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
        return "Create a new company in Keap";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KeapCreateCompanyRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KeapCompanyDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("companies", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KeapDataProvider
