# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoogleDriveDataProvider.qc root data provider and factory for the GoogleDriveDataProvider module

/*  GoogleDriveDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GoogleDriveDataProvider module
public namespace GoogleDriveDataProvider {
#! The root Google Drive data provider
/** Every action is a direct child of this provider, so that action paths registered in the catalog are flat
*/
public class GoogleDriveDataProvider inherits GoogleDriveDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "gdrive",
            "type": "GoogleDriveDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
            "children_can_support_observers": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Child provider map
        const DefaultChildMap = {
            "list-files": Class::forName("GoogleDriveDataProvider::GoogleDriveListFilesDataProvider"),
            "get-file": Class::forName("GoogleDriveDataProvider::GoogleDriveGetFileDataProvider"),
            "download-file": Class::forName("GoogleDriveDataProvider::GoogleDriveDownloadFileDataProvider"),
            "export-file": Class::forName("GoogleDriveDataProvider::GoogleDriveExportFileDataProvider"),
            "upload-file": Class::forName("GoogleDriveDataProvider::GoogleDriveUploadFileDataProvider"),
            "create-text-file": Class::forName("GoogleDriveDataProvider::GoogleDriveCreateTextFileDataProvider"),
            "replace-file": Class::forName("GoogleDriveDataProvider::GoogleDriveReplaceFileDataProvider"),
            "create-folder": Class::forName("GoogleDriveDataProvider::GoogleDriveCreateFolderDataProvider"),
            "copy-file": Class::forName("GoogleDriveDataProvider::GoogleDriveCopyFileDataProvider"),
            "move-file": Class::forName("GoogleDriveDataProvider::GoogleDriveMoveFileDataProvider"),
            "rename-file": Class::forName("GoogleDriveDataProvider::GoogleDriveRenameFileDataProvider"),
            "delete-file": Class::forName("GoogleDriveDataProvider::GoogleDriveDeleteFileDataProvider"),
            "share-file": Class::forName("GoogleDriveDataProvider::GoogleDriveShareFileDataProvider"),
            "events": Class::forName("GoogleDriveDataProvider::GoogleDriveEventsDataProvider"),
        };
    }

    private {
        #! The child provider map, which external modules may extend
        static hash<string, Reflection::Class> childMap = DefaultChildMap;
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Registers a new child data provider
    /** @param pathname the child name
        @param cls the child class

        @throw GOOGLE-DRIVE-CHILD-ERROR the child name is already registered
    */
    static registerChild(string pathname, Reflection::Class cls) {
        if (childMap{pathname}) {
            throw "GOOGLE-DRIVE-CHILD-ERROR", sprintf("child %y already registered", pathname);
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

#! The Google Drive data provider factory
public class GoogleDriveDataProviderFactory inherits AbstractDataProviderFactory {
    private {
        #! Data provider type info
        static Class cls = new Class("GoogleDriveDataProvider");

        #! Factory info
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "gdrive",
            "desc": "Google Drive data provider factory",
            "children_can_support_apis": True,
        };
    }

    #! Returns static factory information without \a provider_info
    private hash<DataProviderFactoryInfo> getInfoImpl() {
        return FactoryInfo;
    }

    #! Returns static provider information
    private hash<DataProviderInfo> getProviderInfoImpl() {
        return GoogleDriveDataProvider::ProviderInfo;
    }

    #! Returns the class for the data provider object
    private Class getClassImpl() {
        return cls;
    }
}
}
