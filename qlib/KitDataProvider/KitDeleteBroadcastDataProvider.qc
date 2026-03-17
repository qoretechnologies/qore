# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitDeleteBroadcastDataProvider.qc Kit delete broadcast action data provider

/*  KitDeleteBroadcastDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KitDataProvider module
public namespace KitDataProvider {

#! Request type for deleting a broadcast
public class KitDeleteBroadcastRequestDataType inherits HashDataType {
    public {
        #! Field definitions for deleting a broadcast
        const Fields = {
            "broadcast_id": {
                "type": StringType,
                "display_name": "Broadcast ID",
                "short_desc": "The broadcast ID to delete",
                "desc": "The unique identifier of the broadcast to delete",
                "required": True,
                "example_value": "12345",
                "ref_data": "broadcasts",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Kit delete broadcast action data provider
public class KitDeleteBroadcastDataProvider inherits KitDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "Delete a broadcast in Kit",
            "type": "KitDeleteBroadcastDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KitRestClient::KitRestClient rest) : KitDataProviderBase(rest) {
    }

    string getName() {
        return "delete";
    }

    *string getDesc() {
        return "Delete a broadcast in Kit";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KitDeleteBroadcastRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new HashDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = remove req.broadcast_id;
        return doDelete("broadcasts/" + encode_url(id, True));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KitDataProvider
