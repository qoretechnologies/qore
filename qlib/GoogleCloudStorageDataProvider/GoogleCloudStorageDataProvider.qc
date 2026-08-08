# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GoogleCloudStorageDataProvider module definition

/** GoogleCloudStorageDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GoogleCloudStorageDataProvider module
public namespace GoogleCloudStorageDataProvider {
#! The Google Cloud Storage data provider class
public class GoogleCloudStorageDataProvider inherits GoogleCloudStorageDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "gcs",
            "type": "GoogleCloudStorageDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };
    }

    private {
        #! Dynamic child data providers
        static hash<string, Reflection::Class> childMap = DefaultChildMap;

        #! Child data providers
        const DefaultChildMap = {
            # Bucket actions
            "list-buckets": Class::forName("GoogleCloudStorageDataProvider::GoogleCloudStorageListBucketsDataProvider"),
            "get-bucket": Class::forName("GoogleCloudStorageDataProvider::GoogleCloudStorageGetBucketDataProvider"),
            # Object actions
            "list-objects": Class::forName("GoogleCloudStorageDataProvider::GoogleCloudStorageListObjectsDataProvider"),
            "get-object-metadata":
                Class::forName("GoogleCloudStorageDataProvider::GoogleCloudStorageGetObjectMetadataDataProvider"),
            "update-object-metadata":
                Class::forName("GoogleCloudStorageDataProvider::GoogleCloudStorageUpdateObjectMetadataDataProvider"),
            "upload-object": Class::forName("GoogleCloudStorageDataProvider::GoogleCloudStorageUploadObjectDataProvider"),
            "download-object":
                Class::forName("GoogleCloudStorageDataProvider::GoogleCloudStorageDownloadObjectDataProvider"),
            "delete-object": Class::forName("GoogleCloudStorageDataProvider::GoogleCloudStorageDeleteObjectDataProvider"),
            "copy-object": Class::forName("GoogleCloudStorageDataProvider::GoogleCloudStorageCopyObjectDataProvider"),
            "compose": Class::forName("GoogleCloudStorageDataProvider::GoogleCloudStorageComposeDataProvider"),
            # Sharing
            "create-signed-url":
                Class::forName("GoogleCloudStorageDataProvider::GoogleCloudStorageCreateSignedUrlDataProvider"),
        };
    }

    #! Creates the object from a REST client and a credential context
    constructor(GoogleCloudStorageRestClient::GoogleCloudStorageRestClientIo rest, hash<GcsAuthContext> auth)
            : GoogleCloudStorageDataProviderBase(rest, auth) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : GoogleCloudStorageDataProviderBase(options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Register a new child data provider
    static registerChild(string pathname, Reflection::Class cls) {
        if (childMap{pathname}) {
            throw "GCS-CHILD-ERROR", sprintf("child %y already registered", pathname);
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
        # the credential travels with the client: a GcpAuth token is minted per request, so a child
        # holding only the client could not authenticate
        return cls.newObject(rest, auth);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
