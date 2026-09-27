# -*- mode: qore; indent-tabs-mode: nil -*-
#
# Qore Programming Language
#
# Copyright 2003 - 2026 Qore Technologies, s.r.o.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

public namespace YamlDocumentDataProvider {

#! Factory for creating YAML document write data providers
public class YamlDocumentWriteDataProviderFactory inherits DataProvider::AbstractDataProviderFactory {
    public {
        #! Factory metadata
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "yamlwrite",
            "desc": "Data provider factory for writing YAML document streams",
            "children_can_support_observers": False,
        };
    }

    #! Returns factory info
    hash<DataProviderFactoryInfo> getInfoImpl() {
        return FactoryInfo;
    }

    #! Returns the provider class
    Class getClassImpl() {
        return Class::forName("YamlDocumentDataProvider::YamlDocumentWriteDataProvider");
    }

    #! Creates a provider instance
    private DataProvider::AbstractDataProvider createImpl(hash<auto> options,
            *LoggerInterface logger) {
        return new YamlDocumentWriteDataProvider(options);
    }

    #! Returns info about the provider
    *hash<auto> getInfoAsDataImpl(hash<auto> constructor_options, *bool with_type_info) {
        return YamlDocumentWriteDataProvider::ProviderInfo;
    }

    #! Returns the description
    string getDescImpl() {
        return FactoryInfo.desc;
    }

    #! Returns provider info
    private hash<DataProvider::DataProviderInfo> getProviderInfoImpl() {
        return YamlDocumentWriteDataProvider::ProviderInfo;
    }
}

} # namespace YamlDocumentDataProvider
