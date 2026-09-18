# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AssemblyAiDataProvider API action manifest

/** AssemblyAiSyncApiManifest.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Curated actions and resource selections for the sync-api API
public class AssemblyAiSyncApiManifest {
    public {
        #! Resource listings used by action dropdowns
        const hash<string, hash<RestSchemaReferenceDataInfo>> ReferenceData = {

        };

        #! Stable action contracts resolved against the vendored API description
        const list<hash<RestSchemaActionInfo>> Manifest = (
            <RestSchemaActionInfo>{
                "action": "transcribe-short-audio",
                "method": "POST",
                "path": "/transcribe",
                "operation_id": "transcribeSync",
                "display_name": "Transcribe Short Audio",
                "short_desc": "Transcribe a short WAV or PCM recording in one request",
                "desc": "Submit 80 milliseconds to 120 seconds of audio and receive a completed transcript. For raw PCM, provide sample rate and channels in `config`.",
                "groups": ("Transcription",),
                "fields": {
                    "X-AAI-Model": <RestSchemaFieldOverlayInfo>{"name": "model", "display_name": "Speech Model", "default_value": "universal-3-5-pro"},
                    "token": <RestSchemaFieldOverlayInfo>{"ignore": True},
                    "audio": <RestSchemaFieldOverlayInfo>{"display_name": "Audio", "desc": "Audio bytes in WAV format, or raw 16-bit little-endian PCM with sample rate and channel configuration."},
                    "config": <RestSchemaFieldOverlayInfo>{"fields": {
                        "conversation_context": <RestSchemaFieldOverlayInfo>{"desc": "Supply preceding dialogue to improve continuity across conversation turns. Accepts a list ordered oldest to newest, or one string treated as a single turn. Limits are 500 turns and 16,000 characters total; excess context is trimmed from the oldest turns, including when the model's context window is exceeded."},
                    }},
                },
            },
            <RestSchemaActionInfo>{
                "action": "transcribe-live-upload",
                "method": "POST",
                "path": "/v1/transcribe/live",
                "operation_id": "transcribeLiveSync",
                "display_name": "Transcribe Audio Upload",
                "short_desc": "Send a short recording to the live-upload transcription endpoint",
                "desc": "Transcribe 80 milliseconds to 120 seconds of audio. This action sends a finite recording and returns one completed transcript. Use the native client for audio produced during the upload. Configuration is sent before audio.",
                "groups": ("Transcription",),
                "fields": {
                    "X-AAI-Model": <RestSchemaFieldOverlayInfo>{"name": "model", "display_name": "Speech Model", "default_value": "universal-3-5-pro"},
                    "token": <RestSchemaFieldOverlayInfo>{"ignore": True},
                    "audio": <RestSchemaFieldOverlayInfo>{"display_name": "Audio", "desc": "Audio bytes in WAV format, or raw 16-bit little-endian PCM with sample rate and channel configuration."},
                    "config": <RestSchemaFieldOverlayInfo>{"default_value": {}, "preselected": True},
                },
            },
        );
    }
}
}
