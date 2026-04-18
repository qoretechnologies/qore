# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyDeleteCalendarEventDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {
public class InsightlyDeleteCalendarEventDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete", "desc": "Delete a calendar event from Insightly CRM",
            "type": "InsightlyDeleteCalendarEventDataProvider",
            "supports_request": True, "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {}
    string getName() { return "delete"; }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new InsightlyDeleteCalendarEventRequestDataType();
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."nothing";
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete("Events/" + string(req.EVENT_ID));
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
} # namespace InsightlyDataProvider
