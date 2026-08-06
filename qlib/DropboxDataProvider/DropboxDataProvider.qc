# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DropboxDataProvider.qc root data provider and factory for the DropboxDataProvider module

/*  DropboxDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DropboxDataProvider module
public namespace DropboxDataProvider {
#! The root Dropbox data provider
/** Every action is a direct child of this provider, so that action paths registered in the catalog are flat
*/
public class DropboxDataProvider inherits DropboxDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "dropbox",
            "type": "DropboxDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Child provider map
        const DefaultChildMap = {
            "list-folder": Class::forName("DropboxDataProvider::DropboxListFolderDataProvider"),
            "search": Class::forName("DropboxDataProvider::DropboxSearchDataProvider"),
            "get-metadata": Class::forName("DropboxDataProvider::DropboxGetMetadataDataProvider"),
            "download-file": Class::forName("DropboxDataProvider::DropboxDownloadFileDataProvider"),
            "upload-file": Class::forName("DropboxDataProvider::DropboxUploadFileDataProvider"),
            "create-text-file": Class::forName("DropboxDataProvider::DropboxCreateTextFileDataProvider"),
            "get-file-link": Class::forName("DropboxDataProvider::DropboxGetFileLinkDataProvider"),
            "create-folder": Class::forName("DropboxDataProvider::DropboxCreateFolderDataProvider"),
            "delete-file": Class::forName("DropboxDataProvider::DropboxDeleteFileDataProvider"),
            "copy-file": Class::forName("DropboxDataProvider::DropboxCopyFileDataProvider"),
            "move-file": Class::forName("DropboxDataProvider::DropboxMoveFileDataProvider"),
            "list-file-revisions": Class::forName("DropboxDataProvider::DropboxListFileRevisionsDataProvider"),
            "restore-file-revision":
                Class::forName("DropboxDataProvider::DropboxRestoreFileRevisionDataProvider"),
            "create-shared-link": Class::forName("DropboxDataProvider::DropboxCreateSharedLinkDataProvider"),
            "list-shared-links": Class::forName("DropboxDataProvider::DropboxListSharedLinksDataProvider"),
            "revoke-shared-link": Class::forName("DropboxDataProvider::DropboxRevokeSharedLinkDataProvider"),
            "get-account-info": Class::forName("DropboxDataProvider::DropboxGetAccountInfoDataProvider"),
            "get-space-usage": Class::forName("DropboxDataProvider::DropboxGetSpaceUsageDataProvider"),
        };
    }

    private {
        #! The child provider map, which external modules may extend
        static hash<string, Reflection::Class> childMap = DefaultChildMap;
    }

    #! Creates the object from a DropboxRestClientIo client
    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Registers a new child data provider
    /** @param pathname the child name
        @param cls the child class

        @throw DROPBOX-CHILD-ERROR the child name is already registered
    */
    static registerChild(string pathname, Reflection::Class cls) {
        if (childMap{pathname}) {
            throw "DROPBOX-CHILD-ERROR", sprintf("child %y already registered", pathname);
        }
        childMap{pathname} = cls;
    }

    #! Returns data provider summary info for all children
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), childMap.iterator();
    }

    #! Returns a list of child data provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys childMap;
    }

    #! Returns the given child provider or @ref nothing if the child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = childMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! The Dropbox data provider factory
public class DropboxDataProviderFactory inherits AbstractDataProviderFactory {
    private {
        #! Data provider type info
        static Class cls = new Class("DropboxDataProvider");

        #! Factory info
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "dropbox",
            "desc": "Dropbox data provider factory",
            "children_can_support_apis": True,
        };
    }

    #! Returns static factory information without \a provider_info
    private hash<DataProviderFactoryInfo> getInfoImpl() {
        return FactoryInfo;
    }

    #! Returns static provider information
    private hash<DataProviderInfo> getProviderInfoImpl() {
        return DropboxDataProvider::ProviderInfo;
    }

    #! Returns the class for the data provider object
    private Class getClassImpl() {
        return cls;
    }
}
}
