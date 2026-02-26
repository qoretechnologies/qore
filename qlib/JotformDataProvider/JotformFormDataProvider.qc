# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JotformDataProvider module definition

/*  JotformFormDataProvider.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! JotForm single form data provider
public class JotformFormDataProvider inherits JotformDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "form",
            "desc": "JotForm single form data provider",
            "type": "JotformFormDataProvider",
            "supports_read": True,
            "supports_children": True,
            "has_record": True,
            "children_can_support_records": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "formID": <DataProviderOptionInfo>{
                "display_name": "Form ID",
                "short_desc": "The form ID",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The ID of the form",
                "required": True,
            },
        };
    }

    private {
        #! The form ID
        string formID;

        #! Record type
        static JotformFormDataType recordType();
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
        return formID;
    }

    #! Returns the provider description
    *string getDesc() {
        return sprintf("JotForm form %s", formID);
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return ("submissions", "reports");
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case "submissions":
                return new JotformFormSubmissionsDataProvider(rest, formID);
            case "reports":
                return new JotformFormReportsDataProvider(rest, formID);
        }
    }

    #! Searches for the form (returns single record)
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> response = doGet(sprintf("/form/%s", formID));
        list<hash<auto>> records;
        if (response.content) {
            records = (response.content,);
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
