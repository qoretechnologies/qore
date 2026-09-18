# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AssemblyAiDataProvider API action manifest

/** AssemblyAiDictationManifest.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Curated actions and resource selections for the dictation API
public class AssemblyAiDictationManifest {
    public {
        #! Resource listings used by action dropdowns
        const hash<string, hash<RestSchemaReferenceDataInfo>> ReferenceData = {

        };

        #! Stable action contracts resolved against the vendored API description
        const list<hash<RestSchemaActionInfo>> Manifest = (
            <RestSchemaActionInfo>{
                "action": "transcribe-dictation",
                "method": "POST",
                "path": "/v1/transcribe/live",
                "operation_id": "transcribeDictation",
                "display_name": "Transcribe Dictation",
                "short_desc": "Turn a short spoken utterance into text ready to send",
                "desc": "Submit up to 120 seconds of WAV or PCM audio. Returns verbatim `text` and cleaned-up `llm_response`. If cleanup fails, use `text` and inspect `llm_error`. Configuration is sent before audio.",
                "groups": ("Dictation",),
                "fields": {
                    "audio": <RestSchemaFieldOverlayInfo>{"display_name": "Audio", "desc": "Audio bytes in WAV format, or raw 16-bit little-endian PCM with sample rate and channel configuration."},
                    "config": <RestSchemaFieldOverlayInfo>{"default_value": {}, "preselected": True},
                },
            },
        );
    }
}
}
