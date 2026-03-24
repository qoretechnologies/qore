# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AnthropicDataProvider module definition

/** AnthropicBatchDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AnthropicDataProvider module
public namespace AnthropicDataProvider {
#! Request counts data type for batch status
public class AnthropicBatchRequestCountsDataType inherits HashDataType {
    public {
        const Fields = {
            "processing": {
                "display_name": "Processing",
                "short_desc": "Number of requests still processing",
                "desc": "Number of requests in the batch that are still being processed",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "succeeded": {
                "display_name": "Succeeded",
                "short_desc": "Number of requests that succeeded",
                "desc": "Number of requests in the batch that have completed successfully",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "errored": {
                "display_name": "Errored",
                "short_desc": "Number of requests that errored",
                "desc": "Number of requests in the batch that encountered an error",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "canceled": {
                "display_name": "Canceled",
                "short_desc": "Number of requests that were canceled",
                "desc": "Number of requests in the batch that were canceled",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "expired": {
                "display_name": "Expired",
                "short_desc": "Number of requests that expired",
                "desc": "Number of requests in the batch that expired before processing",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    constructor() { addQoreFields(Fields); }
    constructor(Type base_type) : HashDataType(base_type) { addQoreFields(Fields); }
}

#! Batch request item data type
public class AnthropicBatchRequestItemDataType inherits HashDataType {
    public {
        const Fields = {
            "custom_id": {
                "display_name": "Custom ID",
                "short_desc": "Developer-provided ID for matching results",
                "desc": "Developer-provided ID for matching results to requests. Must be unique within the batch. "
                    "Max 64 characters, must match `^[a-zA-Z0-9_-]+$`",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "request-001",
            },
            "params": {
                "display_name": "Parameters",
                "short_desc": "Message creation parameters",
                "desc": "The parameters for the message creation request. Same structure as POST `/v1/messages`",
                "type": AbstractDataProviderTypeMap."hash",
                "required": True,
            },
        };
    }

    constructor() { addQoreFields(Fields); }
}

#! Create batch request data type
public class AnthropicCreateBatchRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "requests": {
                "display_name": "Requests",
                "short_desc": "List of message requests to process in batch",
                "desc": "List of requests to process. Each item has a `custom_id` for result matching and "
                    "`params` containing the message creation parameters",
                "type": new ListDataType("AnthropicBatchRequestList", new AnthropicBatchRequestItemDataType(), True),
                "required": True,
            },
        };
    }

    constructor() { addQoreFields(Fields); }
}

public const AnthropicCreateBatchRequestDataType = new AnthropicCreateBatchRequestDataType();

#! Batch response data type
public class AnthropicBatchResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Batch ID",
                "short_desc": "Unique batch identifier",
                "desc": "Unique identifier for the batch. Format: `msgbatch_` followed by alphanumeric characters",
                "type": AbstractDataProviderTypeMap."string",
            },
            "type": {
                "display_name": "Object Type",
                "short_desc": "Object type",
                "desc": "Object type, always `message_batch`",
                "type": AbstractDataProviderTypeMap."string",
            },
            "processing_status": {
                "display_name": "Processing Status",
                "short_desc": "The processing status of the batch",
                "desc": "The processing status of the batch",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "in_progress", "display_name": "In Progress"},
                    <AllowedValueInfo>{"value": "canceling", "display_name": "Canceling"},
                    <AllowedValueInfo>{"value": "ended", "display_name": "Ended"},
                ),
            },
            "request_counts": {
                "display_name": "Request Counts",
                "short_desc": "Counts of requests by status",
                "desc": "Tallies of requests by their status within the batch",
                "type": new AnthropicBatchRequestCountsDataType(AutoHashOrNothingType),
            },
            "ended_at": {
                "display_name": "Ended At",
                "short_desc": "When the batch ended processing",
                "desc": "RFC 3339 datetime when the batch finished processing",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "created_at": {
                "display_name": "Created At",
                "short_desc": "When the batch was created",
                "desc": "RFC 3339 datetime when the batch was created",
                "type": AbstractDataProviderTypeMap."string",
            },
            "expires_at": {
                "display_name": "Expires At",
                "short_desc": "When the batch expires",
                "desc": "RFC 3339 datetime when the batch will expire (24 hours after creation)",
                "type": AbstractDataProviderTypeMap."string",
            },
            "cancel_initiated_at": {
                "display_name": "Cancel Initiated At",
                "short_desc": "When cancellation was initiated",
                "desc": "RFC 3339 datetime when cancellation was initiated, if applicable",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "results_url": {
                "display_name": "Results URL",
                "short_desc": "URL to retrieve batch results",
                "desc": "URL to retrieve the JSONL results file. Available when the batch has ended",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() { addQoreFields(Fields); }
    constructor(Type base_type) : HashDataType(base_type) { addQoreFields(Fields); }
}

public const AnthropicBatchResponseDataType = new AnthropicBatchResponseDataType();
}
