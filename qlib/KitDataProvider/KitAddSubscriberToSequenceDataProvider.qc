# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitAddSubscriberToSequenceDataProvider.qc Kit add subscriber to sequence action data provider

/*  KitAddSubscriberToSequenceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for adding a subscriber to a sequence
public class KitAddSubscriberToSequenceRequestDataType inherits HashDataType {
    public {
        #! Field definitions for adding a subscriber to a sequence
        const Fields = {
            "sequence_id": {
                "type": StringType,
                "display_name": "Sequence ID",
                "short_desc": "The sequence ID",
                "desc": "The unique identifier of the sequence to add the subscriber to",
                "required": True,
                "example_value": "12345",
                "ref_data": "sequences",
            },
            "email_address": {
                "type": StringType,
                "display_name": "Email Address",
                "short_desc": "The subscriber's email address",
                "desc": "The email address of the subscriber to add to the sequence",
                "required": True,
                "example_value": "user@example.com",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Kit add subscriber to sequence action data provider
public class KitAddSubscriberToSequenceDataProvider inherits KitDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "add-subscriber",
            "desc": "Add a subscriber to a sequence in Kit",
            "type": "KitAddSubscriberToSequenceDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KitRestClient::KitRestClient rest) : KitDataProviderBase(rest) {
    }

    string getName() {
        return "add-subscriber";
    }

    *string getDesc() {
        return "Add a subscriber to a sequence in Kit";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KitAddSubscriberToSequenceRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KitSubscriberDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string sequence_id = remove req.sequence_id;
        hash<auto> response = doPost("sequences/" + encode_url(sequence_id, True) + "/subscribers", req);
        return response.subscriber;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KitDataProvider
