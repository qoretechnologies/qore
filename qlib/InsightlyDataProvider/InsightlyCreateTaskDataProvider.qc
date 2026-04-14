# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyCreateTaskDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {
public class InsightlyCreateTaskDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create", "desc": "Create a new task in Insightly CRM",
            "type": "InsightlyCreateTaskDataProvider",
            "supports_request": True, "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {}
    string getName() { return "create"; }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new InsightlyCreateTaskRequestDataType();
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new InsightlyTaskDataType();
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("Tasks", req);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
} # namespace InsightlyDataProvider
