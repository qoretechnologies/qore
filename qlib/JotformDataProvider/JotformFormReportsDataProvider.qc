# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JotformDataProvider module definition

/*  JotformFormReportsDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! JotForm form reports data provider
public class JotformFormReportsDataProvider inherits JotformDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "reports",
            "desc": "JotForm form reports data provider",
            "type": "JotformFormReportsDataProvider",
            "supports_read": True,
            "has_record": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "formID": <DataProviderOptionInfo>{
                "display_name": "Form ID",
                "short_desc": "The form ID",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The ID of the form to get reports for",
                "required": True,
            },
        };
    }

    private {
        #! The form ID
        string formID;

        #! Record type
        static JotformReportDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(JotformRestClient::JotformRestClient rest, string formID) : JotformDataProviderBase(rest) {
        @assert(formID.val(), "form ID cannot be empty");
        self.formID = formID;
    }

    #! Creates the object with a connection
    constructor(JotformRestClient::JotformRestConnection conn, string formID) : JotformDataProviderBase(conn) {
        @assert(formID.val(), "form ID cannot be empty");
        self.formID = formID;
    }

    #! Returns the provider name
    string getName() {
        return "reports";
    }

    #! Returns the provider description
    *string getDesc() {
        return sprintf("JotForm reports for form %s", formID);
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for reports
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> response = doGet(sprintf("/form/%s/reports", formID));
        list<hash<auto>> records = ();
        if (response.content) {
            # Ensure content is a list
            if (response.content.typeCode() == NT_LIST) {
                records = response.content;
            } else if (response.content.typeCode() == NT_HASH) {
                records = (response.content,);
            }
        }
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace JotformDataProvider
