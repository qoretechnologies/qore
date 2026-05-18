# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaBatchStocksDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "batch-stocks", "desc": "Katana batch stocks collection (read-only)",
            "type": "KatanaBatchStocksDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "variant_id": <DataProviderOptionInfo>{
                "display_name": "Variant ID", "short_desc": "Filter by variant",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by variant ID", "ref_data": "variants"},
            "location_id": <DataProviderOptionInfo>{
                "display_name": "Location ID", "short_desc": "Filter by location",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by location ID", "ref_data": "locations"},
            "batch_id": <DataProviderOptionInfo>{
                "display_name": "Batch ID", "short_desc": "Filter by batch",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by batch ID"},
            "batch_number": <DataProviderOptionInfo>{
                "display_name": "Batch Number", "short_desc": "Filter by batch number",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by batch number"},
            "expiration_date_min": <DataProviderOptionInfo>{
                "display_name": "Expiration Date (Min)",
                "short_desc": "Earliest expiration date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Only return batches expiring on/after this date"},
            "expiration_date_max": <DataProviderOptionInfo>{
                "display_name": "Expiration Date (Max)",
                "short_desc": "Latest expiration date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Only return batches expiring on/before this date"},
        } + CommonSearchOptions;
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "batch-stocks"; }
    *string getDesc() { return "Katana batch stocks collection (read-only)"; }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new KatanaBatchStockDataType().getFields();
    }
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return runListSearch("batch_stocks", where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
