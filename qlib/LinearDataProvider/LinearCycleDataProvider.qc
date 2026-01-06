# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearCycleDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Linear single cycle data provider
public class LinearCycleDataProvider inherits LinearDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "LinearCycleDataProvider",
            "supports_read": True,
            "supports_update": True,
            "supports_delete": True,
            "has_record": True,
        };
    }

    private {
        #! The cycle ID
        string cycleId;

        #! Record type
        static LinearCycleDataType recordType();
    }

    #! Creates the object
    constructor(LinearRestClient::LinearRestClient rest, string cycleId) : LinearDataProviderBase(rest) {
        self.cycleId = cycleId;
    }

    #! Returns the provider name
    string getName() {
        return cycleId;
    }

    #! Returns the provider description
    *string getDesc() {
        return sprintf("Linear cycle %s data provider", cycleId);
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for the cycle record (returns single record iterator)
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> data = doGraphQL(GetCycleQuery, {"id": cycleId});
        return new DefaultRecordIterator(new ListHashIterator(data.cycle ? (data.cycle,) : ()),
            where_cond, search_options, recordType.getFields());
    }

    #! Updates the cycle
    private bool updateSingleRecordImpl(hash<auto> set, hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> data = doGraphQL(UpdateCycleMutation, {"id": cycleId, "input": set});
        if (!data.cycleUpdate.success) {
            throw "UPDATE-ERROR", "Failed to update cycle";
        }
        return True;
    }

    #! Archives (soft-deletes) the cycle
    private int deleteRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> data = doGraphQL(ArchiveCycleMutation, {"id": cycleId});
        return data.cycleArchive.success ? 1 : 0;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return cast<hash<DataProviderInfo>>(ProviderInfo + {
            "name": cycleId,
            "desc": sprintf("Linear cycle %s", cycleId),
        });
    }
}

} # namespace LinearDataProvider
