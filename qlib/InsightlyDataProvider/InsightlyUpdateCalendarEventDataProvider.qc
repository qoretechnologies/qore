# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyUpdateCalendarEventDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {
public class InsightlyUpdateCalendarEventDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update", "desc": "Update an existing calendar event in Insightly CRM",
            "type": "InsightlyUpdateCalendarEventDataProvider",
            "supports_request": True, "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {}
    string getName() { return "update"; }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new InsightlyUpdateCalendarEventRequestDataType();
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new InsightlyCalendarEventDataType();
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPut("Events", req);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
} # namespace InsightlyDataProvider
