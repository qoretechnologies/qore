# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SharePointDataProvider module definition

/** SharePointDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#! contains all public definitions in the SharePointDataProvider module
public namespace SharePointDataProvider {
#! The SharePoint data provider class
public class SharePointDataProvider inherits SharePointDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "sharepoint",
            "type": "SharePointDataProvider",
            "supports_children": True,
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_observers": True,
        };

        #! Application name for action catalog
        const AppName = "SharePoint";
    }

    private {
        #! Dynamic child data providers
        static hash<string, Reflection::Class> childMap = DefaultChildMap;

        #! Child data providers
        const DefaultChildMap = {
            # Site and library navigation (DPAT_FIND)
            "sites": Class::forName("SharePointDataProvider::SharePointSitesDataProvider"),
            "drives": Class::forName("SharePointDataProvider::SharePointDrivesDataProvider"),
            "lists": Class::forName("SharePointDataProvider::SharePointListsDataProvider"),
            "columns": Class::forName("SharePointDataProvider::SharePointColumnsDataProvider"),
            # Document library items (DPAT_FIND)
            "items": Class::forName("SharePointDataProvider::SharePointDriveItemsDataProvider"),
            # List items (DPAT_FIND)
            "list-items": Class::forName("SharePointDataProvider::SharePointListItemsDataProvider"),
            # Site actions
            "get-site": Class::forName("SharePointDataProvider::SharePointGetSiteDataProvider"),
            # Document library actions
            "get-drive-info": Class::forName("SharePointDataProvider::SharePointGetDriveInfoDataProvider"),
            "get-item": Class::forName("SharePointDataProvider::SharePointGetItemDataProvider"),
            "create-folder": Class::forName("SharePointDataProvider::SharePointCreateFolderDataProvider"),
            "upload-file": Class::forName("SharePointDataProvider::SharePointUploadFileDataProvider"),
            "create-upload-session":
                Class::forName("SharePointDataProvider::SharePointCreateUploadSessionDataProvider"),
            "download-file": Class::forName("SharePointDataProvider::SharePointDownloadFileDataProvider"),
            "delete-item": Class::forName("SharePointDataProvider::SharePointDeleteItemDataProvider"),
            "update-item": Class::forName("SharePointDataProvider::SharePointUpdateItemDataProvider"),
            "copy-item": Class::forName("SharePointDataProvider::SharePointCopyItemDataProvider"),
            "create-sharing-link":
                Class::forName("SharePointDataProvider::SharePointCreateSharingLinkDataProvider"),
            # List actions
            "get-list": Class::forName("SharePointDataProvider::SharePointGetListDataProvider"),
            "create-list": Class::forName("SharePointDataProvider::SharePointCreateListDataProvider"),
            "delete-list": Class::forName("SharePointDataProvider::SharePointDeleteListDataProvider"),
            # List item actions
            "get-list-item": Class::forName("SharePointDataProvider::SharePointGetListItemDataProvider"),
            "create-list-item":
                Class::forName("SharePointDataProvider::SharePointCreateListItemDataProvider"),
            "update-list-item":
                Class::forName("SharePointDataProvider::SharePointUpdateListItemDataProvider"),
            "delete-list-item":
                Class::forName("SharePointDataProvider::SharePointDeleteListItemDataProvider"),
            # Records
            "records": Class::forName("SharePointDataProvider::SharePointRecordsDataProvider"),
            # Events
            "events": Class::forName("SharePointDataProvider::SharePointEventsDataProvider"),
        };
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : SharePointDataProviderBase(options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Register a new child data provider
    static registerChild(string pathname, Reflection::Class cls) {
        if (childMap{pathname}) {
            throw "SHAREPOINT-CHILD-ERROR", sprintf("child %y already registered", pathname);
        }
        childMap{pathname} = cls;
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), childMap.iterator();
    }

    #! Returns a list of child data provider names, if any
    private *list<string> getChildProviderNamesImpl() {
        return keys childMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = childMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest, {
            "site_id": default_site_id,
            "drive_id": default_drive_id,
        });
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
