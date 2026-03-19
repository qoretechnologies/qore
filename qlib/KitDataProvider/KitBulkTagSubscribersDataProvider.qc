# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitBulkTagSubscribersDataProvider.qc Kit bulk tag subscribers action data provider

/*  KitBulkTagSubscribersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for bulk tagging subscribers
public class KitBulkTagSubscribersRequestDataType inherits HashDataType {
    public {
        #! Field definitions for bulk tagging subscribers
        const Fields = {
            "tag_id": {
                "type": StringType,
                "display_name": "Tag ID",
                "short_desc": "The tag ID",
                "desc": "The unique identifier of the tag to apply to the subscribers",
                "required": True,
                "example_value": "12345",
                "ref_data": "tags",
            },
            "subscribers": {
                "type": new ListDataType("EmailList", StringType, True),
                "display_name": "Subscribers",
                "short_desc": "List of subscriber email addresses",
                "desc": "A list of subscriber email addresses to tag",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Kit bulk tag subscribers action data provider
public class KitBulkTagSubscribersDataProvider inherits KitDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "bulk-tag",
            "desc": "Bulk tag subscribers in Kit",
            "type": "KitBulkTagSubscribersDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KitRestClient::KitRestClient rest) : KitDataProviderBase(rest) {
    }

    string getName() {
        return "bulk-tag";
    }

    *string getDesc() {
        return "Bulk tag subscribers in Kit";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KitBulkTagSubscribersRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new HashDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Kit API requires tagging each subscriber individually via
        # POST /v4/tags/{tag_id}/subscribers
        string tag_id = remove req.tag_id;
        list<string> emails = remove req.subscribers;
        list<hash<auto>> results = ();
        list<hash<auto>> failures = ();
        foreach string email in (emails) {
            try {
                hash<auto> response = doPost(
                    "tags/" + encode_url(tag_id, True) + "/subscribers",
                    {"email_address": email},
                );
                results += response.subscriber;
            } catch (hash<ExceptionInfo> ex) {
                failures += {"email_address": email, "error": ex.desc};
            }
        }
        return {"subscribers": results, "failures": failures};
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KitDataProvider
