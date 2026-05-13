# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaDataProviderFactory.qc Katana data provider factory

/*  KatanaDataProviderFactory.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/

public namespace KatanaDataProvider {

#! The Katana data provider factory
public class KatanaDataProviderFactory inherits DataProvider::AbstractDataProviderFactory {
    private {
        static Class cls = new Class("KatanaDataProvider");

        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "katana",
            "desc": "Katana MRP data provider factory",
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_observers": True,
        };
    }

    private hash<DataProviderFactoryInfo> getInfoImpl() {
        return FactoryInfo;
    }

    private hash<DataProviderInfo> getProviderInfoImpl() {
        return KatanaDataProvider::ProviderInfo;
    }

    private Class getClassImpl() {
        return cls;
    }
}

} # namespace KatanaDataProvider
