# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyCreateNoteDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {
public class InsightlyCreateNoteDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create", "desc": "Create a new note in Insightly CRM",
            "type": "InsightlyCreateNoteDataProvider",
            "supports_request": True, "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;

        # Maps the user-facing parent type value to the API path segment
        const ParentTypeToPath = {
            "Contact": "Contacts",
            "Lead": "Leads",
            "Opportunity": "Opportunities",
            "Organisation": "Organisations",
            "Project": "Projects",
        };
    }
    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {}
    string getName() { return "create"; }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new InsightlyCreateNoteRequestDataType();
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new InsightlyNoteDataType();
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Insightly v3.1 requires notes to be created under a parent record;
        # POST /Notes is not supported.
        *string parent_path = ParentTypeToPath{req.PARENT_TYPE};
        if (!parent_path) {
            throw "INSIGHTLY-CREATE-NOTE-ERROR",
                sprintf("invalid PARENT_TYPE %y; must be one of: %y",
                    req.PARENT_TYPE, keys ParentTypeToPath);
        }
        hash<auto> body = req - ("PARENT_TYPE", "PARENT_ID");
        return doPost(sprintf("%s/%d/Notes", parent_path, req.PARENT_ID), body);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
} # namespace InsightlyDataProvider
