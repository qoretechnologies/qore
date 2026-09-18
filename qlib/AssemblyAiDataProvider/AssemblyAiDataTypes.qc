# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AssemblyAiDataProvider root class definition

/** AssemblyAiDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace AssemblyAiDataProvider {
#! Full transcription request contract with local polling controls
public class AssemblyAiTranscribeRequestDataType inherits HashDataType {
    public {
        #! Local controls never sent to AssemblyAI
        const PollingFields = {
            "poll_interval": {
                "display_name": "Polling Interval",
                "short_desc": "Time between transcript status checks",
                "desc": "Positive interval between status checks. Defaults to three seconds.",
                "type": AbstractDataProviderTypeMap."date",
                "default_value": AssemblyAiRestClientBase::DefaultPollInterval,
            },
            "transcription_timeout": {
                "display_name": "Overall Transcription Timeout",
                "short_desc": "Maximum time to wait for the transcript",
                "desc": "Positive polling time budget. Defaults to 30 minutes. A timeout leaves the server job running; use its ID to retrieve or delete it.",
                "type": AbstractDataProviderTypeMap."date",
                "default_value": AssemblyAiRestClientBase::DefaultTranscriptionTimeout,
            },
        };
        #! Schema fields and polling controls, preserving nested type metadata
        static hash<string, AbstractDataField> Fields;
    }
    private {
        static Mutex field_lock();
    }
    #! Constructs the complete transcription request type
    constructor() : HashDataType("AssemblyAI Transcription Request",
            AssemblyAiTranscribeRequestDataType::makeFields()) {
    }
    private static hash<string, AbstractDataField> makeFields() {
        AutoLock al(field_lock);
        if (Fields) {
            return Fields;
        }
        HashDataType type();
        map type.addField($1), AssemblyAiSchema::getOperationEx("create-transcript").getRequestType()
            .getFields().iterator();
        type.addQoreFields(PollingFields);
        return Fields = type.getFields();
    }
}

#! Completed transcript with recursively typed words, speakers, and analysis
public class AssemblyAiTranscribeResponseDataType inherits HashDataType {
    private {
        #! Full transcript response fields from the committed schema
        static hash<string, AbstractDataField> Fields;
        static Mutex field_lock();
    }
    #! Constructs the transcript response type
    constructor() : HashDataType("AssemblyAI Transcript", AssemblyAiTranscribeResponseDataType::makeFields()) {
    }
    private static hash<string, AbstractDataField> makeFields() {
        AutoLock al(field_lock);
        if (!Fields) {
            Fields = AssemblyAiSchema::getOperationEx("get-transcript").getResponseType().getFields();
        }
        return Fields;
    }
}
}
