# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SalesforceRestDataProvider module definition

/** SalesforceRestApiTableCreateDataProvider.qc Copyright 2019 - 2025 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SalesforceRestDataProvider module
public namespace SalesforceRestDataProvider {
#! The SalesforceRestApiTableCreateDataProvider data provider class
public class SalesforceRestApiTableCreateDataProvider inherits SalesforceRestObjectDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "SalesforceRestApiTableCreateDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
    }

    private {
        HashDataType request_type;
        HashDataType response_type;
    }

    #! Creates the object from the arguments
    constructor(SalesforceRestClient rest, string name, hash<auto> sobject)
            : SalesforceRestObjectDataProvider(rest, sobject.label, sobject) {
    }

    #! Returns the data provider name
    string getName() {
        return name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Salesforce API create table data provider parent for URL `%s` and sobject `%s`",
            rest.getURL(), name);
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # ignore fields that cannot be created
        req.record -= keys record_info.no_create;
        return {
            "records_affected": 1,
            "record": createRecordImpl(req.record),
        };
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *AbstractDataProviderType getRequestTypeImpl() {
        if (!request_type) {
            request_type = getRequestTypeIntern();
        }
        return request_type;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *AbstractDataProviderType getResponseTypeImpl() {
        if (!response_type) {
            response_type = getResponseTypeIntern();
        }
        return response_type;
    }

    private HashDataType getRequestTypeIntern() {
        string cname = name[0].upr() + name[1..];
        HashDataType rv(AutoHashType, cname + "CreateRequest");
        rv.addField(new QoreDataField({
            "name": "record",
            "display_name": "Record Data",
            "short_desc": "Record data",
            "desc": "Record data",
            "type": new HashDataType(AutoHashType, cname + "CreateRecordRequest", getCreateRecordTypeImpl()),
        }));
        return rv;
    }

    private HashDataType getResponseTypeIntern() {
        string cname = name[0].upr() + name[1..];
        DmlResultType rv(cname + "CreateResponse");
        rv.addField(new QoreDataField({
            "name": "record",
            "display_name": "Record Data",
            "short_desc": "Record data as created in Salesforce",
            "desc": "Record data as created in Salesforce",
            "type": new HashDataType(AutoHashType, cname + "CreateRecordResult", record_info.record_type),
        }));
        return rv;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{
            "name": name,
        } + ProviderInfo;
    }
}
}
