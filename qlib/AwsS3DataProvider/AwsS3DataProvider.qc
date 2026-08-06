# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AwsS3DataProvider.qc root data provider for the AwsS3DataProvider module

/*  AwsS3DataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AwsS3DataProvider module
public namespace AwsS3DataProvider {
#! The root Amazon S3 data provider
/** Every action is a direct child of this provider, so that action paths registered in the catalog are flat
    (\c "/get-object" rather than \c "/objects/get")
*/
public class AwsS3DataProvider inherits AwsS3DataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "s3",
            "type": "AwsS3DataProvider",
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
            "list-buckets": Class::forName("AwsS3DataProvider::AwsS3ListBucketsDataProvider"),
            "create-bucket": Class::forName("AwsS3DataProvider::AwsS3CreateBucketDataProvider"),
            "list-objects": Class::forName("AwsS3DataProvider::AwsS3ListObjectsDataProvider"),
            "get-object": Class::forName("AwsS3DataProvider::AwsS3GetObjectDataProvider"),
            "download-file": Class::forName("AwsS3DataProvider::AwsS3DownloadFileDataProvider"),
            "upload-file": Class::forName("AwsS3DataProvider::AwsS3UploadFileDataProvider"),
            "create-text-file": Class::forName("AwsS3DataProvider::AwsS3CreateTextFileDataProvider"),
            "delete-object": Class::forName("AwsS3DataProvider::AwsS3DeleteObjectDataProvider"),
            "copy-object": Class::forName("AwsS3DataProvider::AwsS3CopyObjectDataProvider"),
        };
    }

    private {
        #! The child provider map, which external modules may extend
        static hash<string, Reflection::Class> childMap = DefaultChildMap;
    }

    #! Creates the object from an AwsRestClient
    constructor(AwsRestClient::AwsRestClient rest, bool path_style = False)
            : AwsS3DataProviderBase(rest, path_style) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : AwsS3DataProviderBase(options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Registers a new child data provider
    /** @param pathname the child name
        @param cls the child class

        @throw S3-CHILD-ERROR the child name is already registered
    */
    static registerChild(string pathname, Reflection::Class cls) {
        if (childMap{pathname}) {
            throw "S3-CHILD-ERROR", sprintf("child %y already registered", pathname);
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
        return cls.newObject(rest, path_style);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
