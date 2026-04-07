# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantSnapshotsListCollectionDataProvider class definition

/** QdrantSnapshotsListCollectionDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the QdrantDataProvider module
public namespace QdrantDataProvider {
#! The Qdrant list collection snapshots API data provider
/** This API lists all snapshots of a specific collection

    @note Corresponds to: GET /collections/{name}/snapshots
*/
public class QdrantSnapshotsListCollectionDataProvider inherits QdrantDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-collection",
            "desc": "Qdrant list collection snapshots API data provider (`GET /collections/{name}/snapshots`)",
            "type": "QdrantSnapshotsListCollectionDataProvider",
            "constructor_options": QdrantDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new QdrantSnapshotsListCollectionRequestDataType();

        #! Response type - list of snapshot objects (Qdrant returns result as a list directly)
        const ResponseType = new ListDataType("QdrantSnapshotsListCollectionResponse",
            new QdrantSnapshotElementDataType(), True);
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", QdrantDataProvider::ConstructorOptions, options);
        rest = QdrantDataProvider::getRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(RestClientIo::RestClientIo rest) : QdrantDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Qdrant snapshots %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doQdrantGet(collectionPath(remove req.collection, "/snapshots"));
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Qdrant list collection snapshots request data type
public class QdrantSnapshotsListCollectionRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "collection": {
                "type": StringType,
                "display_name": "Collection",
                "short_desc": "Collection name",
                "desc": "The name of the collection to list snapshots for",
                "example_value": "my_collection",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Qdrant snapshot element data type for snapshot list responses
public class QdrantSnapshotElementDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Snapshot file name",
                "desc": "The name of the snapshot file",
                "example_value": "my_collection-snapshot.snapshot",
            },
            "creation_time": {
                "type": StringOrNothingType,
                "display_name": "Creation Time",
                "short_desc": "Snapshot creation time",
                "desc": "The date/time when the snapshot was created",
                "example_value": "2026-01-01T00:00:00",
            },
            "size": {
                "type": IntOrNothingType,
                "display_name": "Size",
                "short_desc": "Snapshot size in bytes",
                "desc": "The size of the snapshot file in bytes",
                "example_value": 1048576,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."auto");
    }
}
}
