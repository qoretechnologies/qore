# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyDeleteNoteDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {
public class InsightlyDeleteNoteDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete", "desc": "Delete a note from Insightly CRM",
            "type": "InsightlyDeleteNoteDataProvider",
            "supports_request": True, "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {}
    string getName() { return "delete"; }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new InsightlyDeleteNoteRequestDataType();
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."nothing";
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete("Notes/" + string(req.NOTE_ID));
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
} # namespace InsightlyDataProvider
