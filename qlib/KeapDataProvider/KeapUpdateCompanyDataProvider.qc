# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapUpdateCompanyDataProvider.qc Keap update company action data provider

/*  KeapUpdateCompanyDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a company
public class KeapUpdateCompanyRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "company_id": {
                "type": StringType,
                "display_name": "Company ID",
                "short_desc": "The company ID to update",
                "desc": "The unique identifier of the company to update",
                "required": True,
            },
            "company_name": {
                "type": StringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "The company name",
                "desc": "The new name of the company",
            },
            "email_address": {
                "type": new KeapCompanyEmailDataType(AutoHashOrNothingType),
                "display_name": "Email Address",
                "short_desc": "The company's email address",
                "desc": "The new email address of the company as an object with `email` "
                    "and optionally `field`",
            },
            "phone_number": {
                "type": new KeapPhoneNumberDataType(AutoHashOrNothingType),
                "display_name": "Phone Number",
                "short_desc": "The company's phone number",
                "desc": "The new phone number of the company as an object with `number` "
                    "and optionally `field` and `type`",
            },
            "website": {
                "type": StringOrNothingType,
                "display_name": "Website",
                "short_desc": "The company's website",
                "desc": "The new website URL of the company",
            },
            "address": {
                "type": new KeapAddressDataType(AutoHashOrNothingType),
                "display_name": "Address",
                "short_desc": "The company's address",
                "desc": "The new physical address of the company",
            },
            "notes": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Notes about the company",
                "desc": "The new notes about the company",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Keap update company action data provider
public class KeapUpdateCompanyDataProvider inherits KeapDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing company in Keap",
            "type": "KeapUpdateCompanyDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType ResponseType = new KeapCompanyDataType();
        static *AbstractDataProviderType RequestType = new KeapUpdateCompanyRequestDataType();
    }

    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing company in Keap";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KeapUpdateCompanyRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KeapCompanyDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = remove req.company_id;
        return doPatch("companies/" + id, req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KeapDataProvider
