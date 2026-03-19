# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoMessagesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CustomerIoDataProvider module
public namespace CustomerIoDataProvider {

#! Customer.io messages collection data provider
public class CustomerIoMessagesDataProvider inherits CustomerIoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "messages",
            "desc": "Customer.io messages data provider - list message deliveries",
            "type": "CustomerIoMessagesDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "metric": <DataProviderOptionInfo>{
                "display_name": "Metric",
                "short_desc": "Filter by delivery metric type",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter messages by delivery metric type (e.g., `email_sent`, `email_delivered`)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "email_sent", "desc": "Email sent"},
                    <AllowedValueInfo>{"value": "email_delivered", "desc": "Email delivered"},
                    <AllowedValueInfo>{"value": "email_opened", "desc": "Email opened"},
                    <AllowedValueInfo>{"value": "email_clicked", "desc": "Email clicked"},
                    <AllowedValueInfo>{"value": "email_bounced", "desc": "Email bounced"},
                    <AllowedValueInfo>{"value": "email_dropped", "desc": "Email dropped"},
                    <AllowedValueInfo>{"value": "email_spammed", "desc": "Email marked as spam"},
                    <AllowedValueInfo>{"value": "email_unsubscribed", "desc": "Email unsubscribed"},
                    <AllowedValueInfo>{"value": "sms_sent", "desc": "SMS sent"},
                    <AllowedValueInfo>{"value": "sms_delivered", "desc": "SMS delivered"},
                    <AllowedValueInfo>{"value": "sms_failed", "desc": "SMS failed"},
                    <AllowedValueInfo>{"value": "push_sent", "desc": "Push sent"},
                    <AllowedValueInfo>{"value": "push_delivered", "desc": "Push delivered"},
                    <AllowedValueInfo>{"value": "push_opened", "desc": "Push opened"},
                    <AllowedValueInfo>{"value": "push_failed", "desc": "Push failed"},
                ),
            },
            "start": <DataProviderOptionInfo>{
                "display_name": "Start Timestamp",
                "short_desc": "Filter from this Unix timestamp",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter messages sent on or after this Unix timestamp",
            },
            "end": <DataProviderOptionInfo>{
                "display_name": "End Timestamp",
                "short_desc": "Filter until this Unix timestamp",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter messages sent on or before this Unix timestamp",
            },
            "page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Page number",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Page number for pagination (starting at 1)",
            },
            "page_size": <DataProviderOptionInfo>{
                "display_name": "Page Size",
                "short_desc": "Results per page",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Number of results per page",
            },
        };
    }

    private {
        #! Record type
        static CustomerIoMessageDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(CustomerIoRestClient::CustomerIoRestClient rest)
            : CustomerIoDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(CustomerIoRestClient::CustomerIoRestConnection conn)
            : CustomerIoDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "messages";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Customer.io messages data provider - list message deliveries";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for messages
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;

        # Build query parameters from search options
        if (search_options.metric) {
            params.metric = search_options.metric;
        }
        if (search_options.start) {
            params.start = search_options.start;
        }
        if (search_options.end) {
            params.end = search_options.end;
        }
        if (search_options.page) {
            params.page = search_options.page;
        }
        if (search_options.page_size) {
            params.page_size = search_options.page_size;
        } else {
            params.page_size = DefaultPageSize;
        }

        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: params=%y", params));

        auto response = doGet("/messages", params);
        list<hash<auto>> records = response.messages ?? ();

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CustomerIoDataProvider
