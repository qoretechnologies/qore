# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BitlyDeleteBitlinkDataProvider.qc Bitly delete bitlink action data provider

/*  BitlyDeleteBitlinkDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BitlyDataProvider module
public namespace BitlyDataProvider {

#! Request type for deleting a bitlink
public class BitlyDeleteBitlinkRequestDataType inherits HashDataType {
    public {
        #! Field definitions for deleting a bitlink
        const Fields = {
            "bitlink": {
                "type": StringType,
                "display_name": "Bitlink",
                "short_desc": "The bitlink to delete",
                "desc": "The bitlink ID to delete (e.g., `bit.ly/abc123`)",
                "required": True,
                "ref_data": "bitlinks",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for delete operation
public class BitlyDeleteBitlinkResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "deleted": {
                "type": BoolType,
                "display_name": "Deleted",
                "short_desc": "Whether the deletion was successful",
                "desc": "Returns `True` if the bitlink was successfully deleted",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Bitly delete bitlink action data provider
public class BitlyDeleteBitlinkDataProvider inherits BitlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "Delete an existing bitlink",
            "type": "BitlyDeleteBitlinkDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static BitlyDeleteBitlinkResponseDataType ResponseType();
    }

    constructor(RestClientIo::RestClientIo rest) : BitlyDataProviderBase(rest) {
    }

    string getName() {
        return "delete";
    }

    *string getDesc() {
        return "Delete an existing bitlink";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BitlyDeleteBitlinkRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete("bitlinks/" + encodeBitlink(req.bitlink));
        return {"deleted": True};
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BitlyDataProvider
