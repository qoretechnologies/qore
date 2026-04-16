# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyUpdateNoteDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {
public class InsightlyUpdateNoteDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update", "desc": "Update an existing note in Insightly CRM",
            "type": "InsightlyUpdateNoteDataProvider",
            "supports_request": True, "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {}
    string getName() { return "update"; }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new InsightlyUpdateNoteRequestDataType();
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new InsightlyNoteDataType();
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPut("Notes", req);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
} # namespace InsightlyDataProvider
