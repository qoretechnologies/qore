# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyCreateCalendarEventDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {
public class InsightlyCreateCalendarEventDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create", "desc": "Create a new calendar event in Insightly CRM",
            "type": "InsightlyCreateCalendarEventDataProvider",
            "supports_request": True, "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {}
    string getName() { return "create"; }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new InsightlyCreateCalendarEventRequestDataType();
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new InsightlyCalendarEventDataType();
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("Events", req);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
} # namespace InsightlyDataProvider
