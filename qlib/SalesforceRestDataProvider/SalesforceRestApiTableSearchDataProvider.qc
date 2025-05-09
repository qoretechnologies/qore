# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SalesforceRestDataProvider module definition

/** SalesforceRestApiTableSearchDataProvider.qc Copyright 2019 - 2025 Qore Technologies, s.r.o.

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
#! The SalesforceRestApiTableSearchDataProvider data provider class
public class SalesforceRestApiTableSearchDataProvider inherits SalesforceRestObjectDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "SalesforceRestApiTableSearchDataProvider",
            "supports_request": True,
        };
    }

    private {
        HashDataType request_type;
        HashDataType response_type;
    }

    #! Searchs the object from the arguments
    constructor(SalesforceRestClient rest, string name, hash<auto> sobject)
            : SalesforceRestObjectDataProvider(rest, sobject.label, sobject) {
    }

    #! Returns the data provider name
    string getName() {
        return name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Salesforce API search table data provider parent for URL `%s` and sobject `%s`",
            rest.getURL(), name);
    }

    #! Make the request and return the result
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        *list<auto> rv = map $1, searchRecordsImpl(req."where", req - "where");
        return {
            "records_affected": rv.size(),
            "records": rv,
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
        SearchRequestTypeBase rv(cname + "SearchRequest");
        rv.addField(new QoreDataField({
            "name": "where",
            "display_name": "Where Clause",
            "short_desc": "Field values to match exactly",
            "desc": "Field values to match exactly",
            "type": new HashDataType(AutoHashType, cname + "SearchRecordRequest", getOrNothingRecordType()),
        }));
        return rv;
    }

    private HashDataType getResponseTypeIntern() {
        string cname = name[0].upr() + name[1..];
        DmlResultType rv(cname + "SearchResponse");
        rv.addField(new QoreDataField({
            "name": "records",
            "display_name": "Record Data",
            "short_desc": "Record data returned from the query",
            "desc": "Record data return from the query",
            "type": new ListDataType("RecordSet", new HashDataType(AutoHashType,
                sprintf("%sSearchRecordResult", cname), record_info.record_type)),
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

class SearchRequestTypeBase inherits HashDataType {
    public {
        const Fields = {
            "columns": {
                "display_name": "Columns",
                "short_desc": "Columns to include in output",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "desc": "column argument for the SOQL select expression",
            },
            "scope": {
                "display_name": "Scope",
                "short_desc": "Query scope",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The scope to use for the query",
            },
            "limit": {
                "display_name": "Limit",
                "short_desc": "The maximum number of records to return",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "the maximum number of records to return",
            },
            "offset": {
                "display_name": "Offset",
                "short_desc": "The offset in the record list for records to return",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "The offset in the record list for records to return",
            },
            "groupby": {
                "display_name": "Group By",
                "short_desc": "Field(s) to group results by",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "desc": "Field(s) to group results by in case there are aggregate expressions in the query; group "
                    "by argument for the SOQL select expression",
            },
            "having": {
                "display_name": "Having Clause",
                "short_desc": "A 'having' clause for the select expression",
                "type": AbstractDataProviderTypeMap."*hash",
                "desc": "A `having` clause for the SOQL select expression",
            },
            "orderby": {
                "display_name": "Order By",
                "short_desc": "Fields to order the query result by",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "desc": "Sets the order by argument for the SOQL select expression",
            },
            "orderby_desc": {
                "display_name": "Order By Descending",
                "short_desc": "Sort in descending order",
                "type": AbstractDataProviderTypeMap."*softbool",
                "desc": "Sort in descending order",
            },
            "forview": {
                "display_name": "Use For View",
                "short_desc": "Uses FOR VIEW with the query",
                "type": AbstractDataProviderTypeMap."*softbool",
                "desc": "Uses `FOR VIEW` with the query to update the view date",
            },
            "forreference": {
                "display_name": "Use For Reference",
                "short_desc": "Uses FOR REFERENCE with the query",
                "type": AbstractDataProviderTypeMap."*softbool",
                "desc": "Uses `FOR REFERENCE` with the query to update the reference date",
            },
        };
    }

    constructor(*string name) : HashDataType(AutoHashType, name) {
        addQoreFields(Fields);
    }
}
}
