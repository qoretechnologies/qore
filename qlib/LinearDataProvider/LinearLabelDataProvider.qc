# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearLabelDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LinearDataProvider module
public namespace LinearDataProvider {

#! Linear single label data provider
public class LinearLabelDataProvider inherits LinearDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "LinearLabelDataProvider",
            "supports_read": True,
            "supports_update": True,
            "supports_delete": True,
            "has_record": True,
        };
    }

    private {
        #! The label ID
        string labelId;

        #! Record type
        static LinearLabelDataType recordType();
    }

    #! Creates the object
    constructor(LinearRestClient::LinearRestClient rest, string labelId) : LinearDataProviderBase(rest) {
        self.labelId = labelId;
    }

    #! Returns the provider name
    string getName() {
        return labelId;
    }

    #! Returns the provider description
    *string getDesc() {
        return sprintf("Linear label %s data provider", labelId);
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for the label record (returns single record iterator)
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> data = doGraphQL(GetLabelQuery, {"id": labelId});
        return new DefaultRecordIterator(new ListHashIterator(data.issueLabel ? (data.issueLabel,) : ()),
            where_cond, search_options, recordType.getFields());
    }

    #! Updates the label
    private bool updateSingleRecordImpl(hash<auto> set, hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> data = doGraphQL(UpdateLabelMutation, {"id": labelId, "input": set});
        if (!data.issueLabelUpdate.success) {
            throw "UPDATE-ERROR", "Failed to update label";
        }
        return True;
    }

    #! Deletes the label
    private int deleteRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> data = doGraphQL(DeleteLabelMutation, {"id": labelId});
        return data.issueLabelDelete.success ? 1 : 0;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return cast<hash<DataProviderInfo>>(ProviderInfo + {
            "name": labelId,
            "desc": sprintf("Linear label %s", labelId),
        });
    }
}

} # namespace LinearDataProvider
