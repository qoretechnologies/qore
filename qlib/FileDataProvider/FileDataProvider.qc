# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FileDataProvider class definition

/** FileDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the FileDataProvider module
public namespace FileDataProvider {
#! Generic app name for the server filesystem
public const AppName = "Filesystem";

#! The main data provider class, provides API classes as children
public class FileDataProvider inherits FileDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "FileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": False,
            "children_can_support_observers": False,
        };
    }

    private {
        const ChildMap = {
            "copy": Class::forName("FileDataProvider::FileCopyDataProvider"),
            "create": Class::forName("FileDataProvider::FileCreateDataProvider"),
            "delete": Class::forName("FileDataProvider::FileDeleteDataProvider"),
            "get": Class::forName("FileDataProvider::FileGetDataProvider"),
            "list": Class::forName("FileDataProvider::FileListDataProvider"),
            "mkdir": Class::forName("FileDataProvider::FileMkdirDataProvider"),
            "move": Class::forName("FileDataProvider::FileMoveDataProvider"),
            "stat": Class::forName("FileDataProvider::FileStatDataProvider"),
        };
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : FileDataProviderBase(options) {
    }

    #! Returns the data provider name
    string getName() {
        return "file";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Data provider for the server filesystem";
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @throw CHILD-PROVIDER-ERROR error acquiring child provider

        @see getChildProviderEx()
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(root, lock, readonly);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
